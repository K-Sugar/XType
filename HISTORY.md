# XType — Project History

> Archive of completed development sessions. This file is **not loaded by Claude Code by
> default** — it exists as a reference for when a current session needs historical context.
> Active work is tracked in `PLAN.md`.

## Project milestones

| Phase | What it delivered | Sessions |
|---|---|---|
| **Phase 0** | Ollama client, ContextBuffer, Debouncer foundations | 1–3 |
| **Phase A** | IBus prototype validated on KDE Wayland (Python) | 4–8 |
| **Phase B** | Fcitx5 production engine (C++17) with browser compat | 9–13.5 |

> **Session 14** (model upgrade + assistant-prefill anti-loop redesign) is intentionally still
> in `PLAN.md` — Sessions 15–17 reference its echo-detection and prompt structure directly.
> It will be moved here once Phase D is complete.

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

### [x] Session 8 — IBus end-to-end integration test

**Files:** `docs/testing-ibus.md`, `~/.config/environment.d/ibus.conf`, `ibus-engine/engine/main.py`

**Objectives:**
- Write `~/.config/environment.d/ibus.conf` with `QT_IM_MODULE=ibus`, `GTK_IM_MODULE=ibus`, `XMODIFIERS=@im=ibus` so all apps pick up IBus automatically on login (no per-launch env prefixing needed)
- Add rotating file logger to `main.py` → `~/.local/share/xtype/engine.log` (always DEBUG, 1 MB / 3 rotations) for post-test analysis
- Test in Kate (Qt6, text-input-v2): ghost text appears inline, Tab accepts word-by-word
- Test in Zen Browser (GTK3, text-input-v3): `GTK_IM_MODULE=ibus`, verify preedit
- Verify Alacritty is blocklisted (no preedit, no glitches)
- Verify focus-out commit in Kate and Zen Browser
- Document any issues in `docs/testing-ibus.md`

**Outcome:** End-to-end validated on KDE Wayland. Known limitations documented in `docs/testing-ibus.md` — notably that `do_focus_in_id` is never called by the KWin IBus bridge (app_id always empty), making blocklist-by-app-id inactive on Wayland. Alacritty passthrough works incidentally because terminals don't route printable chars through the IM.

**Commit:** `test(ibus): end-to-end validation on KDE Wayland — Kate + Zen + Alacritty`

---

## Phase B — Fcitx5 Production (C++17)

### [x] Session 9 — CMake scaffold + addon descriptor

**Files:** `fcitx5-engine/CMakeLists.txt`, `fcitx5-engine/data/xtype-addon.conf.in`, `fcitx5-engine/data/xtype.conf`

**Objectives:**
- CMakeLists.txt: find Fcitx5, C++17, shared library target `xtype-fcitx5`
- `xtype-addon.conf.in`: addon type=SharedLibrary, category=InputMethod
- `xtype.conf`: IM registration (name, native name, icon)
- Verify: `cmake -B build -G Ninja && ninja -C build` produces the .so

**Commit:** `feat(fcitx5): cmake scaffold + addon descriptor`

---

### [x] Session 10 — InferenceClient C++ (libcurl)

**Files:** `fcitx5-engine/src/inference_client.h`, `fcitx5-engine/src/inference_client.cpp`

**Objectives:**
- `InferenceClient` class: libcurl streaming HTTP POST to Ollama `/api/generate`
- `std::thread` background worker, `std::atomic<bool>` cancel token
- Streaming: parse NDJSON response, accumulate tokens, call `onToken` callback
- Marshal result to Fcitx5 main thread via `eventDispatcher().schedule()`
- Same options as Python: num_predict=30, temperature=0.3, top_p=0.9, stop tokens

**Commit:** `feat(fcitx5): libcurl inference client with streaming + cancel`

---

### [x] Session 11 — ContextBuffer C++ port

**Files:** `fcitx5-engine/src/context_buffer.h`, `fcitx5-engine/src/context_buffer.cpp`

**Objectives:**
- 1:1 port of Python `ContextBuffer` using `std::deque<char>`
- Identical API: `appendChar()`, `backspace()`, `setSuggestion()`, `acceptNextWord()`, `acceptAll()`, `dismiss()`, `contextText()`
- Unit tests using Catch2 or doctest (add to CMake)

**Commit:** `feat(fcitx5): context buffer C++ port + unit tests`

---

### [x] Session 12 — XTypeEngine C++ core

**Files:** `fcitx5-engine/src/xtype.h`, `fcitx5-engine/src/xtype.cpp`

**Objectives:**
- `XTypeEngine : public fcitx::InputMethodEngineV2`
- Override `keyEvent()`: same logic as IBus Python engine
- `ic->inputPanel().setClientPreedit()` with `TextFormatFlag::Underline`
- Focus-out: dismiss preedit (clear without committing) — ghost text is AI suggestion, not user input; committing on focus-out inserts unwanted text
- Wire `InferenceClient` callbacks to preedit updates on main thread

**Commit:** `feat(fcitx5): full engine core — key handler + preedit + focus`

---

### [x] Session 13 — KDE Plasma Wayland integration

**Files:** `docs/testing-fcitx5.md`

**Objectives:**
- Activate: Settings → Keyboard → Virtual Keyboard → Fcitx5 (must be KWin-launched)
- Disable IBus autostart to avoid conflicts
- Test matrix: Kate ✅, Firefox (GTK_IM_MODULE=fcitx) ✅, Chromium (--wayland-text-input-version=3) ⚠️, Konsole 🚫, Electron (ELECTRON_OZONE_PLATFORM_HINT=wayland) ⚠️, LibreOffice ✅
- Document results in `docs/testing-fcitx5.md`

**Commit:** `test(fcitx5): KDE Plasma Wayland integration matrix`

---

### [x] Session 13.5 — Browser compatibility: Zen + Chromium ghost text

**Files:** `fcitx5-engine/src/xtype.cpp`, `fcitx5-engine/src/config.h`

**Root-cause analysis (from debug.log):**

**Zen Browser — no ghost text:**
Log lines like `key 'g' prog=zen ctx_len=0` appeared on every keystroke, with `ctx_len` never growing. Between keystrokes, rapid `deactivate prog=zen` / `activate prog=zen` pairs appeared. `activate()` called `resetState()` which called `_ctx.reset()`, wiping the context buffer on every key. Zen (GTK4/Firefox Wayland IM) cycles deactivate+activate per keystroke — this is normal GTK4 behaviour, not a bug. With the original code, `min_context_chars=10` was never reachable.

**Chromium — ghost text commits on defocus:**
Log entries `deactivate prog=chromium hasSuggestion=1` confirmed the `commitString("")` guard saw the suggestion — unlike Kate, where the suggestion was already gone by `deactivate()`. Yet ghost text still committed. `TextFormatFlag::DontCommit` is a Fcitx5-internal signal; Chromium's own text-input-v3 implementation ignores it and commits the preedit via the Wayland protocol before the engine's deactivate callback can act.

**Resolution:**

1. **Zen context buffer reset:** changed `activate()` to NOT call `resetState()`. New `resetInferenceOnly()` helper cancels in-flight inference and increments `_gen` but leaves `_ctx` intact. Only wipes `_ctx` in `deactivate()` when the program changes (tracked via `_lastProg` member). After fix, `ctx_len` grows correctly in Zen between keystrokes.

2. **Chromium focus-out commit:** call `clearPreedit(ic)` at the TOP of `deactivate()`, unconditionally. This sends an empty preedit to the Wayland compositor before Fcitx5 processes the deactivation commit. Even if Chromium then commits the preedit, it commits an empty string. `commitString("")` retained as belt-and-suspenders for other IM clients.

3. **Blocklist case mismatch:** `config.h` had `{"konsole", "alacritty"}` but Fcitx5 reports `ic->program()` as `"Konsole"` and `"Alacritty"` (capitalised). Switched `isBlocked()` to lowercase both sides before matching.

**Outcome:** Ghost text works correctly in Kate, Zen Browser, and Chromium. Konsole and Alacritty are correctly suppressed by the case-insensitive blocklist.

**Commit:** `fix(fcitx5): browser compat — Zen context reset + Chromium focus-out commit`
