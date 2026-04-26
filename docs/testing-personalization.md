# Personalization & Learning — Live Verification

End-to-end manual test matrix for the corpus collector (Session 15), style
profile extractor (Session 16), and dynamic system prompt assembly
(Session 17).

The unit-test suite (`ninja -C build test`) covers the algorithmic surface;
this document covers the things only a real fcitx5 session can verify:
that key events, lifecycle calls, the inference HTTP path, and the file
system all interact correctly.

## Environment

- OS: CachyOS, KDE Plasma on Wayland
- Fcitx5: 5.1.19
- Engine: `libxtype-fcitx5.so` (Sessions 15–17.5)
- Model: `qwen2.5:1.5b` via Ollama
- Default corpus path: `~/.local/share/xtype/corpus.txt`
- Default profile path: `~/.local/share/xtype/style_profile.json`

## Pre-flight

### 1. Enable learning (opt-in)

Edit `fcitx5-engine/src/config.h`:
```cpp
struct LearningConfig {
    bool enabled = true;   // <-- flip from false
    ...
};
```

### 2. Build and install

```bash
cd fcitx5-engine
cmake -B build -G Ninja -DBUILD_TESTS=ON -DFCITX_INSTALL_USE_FCITX_SYS_PATHS=ON
ninja -C build
ninja -C build test          # 73 tests should pass before going further
sudo ninja -C build install
```

### 3. Restart fcitx5 and wipe any prior corpus

```bash
fcitx5-remote -r
scripts/xtype-corpus wipe    # if anything lingered from a previous session
```

### 4. (Optional) Enable verbose content logging

**WARNING**: this writes sentence prefixes (up to 60 chars) and the prompt
head (up to 200 chars) into `fcitx5-engine/debug.log`. Don't enable this
when typing anything sensitive.

Two options, either works; env var takes precedence:

```bash
# Option A: env var
echo 'XTYPE_DEBUG_VERBOSE=1' >> ~/.config/environment.d/input-method.conf
# log out / log in to apply

# Option B: sentinel file (no logout needed)
mkdir -p ~/.local/share/xtype
touch ~/.local/share/xtype/.debug_verbose
fcitx5-remote -r
```

Disable: remove the env line / delete the sentinel file, then `fcitx5-remote -r`.

### 5. (Optional) Speed up the refresh timer for testing

Default refresh interval is 5 minutes. For test section G, override:

```bash
echo 'XTYPE_PROFILE_REFRESH_SEC=30' >> ~/.config/environment.d/input-method.conf
# log out / log in
```

Floor is 30 s (clamped). Remove the line and re-login when done testing.

### 6. Open the log watcher in another terminal

```bash
scripts/xtype-watch --tag corpus,profile,prompt,harvest
```

This tails both `debug.log` and `thread.log`, filtering to personalization
events. Lines are prefixed `[dbg]` (engine main thread) or `[inf]`
(inference / prompt-set thread).

---

## Test matrix

Result column legend: `✓` pass, `✗` fail, `-` skipped, `?` ambiguous.

### A. Corpus collection — basic capture

| # | Step | Expected log | Expected file state | Result |
|---|------|--------------|---------------------|--------|
| A1 | Open Kate, type `Hello world this is a test.` | `[harvest] ok len=27` | (no file yet — flush is async) | |
| A2 | Wait ~65 s | `[corpus] flush: 1 entries written, file=28 bytes` | `corpus.txt` contains the line | |
| A3 | `scripts/xtype-corpus stats` | — | `lines: 1  words: 6  bytes: 28` | |
| A4 | Type `Quick second one for the corpus.` then wait ~65 s | `[harvest] ok ...` then `[corpus] flush: 1 entries` | 2 lines | |

### B. Sentence terminators

| # | Step | Expected log | Result |
|---|------|--------------|--------|
| B1 | Type `Hello.` | `[harvest] drop too-short len=5` (period stripped, "Hello" < 12) | |
| B2 | Type `This is a long-enough sentence!` | `[harvest] ok len=30` | |
| B3 | Type `What about questions?` | `[harvest] ok len=20` | |
| B4 | Type `Line one then enter`, press Enter | `[harvest] ok len=18` (Enter triggers harvest) | |

### C. AI suggestions excluded from corpus

| # | Step | Expected | Result |
|---|------|----------|--------|
| C1 | Wipe corpus: `xtype-corpus wipe`, restart fcitx5 | (clean slate) | |
| C2 | Type `The weather today is ` (trailing space, no terminator yet) | no `[harvest]` line yet | |
| C3 | Pause until ghost text appears (~300 ms after last keystroke); without typing more, press **Tab** once to accept the next word | the AI-accepted token gets committed to the document. There must be **no** `[harvest] ok` line for the accepted text. | |
| C4 | Type `nice.` to terminate | `[harvest] ok len=N`; verify N counts only `The weather today is nice` (user-typed chars), excluding the AI token | |
| C5 | (If verbose) verify the `[harvest] verbose content='...'` snippet does NOT contain the AI-accepted token | | |

### D. Regular blocklist

| # | Step | Expected | Result |
|---|------|----------|--------|
| D1 | Open Konsole, type `This should not be captured.` | (blocklisted apps short-circuit before record; **no** `[harvest]` line in the log) | |
| D2 | `xtype-corpus stats` | line count unchanged from before D1 | |

### E. Hard-blocked apps (password managers)

If you don't have KeePassXC installed, skip; coverage is provided by the
unit test `isHardBlocked matches password managers case-insensitively`.

| # | Step | Expected | Result |
|---|------|----------|--------|
| E1 | Open KeePassXC, click any text field, type `fake password text long enough.` | `[harvest] drop hard-blocked app=keepassxc` | |
| E2 | Repeat with Bitwarden / 1Password / Seahorse if available | `drop hard-blocked` for each | |
| E3 | `xtype-corpus stats` | line count unchanged | |

### F. Profile generation (initial)

| # | Step | Expected | Result |
|---|------|----------|--------|
| F1 | Wipe everything: `xtype-corpus wipe`, then `fcitx5-remote -r` | `[corpus] enabled path=...`, no `[profile] load` (no corpus to load yet) | |
| F2 | Hand-write a corpus: paste 50 ASCII sentences into `~/.local/share/xtype/corpus.txt` (mix of 12–49 / 50–100 / 101+ char lengths) | — | |
| F3 | `fcitx5-remote -r` to trigger reload | `[profile] stale=true reason=...`, `[profile] load: exemplars=N avg=M count=50`, `[profile] serialize: ...style_profile.json`, `[prompt] set N chars exemplars=N truncated=0` | |
| F4 | `cat ~/.local/share/xtype/style_profile.json` | valid JSON; `version=1`; 3–5 exemplars; ≥1 `common_openers` entry | |
| F5 | Inspect `[prompt] set` line | `chars > 210` (base prompt size); `exemplars=N` matches profile | |

### G. Refresh timer

Requires `XTYPE_PROFILE_REFRESH_SEC=30` from pre-flight Step 5.

| # | Step | Expected | Result |
|---|------|----------|--------|
| G1 | After F3, idle for ~30 s | `[profile] refresh: timer fired`, `[profile] stale=false reason=none` (corpus unchanged), `[profile] load: ...` | |
| G2 | Type 250+ new sentences (terminated, ≥12 chars each) over a couple minutes | next refresh: `[profile] stale=true reason=stale`, new `[profile] load:` and `[profile] serialize:` and `[prompt] set ...` | |
| G3 | `stat ~/.local/share/xtype/style_profile.json` | mtime changed | |

### H. Budget cap

| # | Step | Expected | Result |
|---|------|----------|--------|
| H1 | Hand-craft a corpus where `loadFromCorpus` will produce exemplars summing well over 2000 chars (e.g. 5 × 600-char sentences) | — | |
| H2 | `fcitx5-remote -r` | `[prompt] set N chars ... truncated=1`; **WARNING** line `system prompt truncated to fit 2000-char budget` | |
| H3 | Confirm `N <= 2000` | | |

### I. `include_examples_in_prompt` toggle

| # | Step | Expected | Result |
|---|------|----------|--------|
| I1 | Edit `config.h`: `include_examples_in_prompt = false`; rebuild + install + `fcitx5-remote -r` | `[prompt] set N chars exemplars=0` even though `[profile] load:` reports a non-zero exemplar count | |
| I2 | Type a sentence; verify corpus collector still records | `[harvest] ok` lines still appear | |

### J. CLI

| # | Step | Expected | Result |
|---|------|----------|--------|
| J1 | After a rotation (or hand-place `corpus.txt.1`): `xtype-corpus stats` | `files:` line lists both files | |
| J2 | `xtype-corpus wipe`, type `wipe` to confirm | reports removed count; `corpus.txt`, `corpus.txt.1`, and `style_profile.json` all gone | |
| J3 | `xtype-corpus wipe` again | `nothing to wipe under ...` | |

### K. Inference still works with personalization on

| # | Step | Expected | Result |
|---|------|----------|--------|
| K1 | After F (profile loaded with exemplars), type `Hello there, ` in Kate | ghost text appears within ~300 ms | |
| K2 | TTFT delta vs no-profile baseline: `python3 scripts/benchmark_ollama.py --runs 20` | record p50/p95 in `docs/personalization-perf.md` field-measurement table | |
| K3 | Tab to accept; full Tab cycle behaves as before | suggestion accepts cleanly; no double-write or stutter | |

---

## Cross-cutting check: `[prompt] set` thread

`[prompt]` lines come from the inference thread (`thread.log`), shown by
`xtype-watch` as `[inf]`. Every other personalization tag comes from the
main engine thread. If you see `[prompt] set ...` arrive **before** the
preceding `[profile] load: ...`, that's a marshaling bug — file an issue.

## Known issues / observations

<!-- Fill in during testing -->
