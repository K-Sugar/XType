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
Log lines like `key 'g' prog=zen ctx_len=0` appear on every keystroke, with `ctx_len` never growing. Between keystrokes, rapid `deactivate prog=zen` / `activate prog=zen` pairs appear. `activate()` calls `resetState()` which calls `_ctx.reset()`, wiping the context buffer on every key. Zen (GTK4/Firefox Wayland IM) cycles deactivate+activate per keystroke — this is normal GTK4 behaviour, not a bug. With the current code, `min_context_chars=10` is never reachable.

**Chromium — ghost text commits on defocus:**
Log entries `deactivate prog=chromium hasSuggestion=1` confirm that the `commitString("")` guard DOES see the suggestion — unlike Kate, where the suggestion was already gone by `deactivate()`. Yet ghost text still commits. `TextFormatFlag::DontCommit` is a Fcitx5-internal signal; Chromium's own text-input-v3 implementation ignores it and commits the preedit via the Wayland protocol before the engine's deactivate callback can act.

**Objectives:**

1. **Fix Zen context buffer reset:**
   - Change `activate()` to NOT call `resetState()`. Instead call a new `resetInferenceOnly()` helper that cancels in-flight inference and increments `_gen` but leaves `_ctx` intact.
   - Only wipe `_ctx` in `deactivate()` when the program changes (track `_lastProg` member, compare against `ic->program()`).
   - `reset()` and explicit refocus to a different app should still call full `resetState()`.
   - After fix, verify `ctx_len` grows correctly in Zen between keystrokes.

2. **Fix Chromium focus-out commit:**
   - The current `deactivate()` calls `commitString("")` when `hasSuggestion`. This fires the empty commit AFTER Chromium has already committed the old preedit.
   - Fix: call `clearPreedit(ic)` (sets empty preedit + `updatePreedit()`) at the TOP of `deactivate()`, unconditionally. This sends an empty preedit to the Wayland compositor before Fcitx5 processes the deactivation commit. Even if Chromium then commits the preedit, it commits an empty string.
   - Implementation in `deactivate()`:
     ```cpp
     void XTypeEngine::deactivate(...) {
         auto *ic = event.inputContext();
         dbg("deactivate prog=%s hasSuggestion=%d", ...);
         clearPreedit(ic);          // clear preedit first — prevents Chromium from committing ghost text
         if (_ctx.hasSuggestion())
             ic->commitString("");  // belt-and-suspenders for other IM clients
         resetState(ic);
     }
     ```

3. **Fix blocklist case mismatch:**
   - `config.h` has `{"konsole", "alacritty"}` but Fcitx5 reports `ic->program()` as `"Konsole"` and `"Alacritty"` (capitalised). Change `isBlocked()` to do case-insensitive comparison, or lowercase the program name before matching.
   - Simplest fix: lowercase both sides in `isBlocked()`:
     ```cpp
     bool XTypeEngine::isBlocked(const std::string &program) const {
         std::string prog_lower = program;
         std::transform(prog_lower.begin(), prog_lower.end(), prog_lower.begin(), ::tolower);
         for (const auto &app : _cfg.behaviour.blocklist_apps) {
             if (prog_lower.find(app) != std::string::npos) return true;
         }
         return false;
     }
     ```

4. **Rebuild, reinstall, and test:**
   - Kate: ghost text still works, no regression
   - Zen Browser: ghost text now appears after typing ≥10 chars
   - Chromium: ghost text appears, does NOT commit on defocus
   - Konsole/Alacritty: blocklist correctly suppresses ghost text (case fix)

**Gotchas:**
- `clearPreedit()` in `deactivate()` must fire before the Wayland commit sequence completes — calling it synchronously at the top of `deactivate()` achieves this, since Fcitx5 flushes the display update before sending the commit signal to the client.
- Zen's per-keystroke activate/deactivate means `_lastProg` tracking must compare by value, not pointer — `ic->program()` returns a `const std::string &` which is stable.
- Do not remove the `commitString("")` guard — it is still needed for any Wayland IM client that does not respect `DontCommit` but also does not auto-commit (belt-and-suspenders).
- `resetInferenceOnly()` must increment `_gen` so in-flight token callbacks from the previous request are dropped on dispatch.

**Commit:** `fix(fcitx5): browser compat — Zen context reset + Chromium focus-out commit`

---

## Phase C — Packaging & Polish

### [ ] Session 14 — Model upgrade + cursor-marker prompt (anti-loop)

**Files:** `fcitx5-engine/src/inference_client.cpp`, `fcitx5-engine/src/config.h`

**Context:** The 0.5b model echoes recently accepted text because it sees its own output in the context buffer and treats it as a pattern to continue. Two root-cause fixes: (1) upgrade to qwen2.5:1.5b, which reliably follows "don't repeat" instructions — the 0.5b model cannot; (2) append a cursor-marker token (`[CURSOR]`) to the end of the prompt so the model has an unambiguous signal of exactly where generation should start, preventing it from regenerating anything before the marker.

**Objectives:**
1. Pull the new model: `ollama pull qwen2.5:1.5b`
2. Update `config.h` default: `model = "qwen2.5:1.5b"`, increase `debounce_ms` to 220 to absorb the extra latency
3. In `build_payload()`, append the marker to the context before JSON-encoding it:
   ```cpp
   std::string markedContext = context + "[CURSOR]";
   ```
4. Update `SYSTEM_PROMPT` to reference the marker:
   ```
   "Complete the text at [CURSOR]. Output ONLY what follows [CURSOR], never reproduce text before it. Be concise."
   ```
5. Switch the endpoint from `/api/generate` to `/api/chat` — Qwen2.5 is instruction-tuned using the ChatML template; the chat endpoint activates it, while generate bypasses it and produces worse instruction-following:
   - URL: `_cfg.ollama_host + "/api/chat"`
   - Request format: `{"model":"...","messages":[{"role":"system","content":"..."},{"role":"user","content":"...text...[CURSOR]"}],"stream":true,"options":{...}}`
   - Drop the top-level `"system"` and `"prompt"` keys from `build_payload`; the system message now lives in `messages[0]`
   - The `"options"` block (`num_predict`, `temperature`, `top_p`, `stop`) stays at the top level unchanged
6. Update the streaming response parser — `/api/chat` emits `"message":{"role":"assistant","content":"token"}` instead of `"response":"token"`:
   - In `stream_cb`, change `json_str(line, "response")` → `json_str(line, "content")`
   - This works because `"content":"` appears exactly once per chat stream line; no nested-JSON parser needed
   - Verify with `iclog` that tokens arrive correctly before removing debug output
7. Remove the per-request debug logging added during session 13 (`"stream_cb: raw=..."`, `"stream_cb: line=..."`) — these were diagnostic and are now noise; keep only `"execute:"`, `"run: dispatching"`, and error lines
8. Verify `health_check()` still works — it searches `/api/tags` body for `'"' + model + '"'`; no code change needed, just confirm the new model name appears after pulling
9. Test anti-loop: type a sentence, accept the suggestion with Tab, type more — the next suggestion must not open with words already visible in the buffer

**Gotchas:**
- `/api/chat` `stop` tokens go inside `"options"`, not as a top-level `"stop"` array — verify the payload builder puts them in the right place
- `json_str(line, "content")` will correctly find `"content":"` inside the nested `"message"` object because our flat string search doesn't care about nesting depth; it will not accidentally match other fields because no other top-level field is named `"content"` in the Ollama chat stream format
- The final `done:true` line in chat format is `{"message":{"role":"assistant","content":""},"done":true,...}` — `json_str(line, "content")` returns `""`, which is already handled by the `if (!token.empty())` guard in `stream_cb`
- If latency is still too high at 1.5b, try `qwen2.5-coder:1.5b` — it is specifically trained on text completion tasks and may loop less on prose too

**Commit:** `feat(fcitx5): qwen2.5:1.5b + [CURSOR] marker prompt — anti-loop inference redesign`

---

### [ ] Session 15 — PKGBUILD + systemd + AUR

**Files:** `packaging/PKGBUILD`, `packaging/xtype-linux.service`, `packaging/org.xtype.linux.desktop`

**Objectives:**
- PKGBUILD: depends on `fcitx5`, `ollama`, `python`, split package (ibus-dev + fcitx5-prod)
- systemd user service: starts Ollama, `WantedBy=default.target`
- `.desktop` file for settings entry
- `makepkg -si` succeeds cleanly on CachyOS

**Commit:** `feat(packaging): PKGBUILD + systemd service + AUR-ready`

---

### [ ] Session 16 — Settings UI + per-app profiles

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

1. **Focus-out MUST NOT commit AI ghost text** — use `TextFormatFlag::DontCommit` on the client preedit; committing on focus-out inserts text the user never accepted
2. **Never call IBus/Fcitx5 APIs from the inference thread** — always marshal to main thread
3. **Fcitx5 on KDE Wayland** — must be launched by KWin, not autostarted
4. **Chromium/Electron** — need flags/env vars for text-input-v3 preedit support
5. **Terminals (Konsole, Alacritty)** — blocklist them, preedit causes visual glitches
