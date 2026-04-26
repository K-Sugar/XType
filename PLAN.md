# XType — Development Plan

> This file is the source of truth for progress. At the start of each session, read this file
> and the current session's objectives. At the end of each session, mark the session complete,
> update the status, and commit with the message format specified below.

## How to use this plan

1. Read `CLAUDE.md` and this file at the start of every session
2. Work through the current session's objectives completely
3. Write tests where specified — do not skip them
4. When the session is done, update the status marker: `[ ]` → `[x]`
5. Commit with the exact message format shown for that session
6. Push to `origin main`

## Commits
- One commit per session, using the exact message in PLAN.md
- Never commit with failing tests
- Never bundle two sessions into one commit

---

## Status legend

- `[x]` — complete
- `[~]` — in progress / current session
- `[ ]` — not started

---

## Phase 0 — Environment & Foundations

### [x] Session 1 — Ollama client + latency benchmark
**Files:** `ibus-engine/engine/inference.py`, `scripts/benchmark_ollama.py`
**Commit:** `feat(inference): async ollama client + TTFT benchmark — p50/p95 results`

### [x] Session 2 — ContextBuffer + unit tests
**Files:** `ibus-engine/engine/context_buffer.py`, `ibus-engine/tests/test_context_buffer.py`
**Commit:** `feat(context-buffer): state machine with full pytest suite`

### [x] Session 3 — Debouncer + cancellation tests
**Files:** `ibus-engine/engine/debouncer.py`, `ibus-engine/tests/test_debouncer.py`
**Commit:** `feat(debouncer): 180ms debounce with cancellation + tests`

---

## Phase A — IBus Prototype (Python)

### [x] Session 4 — IBus engine skeleton + XML registration
**Commit:** `feat(ibus): engine skeleton + XML registration`

### [x] Session 5 — Preedit + Tab/Escape/Backspace UX
**Commit:** `feat(ibus): preedit ghost text + full key handler UX`

### [x] Session 6 — Focus/reset/commit edge cases
**Commit:** `fix(ibus): focus-out commits preedit, focus-in resets state`

### [x] Session 7 — Config system + blocklist
**Commit:** `feat(config): TOML config + app blocklist + ollama health check`

### [x] Session 8 — IBus end-to-end integration test
**Commit:** `test(ibus): end-to-end validation on KDE Wayland — Kate + Zen + Alacritty`

---

## Phase B — Fcitx5 Production (C++17)

### [x] Session 9 — CMake scaffold + addon descriptor
**Commit:** `feat(fcitx5): cmake scaffold + addon descriptor`

### [x] Session 10 — InferenceClient C++ (libcurl)
**Commit:** `feat(fcitx5): libcurl inference client with streaming + cancel`

### [x] Session 11 — ContextBuffer C++ port
**Commit:** `feat(fcitx5): context buffer C++ port + unit tests`

### [x] Session 12 — XTypeEngine C++ core
**Commit:** `feat(fcitx5): full engine core — key handler + preedit + focus`

### [x] Session 13 — KDE Plasma Wayland integration
**Commit:** `test(fcitx5): KDE Plasma Wayland integration matrix`

### [x] Session 13.5 — Browser compatibility: Zen + Chromium ghost text
**Commit:** `fix(fcitx5): browser compat — Zen context reset + Chromium focus-out commit`

### [x] Session 14 — Model upgrade + assistant-prefill chat format (anti-loop)
**Commit:** `feat(fcitx5): qwen2.5:1.5b + assistant-prefill chat format — anti-loop inference redesign`

---

## Phase D — Personalization Engine

> Goal: the engine learns the user's writing style over time and uses it to inform suggestions.
> Approach: passively collect typed text into a local corpus, periodically extract representative
> samples, inject them as few-shot examples into the system prompt. **No fine-tuning, no cloud,
> no telemetry.** The corpus is local-only and opt-in.

### [x] Session 15 — Writing corpus collector

**Files:** `fcitx5-engine/src/corpus_collector.h`, `fcitx5-engine/src/corpus_collector.cpp`, `fcitx5-engine/src/config.h` (extend), `fcitx5-engine/src/xtype.cpp` (wire in)

**Objectives:**

1. **Extend `config.h` with `[learning]` section:**
   ```cpp
   struct LearningConfig {
       bool        enabled            = false;  // OPT-IN: default off
       std::string corpus_path        = "~/.local/share/xtype/corpus.txt";
       int         flush_interval_sec = 60;
       int         max_corpus_mb      = 50;
       int         min_sentence_chars = 12;     // skip fragments
   };
   ```
   Add `LearningConfig learning;` to `XTypeConfig`.

2. **`CorpusCollector` class** (background-thread, append-only writer):
   - Constructor takes `LearningConfig`, expands `~` in path
   - `void record(std::string text)` — thread-safe, non-blocking; pushes to bounded `std::deque<std::string>` (cap 1000, drop-oldest on overflow)
   - Background `std::thread` flushes the queue to disk every `flush_interval_sec`, or when queue size > 100
   - On flush: append each entry + `'\n'` to corpus file, then check size — if > `max_corpus_mb`, rotate (rename to `corpus.txt.1`, start fresh)
   - Destructor: signal shutdown, join thread, final flush

3. **Hook into `XTypeEngine`:**
   - Add `std::unique_ptr<CorpusCollector> _corpus;` member
   - In constructor, if `_cfg.learning.enabled && !_cfg.learning.corpus_path.empty()`: instantiate
   - In `keyEvent` after a printable char is appended: if the char is in `{'.', '!', '?', '\n'}`, extract the trailing sentence from `_ctx.contextText()` (split on previous `[.!?\n]`), and if `len >= min_sentence_chars` and `!isBlocked(program)`, call `_corpus->record(sentence)`
   - In `deactivate`: if buffer has unfinished sentence ≥ `min_sentence_chars` and learning enabled and not blocked, record it

4. **Privacy guards:**
   - **Default blocklist must include:** `keepassxc`, `1password`, `bitwarden`, `gnome-keyring`, `seahorse`. Add these to the `BehaviourConfig::blocklist_apps` default in `config.h`.
   - Never record text if `isBlocked(ic->program())` returns true — check before every `record()` call
   - Never record AI-generated suggestions — only record from chars the user typed (i.e. trace back through `appendChar` only, not `acceptNextWord`/`acceptAll`). Easiest implementation: maintain a parallel `_userTypedBuffer` that only `appendChar` writes to; harvest sentences from there

5. **CLI sub-command** in `scripts/xtype-corpus`:
   - `xtype-corpus stats` — prints lines, word count, file size
   - `xtype-corpus wipe` — deletes corpus and rotated backups (with confirmation prompt)

**Gotchas:**
- The IM thread MUST NOT block on disk I/O. Queue everything, flush in background.
- File rotation must be atomic-ish: `rename()` is atomic on the same filesystem; do that, then `open()` fresh.
- If `corpus_path` parent directory doesn't exist, create it (`std::filesystem::create_directories`).
- Sensitive-app blocklist applies to the corpus collector even if the user removes those apps from `behaviour.blocklist_apps`. Keep a hard-coded list of always-blocked-for-corpus apps in `corpus_collector.cpp`.
- Sentences containing only whitespace, only punctuation, or matching a regex of code-shaped patterns (`[{};=]{2,}`) should be filtered out at record-time.

**Commit:** `feat(personalization): writing corpus collector with opt-in logging + privacy guards`

---

### [x] Session 16 — Style profile extraction

**Files:** `fcitx5-engine/src/style_profile.h`, `fcitx5-engine/src/style_profile.cpp`, `fcitx5-engine/tests/test_style_profile.cpp`

**Objectives:**

1. **`StyleProfile` class:**
   - `void loadFromCorpus(string corpus_path)` — reads file, parses sentences (split on `[.!?\n]`, trim, filter < 12 chars)
   - Computes:
     - `std::vector<std::string> exemplars` — 3-5 representative sentences, sampled to cover varied lengths (one short ~30 chars, one medium ~80, one long ~150, plus 1-2 random)
     - `int avg_sentence_chars`
     - `std::vector<std::string> common_openers` — top 10 bigrams that start sentences (after lowercasing)
   - `std::string generatePreamble()` — produces:
     ```
     The user's typical writing style:
     - {exemplar 1}
     - {exemplar 2}
     - {exemplar 3}
     ```
   - `void serialize(string path)` / `static StyleProfile deserialize(string path)` — JSON, stored at `~/.local/share/xtype/style_profile.json`
   - Cache field: `time_t last_updated` — preamble regeneration only when `corpus_mtime > last_updated + 30 minutes` OR corpus has > 200 new lines since last profile

2. **Privacy filtering during extraction:**
   - Skip sentences containing email-like patterns (`\S+@\S+\.\S+`)
   - Skip sentences with sequences of digits ≥ 5 (likely IDs, phone numbers, addresses)
   - Skip sentences containing `password`, `secret`, `token`, `key`, `auth` (case-insensitive)
   - Drop the longest 5% and shortest 5% of sentences before sampling exemplars (outliers)

3. **Refresh trigger** (wired in Session 17): poll corpus mtime every 60s on a low-priority timer; if changed and profile stale, regenerate.

4. **Tests** (`test_style_profile.cpp`):
   - `loadFromCorpus` parses sentence boundaries correctly
   - Length-varied exemplar sampling produces 3-5 sentences across length buckets
   - Privacy filter rejects email/secret patterns
   - `serialize`/`deserialize` round-trip preserves all fields
   - Empty corpus produces empty preamble, not a crash

**Gotchas:**
- Corpus may be large (50MB cap × thousands of sentences). Stream-read line by line, don't slurp the whole file.
- Exemplar selection should be deterministic-with-seed for testability — pass an optional `unsigned seed` to the sampler, default to `time(nullptr)`.
- Don't extract more than 1000 sentences worth of statistics — cap the input scan to reduce CPU on large corpora.
- The profile is referenced by Session 17's prompt builder, which runs on the GLib/eventDispatcher main thread. Profile generation MUST happen on a background thread; only the resulting `std::string preamble` is touched by the main thread.

**Commit:** `feat(personalization): style profile extraction with privacy filtering + tests`

---

### [x] Session 17 — Dynamic system prompt assembly

**Files:** `fcitx5-engine/src/inference_client.h` (extend), `fcitx5-engine/src/inference_client.cpp` (modify), `fcitx5-engine/src/xtype.cpp` (compose), `fcitx5-engine/src/xtype.h` (add member)

**Objectives:**

1. **Make `InferenceClient` system prompt mutable:**
   - Remove `static constexpr char SYSTEM_PROMPT[]` from `inference_client.cpp`
   - Add `std::string _system_prompt` member to `InferenceClient`, default-initialised to the current base text
   - Add `void set_system_prompt(std::string)` — thread-safe via `std::mutex`
   - In `build_payload`, read `_system_prompt` under the mutex
   - The base instruction stays the same as today; only style/user additions are layered on

2. **`PromptBuilder` helper in `xtype.cpp`** (anonymous namespace or static class):
   ```cpp
   static std::string buildSystemPrompt(
       const std::string& baseInstruction,
       const StyleProfile* profile,        // nullptr if no profile
       const UserPromptConfig& userPrompt, // from Session 19
       bool includeExamples
   );
   ```
   Output structure:
   ```
   <baseInstruction>

   [if userPrompt.description non-empty]
   About the user: <description>

   [if includeExamples && profile has exemplars]
   The user's typical writing style:
   - <exemplar 1>
   - <exemplar 2>
   - <exemplar 3>

   [if userPrompt.avoid_phrases non-empty]
   Avoid these phrases: <comma-joined>
   ```

3. **Wire into `XTypeEngine`:**
   - Add `std::unique_ptr<StyleProfile> _profile;` and `std::unique_ptr<fcitx::EventSourceTime> _profileRefreshTimer;` members
   - On engine startup: if `learning.enabled`, schedule profile load (background thread); on completion, marshal back via `eventDispatcher().schedule()` to call `_inference.set_system_prompt(buildSystemPrompt(...))`
   - Profile refresh timer fires every 5 minutes — checks corpus mtime, regenerates profile if stale, updates inference system prompt
   - New config field `learning.include_examples_in_prompt` (default `true`) toggles exemplar injection without disabling collection

4. **Token budget cap:**
   - Final assembled prompt > 2000 chars → truncate exemplars (drop longest first) until under budget
   - Log a warning if truncation happens

**Gotchas:**
- Bigger system prompts increase TTFT measurably. Re-run `scripts/benchmark_ollama.py` after this session and document the delta in `docs/personalization-perf.md`.
- The head-of-context echo detection in `xtype.cpp` (Session 14) was tuned against the old prompt. Re-test with `qwen2.5:1.5b` + style preamble: type a paragraph, verify the model doesn't echo from char 0 of context. If it does, lower `context_window` from 150 to 120.
- `set_system_prompt` must NOT be called while `execute()` is reading `_system_prompt` for a payload — that's why the mutex. Test cancel-during-prompt-update.
- Profile regeneration on a 5-minute timer + active typing means the system prompt can change between requests in the same typing session. That's fine; the model handles it gracefully.

**Commit:** `feat(personalization): dynamic system prompt with style examples + profile refresh`

---

## Phase E — Model & Prompt Customization

### [ ] Session 18 — Model presets with hardware tiers

**Files:** `fcitx5-engine/src/config.h` (extend), `fcitx5-engine/src/xtype.cpp` (resolve preset), `scripts/detect_hardware.sh` (new), `docs/models.md` (new)

**Objectives:**

1. **Extend `config.h`:**
   ```cpp
   enum class ModelTier { Auto, Low, Balanced, High, Enthusiast };

   struct ModelPresetConfig {
       ModelTier tier = ModelTier::Auto;
   };
   ```
   Add `ModelPresetConfig preset;` to `XTypeConfig`.

2. **Tier → model mapping** (compile-time table in `xtype.cpp`):

   | Tier | Model | Approx RAM | Target TTFT |
   |---|---|---|---|
   | Low | `qwen2.5:0.5b` | ~1 GB | <150 ms |
   | Balanced | `qwen2.5:1.5b` | ~2 GB | <300 ms |
   | High | `qwen2.5:3b` | ~3.5 GB | <500 ms |
   | Enthusiast | `qwen2.5:7b-instruct-q4_K_M` | ~5 GB | <800 ms |

3. **Auto-detection logic** in `XTypeEngine` constructor:
   ```cpp
   long pages   = sysconf(_SC_PHYS_PAGES);
   long pgsize  = sysconf(_SC_PAGE_SIZE);
   long mem_gb  = (pages * pgsize) / (1024L * 1024L * 1024L);
   ```
   - `< 6 GB` → Low
   - `6–12 GB` → Balanced
   - `12–24 GB` → High
   - `≥ 24 GB` → Enthusiast

4. **Resolution precedence:**
   - If `inference.model` is explicitly set in TOML (non-default), it wins (back-compat)
   - Else if `preset.tier != Auto`, use the tier's model
   - Else use auto-detected tier's model
   - Log the resolved choice on startup: `dbg("model resolved: tier=%s model=%s mem=%ldGB", ...)`

5. **Health check fallback:**
   - On startup, if the resolved model fails `health_check()`, log a warning and fall back to `qwen2.5:1.5b` (Balanced)
   - If THAT fails too, keep the broken model — user must fix Ollama

6. **`scripts/detect_hardware.sh`** — standalone bash script:
   - Prints detected RAM, recommended tier, suggested `ollama pull` command
   - Useful for troubleshooting and for the Settings UI later

7. **`docs/models.md`** — table of tiers with measured TTFT (run benchmarks on your machine for the row that applies, leave others as estimates), example use cases, and `ollama pull` commands.

**Gotchas:**
- VRAM-based detection (for users with discrete GPUs running Ollama on GPU) is harder — Ollama uses GPU automatically when present, so RAM tier is still a sane proxy. Don't over-engineer.
- Don't auto-pull models on startup. If the resolved model isn't available, log clearly: `WARN: tier 'high' wants 'qwen2.5:3b' but it's not pulled. Run: ollama pull qwen2.5:3b`
- Per-app overrides (Session 20) can override the resolved model — account for that in the resolution order.

**Commit:** `feat(config): model presets with hardware-tier auto-selection + docs`

---

### [ ] Session 19 — Personal prompt configuration

**Files:** `fcitx5-engine/src/config.h` (extend), `fcitx5-engine/src/xtype.cpp` (consume in `buildSystemPrompt`)

**Objectives:**

1. **Extend `config.h`:**
   ```cpp
   struct UserPromptConfig {
       std::string              description;       // user's description of self/voice
       std::vector<std::string> avoid_phrases;     // phrases the model should NOT use
       std::string              tone;              // e.g. "formal", "casual", "technical"
   };
   ```
   Add `UserPromptConfig user_prompt;` to `XTypeConfig`.

2. **Default config TOML** (auto-written if missing) gets a new section with documenting comments:
   ```toml
   [user_prompt]
   # Multi-line description of your writing voice, role, common topics.
   # Keep it under 1000 characters for low TTFT.
   description = """
   I am a software engineer. I write technical documentation, emails, and
   code review comments. I prefer concise, direct prose without filler.
   """

   # Phrases the model should avoid. The model will be told to skip these.
   avoid_phrases = ["hope this helps", "feel free to", "I hope you're well"]

   # Tone hint: "formal" | "casual" | "technical" | "" (none)
   tone = ""
   ```

3. **Wire into `buildSystemPrompt`** (Session 17 added the function; this session populates the user-prompt branches):
   - If `description` non-empty: append `About the user: <description>`
   - If `tone` non-empty: append `Preferred tone: <tone>`
   - If `avoid_phrases` non-empty: append `Avoid these phrases: <comma-joined, quoted>`

4. **Validation in config loader:**
   - Truncate `description` to 1000 chars with a warning log if exceeded
   - Cap `avoid_phrases` to 20 entries

5. **Hot-reload deferred to v2.** Document in the default config that `fcitx5-remote -r` is needed to apply changes.

**Gotchas:**
- TOML triple-quoted strings preserve newlines — fine for human readability, but they get sent verbatim to the model. Strip leading/trailing whitespace and collapse internal blank-line runs to single newlines on load.
- The user might paste sensitive info (their real name, employer, etc.) into `description`. That data goes only to local Ollama; document this in the config comments and `docs/personalization.md`.
- Avoid-phrases interaction with the model: Qwen 2.5 sometimes ignores negative instructions. Test that `avoid_phrases = ["hope this helps"]` actually reduces (not eliminates) those phrases in suggestions.

**Commit:** `feat(personalization): user-defined prompt with description, tone, avoid-list`

---

## Phase F — Per-App Settings

### [ ] Session 20 — Per-app config overrides

**Files:** `fcitx5-engine/src/config.h` (extend), `fcitx5-engine/src/xtype.cpp` (apply overlay), `fcitx5-engine/src/xtype.h` (add per-app inference cache)

**Objectives:**

1. **Extend `config.h`:**
   ```cpp
   struct AppOverride {
       std::optional<bool>        enabled;
       std::optional<std::string> model;
       std::optional<int>         debounce_ms;
       std::optional<std::string> prompt_addendum;
   };

   struct XTypeConfig {
       // ...existing fields...
       std::map<std::string, AppOverride> apps;  // key: lowercase program name
   };
   ```

2. **TOML syntax** (subtables):
   ```toml
   [apps.kate]
   model = "qwen2.5:3b"        # better suggestions in code editor
   debounce_ms = 220

   [apps.firefox]
   enabled = false              # disable in browser

   [apps.libreoffice]
   prompt_addendum = "Writing formal documentation."

   [apps.thunderbird]
   prompt_addendum = "Writing professional email."
   debounce_ms = 300
   ```

3. **Resolution function** in `xtype.cpp`:
   ```cpp
   struct EffectiveAppConfig {
       bool        enabled;
       std::string model;
       int         debounce_ms;
       std::string prompt_addendum;
   };
   EffectiveAppConfig resolveForApp(const std::string& program) const;
   ```
   Lowercases program name, looks up in `_cfg.apps`, layers override on top of base config.

4. **Per-app `InferenceClient` pool:**
   - `XTypeEngine` adds `std::map<std::string, std::unique_ptr<InferenceClient>> _inferencePool;` keyed by model name
   - On `requestInference`: resolve effective model for current app, look up or instantiate the matching `InferenceClient`, route the request
   - Pool size cap: 3 clients (the default + 2 most-recently-used app-specific). Evict LRU on cap.
   - Eviction must call `cancel()` and let destructor join the thread cleanly.

5. **Apply overrides in keyEvent / activate:**
   - In `activate(...)`: resolve `EffectiveAppConfig` for the current program, store in `_currentApp` member
   - In `keyEvent`: if `!_currentApp.enabled`, return immediately (alias for blocklist)
   - Use `_currentApp.debounce_ms` for the timer
   - In prompt builder: append `_currentApp.prompt_addendum` to the system prompt

6. **`enabled = false` is an alias for blocklist:**
   - `isBlocked()` continues to consult `behaviour.blocklist_apps` as before
   - Plus: returns true if `apps[program].enabled == false`
   - The two mechanisms coexist; users can use whichever feels natural

7. **Tests** (`tests/test_config.cpp` or extend existing):
   - `resolveForApp` returns base config when no app entry exists
   - `resolveForApp` overlays partial overrides correctly
   - Case-insensitive lookup works for `Kate` / `kate` / `KATE`
   - `enabled = false` is detected by `isBlocked`

**Gotchas:**
- `InferenceClient` instantiation is non-trivial (spawns a thread, opens a curl session). Don't create one per-keystroke — only on first use of a new model.
- When the app changes mid-typing (rare but possible: tab switch in same window class), the in-flight inference on the old client gets cancelled (`++_gen` already handles staleness), and the new client takes over on the next debounce.
- A model in an `[apps.X]` override that isn't pulled: log warning, fall back to base config's resolved model for that app.
- Per-app `prompt_addendum` is appended AFTER user-prompt sections from Session 19 — i.e. the per-app addendum is the most-specific layer. Document this layering in `docs/personalization.md`.
- Watch for memory: 3 simultaneous `qwen2.5:1.5b` clients = 3× HTTP-thread overhead, but the model itself lives in Ollama and is shared. Per-client overhead is small (~MB); loaded models in Ollama are managed by Ollama (idle unload).

**Commit:** `feat(config): per-app overrides — model, enable, debounce, prompt addendum`

---

## Phase G — Packaging & Polish

### [ ] Session 21 — PKGBUILD + systemd + AUR

**Files:** `packaging/PKGBUILD`, `packaging/xtype-linux.service`, `packaging/org.xtype.linux.desktop`

**Objectives:**
- PKGBUILD: depends on `fcitx5`, `ollama`, `python` (for hardware-detection script and corpus CLI), `curl`. Build deps: `cmake`, `ninja`, `gcc`, `pkgconf`. Source from a tagged release.
- Install layout:
  - `libxtype-fcitx5.so` → `/usr/lib/fcitx5/`
  - `xtype.conf` (addon) → `/usr/share/fcitx5/addon/`
  - `xtype.conf` (IM) → `/usr/share/fcitx5/inputmethod/`
  - `scripts/xtype-corpus` → `/usr/bin/xtype-corpus`
  - `scripts/detect_hardware.sh` → `/usr/bin/xtype-detect-hardware`
  - Default config → `/usr/share/xtype/config.toml.example`
- systemd user service: `xtype-linux.service` — `WantedBy=default.target`, `ExecStart=/usr/bin/ollama serve`, `Restart=on-failure`. NB: only enable if Ollama isn't already running as a system service. Document this in the service file.
- `.desktop` file for KDE settings entry (calls the Settings UI from Session 22)
- Post-install message: instruct user to run `fcitx5-remote -r` and pick XType in `fcitx5-configtool`
- `makepkg -si` succeeds cleanly on CachyOS

**Gotchas:**
- `FCITX_INSTALL_ADDONDIR` and `FCITX_INSTALL_PKGDATADIR` from CMake must resolve to the standard system paths under `/usr` (not `/usr/local`) for the AUR build. Pass `-DFCITX_INSTALL_USE_FCITX_SYS_PATHS=ON`.
- Don't bundle Ollama; depend on the system package. Don't bundle models — they're 1–5 GB each. Document `ollama pull qwen2.5:1.5b` in the post-install message.

**Commit:** `feat(packaging): PKGBUILD + systemd service + AUR-ready`

---

### [ ] Session 22 — Settings UI + per-app profiles

(merged into Session U1c — see ui-ultra-plan/)

---

## Commit message conventions

```
feat(scope): short description
fix(scope): short description
test(scope): short description
refactor(scope): short description
docs(scope): short description
chore(scope): short description
```

Scopes: `inference`, `context-buffer`, `debouncer`, `ibus`, `fcitx5`, `config`, `personalization`, `packaging`, `settings`

---

## Quick reference — critical gotchas

1. **Focus-out MUST NOT commit AI ghost text** — use `TextFormatFlag::DontCommit` + `commitString("")` in `deactivate`
2. **Never call IBus/Fcitx5 APIs from the inference thread** — always marshal to main thread
3. **Fcitx5 on KDE Wayland** — must be launched by KWin, not autostarted
4. **Chromium/Electron** — need flags/env vars for text-input-v3 preedit support
5. **Terminals (Konsole, Alacritty)** — blocklist them, preedit causes visual glitches
6. **Corpus is private and local-only** — never sent over the network, hard-blocklisted in password manager apps
7. **System prompt size impacts TTFT** — keep style preamble + user description under 2000 chars combined
8. **Per-app InferenceClient pool capped at 3** — evict LRU; cancel + join cleanly on eviction

---

## Future work (not yet planned as sessions)

- **LoRA fine-tuning loop**: separate offline script that trains a tiny LoRA adapter on the corpus monthly, registers it via Ollama Modelfile (`ADAPTER ./xtype.gguf`), engine swaps to the adapted model. Requires GPU + evaluation harness.
- **Per-field prompt context**: detect field type (email subject, code editor, chat input) via AT-SPI heuristics or window-title parsing, vary prompt accordingly.
- **Suggestion ranking from acceptance history**: track which suggestions get accepted vs dismissed, use that to bias future generation (temperature, top_p, even prompt phrasing).
- **Multi-user / cloud sync of style profile**: explicitly out of scope; would break the local-only privacy guarantee.
