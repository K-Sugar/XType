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

**Objectives:**
- Async Ollama HTTP client with streaming support
- Cancel-on-new-request (cancel token pattern)
- TTFT (time-to-first-token) measurement
- Benchmark script: 20 runs against qwen2.5:0.5b, print p50/p95/p99
- Verify <200ms TTFT target on local machine

**Commit:** `feat(inference): async ollama client + TTFT benchmark — p50/p95 results`

---

### [x] Session 2 — ContextBuffer + unit tests

**Files:** `ibus-engine/engine/context_buffer.py`, `ibus-engine/tests/test_context_buffer.py`

**Objectives:**
- `ContextBuffer` class with `collections.deque`, max 500 chars
- Methods: `append_char()`, `backspace()`, `set_suggestion()`, `accept_next_word()`, `accept_all()`, `dismiss()`
- `context_text` property returns full buffer as string
- `accept_next_word()` returns next word + trailing space, advances word index, appends to buffer
- `append_char()` and `backspace()` auto-dismiss any active suggestion
- Full pytest suite: all state transitions, edge cases (empty buffer, single word, trailing spaces)

**Commit:** `feat(context-buffer): state machine with full pytest suite`

---

### [x] Session 3 — Debouncer + cancellation tests

**Files:** `ibus-engine/engine/debouncer.py`, `ibus-engine/tests/test_debouncer.py`

**Objectives:**
- `Debouncer` class: timer-based, configurable delay (default 180ms)
- Each new call cancels the pending timer and restarts
- Thread-safe (used from GLib main thread, fires callback on same thread via GLib.idle_add)
- Tests: rapid-fire calls only trigger once, delay respected, cancel works

**Commit:** `feat(debouncer): 180ms debounce with cancellation + tests`

---

## Phase A — IBus Prototype (Python)

### [x] Session 4 — IBus engine skeleton + XML registration

**Files:** `ibus-engine/cotypist.xml`, `ibus-engine/engine/main.py`, `ibus-engine/engine/engine.py`

**Objectives:**
- `cotypist.xml`: IBus component descriptor (name, exec path, description)
- `main.py`: `IBus.init()`, `IBus.Factory`, `IBus.main()` loop
- `engine.py`: `CotypistEngine(IBus.EngineBase)` skeleton — override `do_process_key_event`, `do_focus_in`, `do_focus_out`, `do_reset`
- Engine registers with factory, loads config, initialises ContextBuffer + Debouncer
- Verify: `ibus-daemon --replace` picks up the engine, no crashes on focus

**Commit:** `feat(ibus): engine skeleton + XML registration`

---

### [x] Session 5 — Preedit + Tab/Escape/Backspace UX

**Files:** `ibus-engine/engine/engine.py` (extend)

**Objectives:**
- Implement full key event flow from spec:
  - Printable char → pass through, append to buffer, debounce → inference
  - Tab (suggestion active) → commit next word, update preedit with remainder
  - Shift+Tab → commit entire suggestion, clear preedit
  - Escape → dismiss, clear preedit
  - Backspace (suggestion active) → dismiss only; (no suggestion) → remove last char
  - Enter → dismiss, pass through
  - Modifier combos → always pass through
  - Key releases → always ignore
- Preedit styling: `IBus.AttrType.UNDERLINE` + `IBus.AttrType.FOREGROUND` 0x888888
- Wire debouncer callback to inference, inference result calls `set_suggestion()` + updates preedit

**Commit:** `feat(ibus): preedit ghost text + full key handler UX`

---

### [x] Session 6 — Focus/reset/commit edge cases

**Files:** `ibus-engine/engine/engine.py` (extend)

**Objectives:**
- `do_focus_out`: MUST call `commit_text()` on any active preedit before clearing — text is lost otherwise
- `do_focus_in`: clear preedit, reset context buffer
- `do_reset`: same as focus_in
- Cancel any in-flight inference on focus change
- Manual test checklist: switch apps mid-suggestion, click away, Ctrl+Tab

**Commit:** `fix(ibus): focus-out commits preedit, focus-in resets state`

---

### [x] Session 7 — Config system + blocklist

**Files:** `ibus-engine/engine/config.py`, `ibus-engine/tests/test_config.py`

**Objectives:**
- TOML config loader using `tomllib` (stdlib, Python 3.11+)
- Reads from `~/.config/cotypist-linux/config.toml`, falls back to defaults
- Sections: `[inference]` (model, ollama_host, debounce_ms, min_context_chars, context_window), `[behaviour]` (tab_accepts_word, passthrough_terminals, blocklist_apps)
- `passthrough_terminals = true` + `blocklist_apps` → engine returns False immediately for those window classes
- Ollama health check on startup: `GET /api/tags`, log warning if unreachable
- Write default config file if missing

**Commit:** `feat(config): TOML config + app blocklist + ollama health check`

---

### [ ] Session 8 — IBus end-to-end integration test

**Files:** `docs/testing-ibus.md`, manual test log

**Objectives:**
- Test in Kate (Qt6, text-input-v2): ghost text appears inline, Tab accepts word-by-word
- Test in Firefox (GTK3, text-input-v3): set `GTK_IM_MODULE=fcitx`, verify preedit
- Verify Konsole is blocklisted (no preedit, no glitches)
- Verify focus-out commit in all three apps
- Document any issues in `docs/testing-ibus.md`

**Commit:** `test(ibus): end-to-end validation on KDE Wayland — Kate + Firefox + Konsole`

---

## Phase B — Fcitx5 Production (C++17)

### [ ] Session 9 — CMake scaffold + addon descriptor

**Files:** `fcitx5-engine/CMakeLists.txt`, `fcitx5-engine/data/cotypist-addon.conf.in`, `fcitx5-engine/data/cotypist.conf`

**Objectives:**
- CMakeLists.txt: find Fcitx5, C++17, shared library target `cotypist-fcitx5`
- `cotypist-addon.conf.in`: addon type=SharedLibrary, category=InputMethod
- `cotypist.conf`: IM registration (name, native name, icon)
- Verify: `cmake -B build -G Ninja && ninja -C build` produces the .so

**Commit:** `feat(fcitx5): cmake scaffold + addon descriptor`

---

### [ ] Session 10 — InferenceClient C++ (libcurl)

**Files:** `fcitx5-engine/src/inference_client.h`, `fcitx5-engine/src/inference_client.cpp`

**Objectives:**
- `InferenceClient` class: libcurl streaming HTTP POST to Ollama `/api/generate`
- `std::thread` background worker, `std::atomic<bool>` cancel token
- Streaming: parse NDJSON response, accumulate tokens, call `onToken` callback
- Marshal result to Fcitx5 main thread via `eventDispatcher().schedule()`
- Same options as Python: num_predict=30, temperature=0.3, top_p=0.9, stop tokens

**Commit:** `feat(fcitx5): libcurl inference client with streaming + cancel`

---

### [ ] Session 11 — ContextBuffer C++ port

**Files:** `fcitx5-engine/src/context_buffer.h`, `fcitx5-engine/src/context_buffer.cpp`

**Objectives:**
- 1:1 port of Python `ContextBuffer` using `std::deque<char>`
- Identical API: `appendChar()`, `backspace()`, `setSuggestion()`, `acceptNextWord()`, `acceptAll()`, `dismiss()`, `contextText()`
- Unit tests using Catch2 or doctest (add to CMake)

**Commit:** `feat(fcitx5): context buffer C++ port + unit tests`

---

### [ ] Session 12 — CotypistEngine C++ core

**Files:** `fcitx5-engine/src/cotypist.h`, `fcitx5-engine/src/cotypist.cpp`

**Objectives:**
- `CotypistEngine : public fcitx::InputMethodEngineV2`
- Override `keyEvent()`: same logic as IBus Python engine
- `ic->inputPanel().setClientPreedit()` with `TextFormatFlag::Underline`
- Focus-out: commit preedit via `ic->commitString()` before clearing
- Wire `InferenceClient` callbacks to preedit updates on main thread

**Commit:** `feat(fcitx5): full engine core — key handler + preedit + focus`

---

### [ ] Session 13 — KDE Plasma Wayland integration

**Files:** `docs/testing-fcitx5.md`

**Objectives:**
- Activate: Settings → Keyboard → Virtual Keyboard → Fcitx5 (must be KWin-launched)
- Disable IBus autostart to avoid conflicts
- Test matrix: Kate ✅, Firefox (GTK_IM_MODULE=fcitx) ✅, Chromium (--wayland-text-input-version=3) ⚠️, Konsole 🚫, Electron (ELECTRON_OZONE_PLATFORM_HINT=wayland) ⚠️, LibreOffice ✅
- Document results in `docs/testing-fcitx5.md`

**Commit:** `test(fcitx5): KDE Plasma Wayland integration matrix`

---

## Phase C — Packaging & Polish

### [ ] Session 14 — PKGBUILD + systemd + AUR

**Files:** `packaging/PKGBUILD`, `packaging/cotypist-linux.service`, `packaging/org.cotypist.linux.desktop`

**Objectives:**
- PKGBUILD: depends on `fcitx5`, `ollama`, `python`, split package (ibus-dev + fcitx5-prod)
- systemd user service: starts Ollama, `WantedBy=default.target`
- `.desktop` file for settings entry
- `makepkg -si` succeeds cleanly on CachyOS

**Commit:** `feat(packaging): PKGBUILD + systemd service + AUR-ready`

---

### [ ] Session 15 — Settings UI + per-app profiles

**Files:** `settings/` (new directory)

**Objectives:**
- Qt/KDE settings panel (KCModule or standalone)
- Expose: model selection, debounce_ms slider, blocklist editor
- Per-app profile overrides (model, debounce, enable/disable)
- Integrates with KDE System Settings

**Commit:** `feat(settings): KDE settings UI + per-app profile overrides`

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

Scopes: `inference`, `context-buffer`, `debouncer`, `ibus`, `fcitx5`, `config`, `packaging`, `settings`

---

## Quick reference — critical gotchas

1. **Focus-out MUST commit preedit** — text is silently lost otherwise
2. **Never call IBus/Fcitx5 APIs from the inference thread** — always marshal to main thread
3. **Fcitx5 on KDE Wayland** — must be launched by KWin, not autostarted
4. **Chromium/Electron** — need flags/env vars for text-input-v3 preedit support
5. **Terminals (Konsole, Alacritty)** — blocklist them, preedit causes visual glitches
