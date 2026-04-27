# XType — Project History

> Archive of completed development sessions. This file is **not loaded by Claude Code by
> default** — it exists as a reference for when a current session needs historical context.
> Active work is tracked in `PLAN.md`.

## Project milestones

| Phase | What it delivered | Sessions |
|---|---|---|
| **Phase 0** | Ollama client, ContextBuffer, Debouncer foundations | 1–3 |
| **Phase A** | IBus prototype validated on KDE Wayland (Python) | 4–8 |
| **Phase B** | Fcitx5 production engine (C++17) with browser compat | 9–14 |
| **Phase D** | Personalization: corpus collector, style profile, dynamic prompt | 15–17 |
| **Phase U** | Qt6/QML settings app — full UI prototype | U1a–U1c |
| **Phase E** | Engine-UI gap closure — config loader, all controls wired end-to-end | E0–E6 |

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

---

### [x] Session 14 — Model upgrade + anti-loop inference redesign

**Files:** `fcitx5-engine/src/inference_client.h`, `fcitx5-engine/src/inference_client.cpp`, `fcitx5-engine/src/xtype.cpp`

**Objectives:**
- Switch default model from `qwen2.5:0.5b` to `qwen2.5:1.5b` for materially stronger suggestions
- Replace single-turn `/api/generate` with chat-format `/api/chat` using assistant-prefill: the beginning of the user's current context is injected as the assistant's partial reply, so the model continues it rather than echoing from token 0
- Stop token strategy tightened to prevent runaway multi-sentence generation

**Outcome:** Ghost text quality improved significantly. The assistant-prefill pattern eliminates the echo/repetition loop that appeared with the larger model on plain `generate` prompts. Sessions 15–17 build on this prompt structure.

**Commit:** `feat(fcitx5): qwen2.5:1.5b + assistant-prefill chat format — anti-loop inference redesign`

---

## Phase D — Personalization Engine

### [x] Session 15 — Writing corpus collector

**Files:** `fcitx5-engine/src/corpus_collector.h`, `fcitx5-engine/src/corpus_collector.cpp`, `fcitx5-engine/src/config.h` (extend)

**Objectives:**
- `CorpusCollector` runs a background thread; `record()` is non-blocking — pushes text to a bounded deque (cap 1000), flushed every 60 s or when queue exceeds 100 entries
- Sentence-level filters at record time: drops code-shaped patterns (`[{};=]{2,}`), all-digit/punctuation runs, fragments under `min_sentence_chars`
- Hard-coded blocklist in `corpus_collector.cpp` permanently excludes password managers regardless of user config: `keepassxc`, `1password`, `bitwarden`, `gnome-keyring`, `seahorse`
- Corpus size cap: atomic rename rotation (`corpus.txt` → `corpus.txt.1`) when file exceeds `max_corpus_mb`
- Only characters from `appendChar` feed the corpus — `acceptNextWord`/`acceptAll` output is explicitly excluded so AI suggestions never enter training data
- Opt-in by default (`learning.enabled = false`)

**Commit:** `feat(personalization): writing corpus collector with opt-in logging + privacy guards`

---

### [x] Session 16 — Style profile extraction

**Files:** `fcitx5-engine/src/style_profile.h`, `fcitx5-engine/src/style_profile.cpp`, `fcitx5-engine/tests/test_style_profile.cpp`

**Objectives:**
- `StyleProfile::loadFromCorpus()` streams the corpus line-by-line (no full-file slurp); selects 3–5 length-varied exemplar sentences using a seeded RNG for test determinism
- Privacy filters applied before exemplar selection: rejects email-like patterns, sequences of ≥5 digits, sentences containing `password`/`secret`/`token`/`auth` (case-insensitive)
- `serialize()`/`deserialize()` JSON round-trip persisted at `~/.local/share/xtype/style_profile.json`
- Staleness check: profile regenerated only when corpus mtime > `last_updated + 30 min` or 200+ new lines added — avoids CPU churn during heavy typing
- Full Catch2 test suite covering sampling, privacy filters, serialize/deserialize round-trip, and empty-corpus safety

**Commit:** `feat(personalization): style profile extraction with privacy filtering + tests`

---

### [x] Session 17 — Dynamic system prompt + observability

**Files:** `fcitx5-engine/src/prompt_builder.h`, `fcitx5-engine/src/prompt_builder.cpp`, `fcitx5-engine/src/inference_client.h/.cpp`, `fcitx5-engine/src/xtype.h/.cpp`

**Objectives:**
- `buildSystemPrompt()` assembles in layers: base instruction → user description (About the user) → style exemplars → avoid phrases
- Token budget enforced: assembled prompt > 2000 chars triggers drop-longest-exemplar-first until within budget; truncation is logged as a warning
- Profile refresh timer fires every 5 minutes via `eventDispatcher().schedule()` on the main thread — background regeneration, main-thread swap
- `InferenceClient::set_system_prompt()` mutex-guarded for safe updates between in-flight requests
- Manual test matrix verified end-to-end on KDE Wayland and documented

**Commit:** `feat(personalization): dynamic system prompt with style examples + profile refresh`
**Commit:** `feat(personalization): observability + manual test matrix for live verification`

---

## Phase U — Qt6/QML Settings App

### [x] Session U1a — Engine scaffold + Qt6 window chrome

**Files:** `fcitx5-engine/src/engine_metrics.h`, `fcitx5-engine/src/recent_events.h`, `settings/qt6-app/CMakeLists.txt`, `settings/qt6-app/src/main.cpp`, `settings/qt6-app/qml/App.qml`, `settings/qt6-app/qml/Theme.qml`, `settings/qt6-app/qml/primitives/XSidebar.qml`, `settings/qt6-app/qml/primitives/XTitleBar.qml`

**Objectives:**
- Engine scaffolded with `EngineMetrics` and `RecentEvents` structs that the settings app will read from JSON (written by the engine in Phase E)
- Qt6/QML project set up: CMakeLists, `main.cpp`, Inter + JetBrains Mono variable fonts bundled
- `Theme` QML singleton: color palette, typography scale, grain texture provider for background
- Window chrome: frameless `XTitleBar` (draggable), `XSidebar` (navigation + live metrics panel), `App.qml` content stack

**Commit:** `feat(engine): scaffold config + metrics + recent-events for settings UI`
**Commit:** `feat(settings): qt6 project scaffold + main + cmake`
**Commit:** `feat(settings): theme singleton + grain provider + bundled fonts`
**Commit:** `feat(settings): window chrome + sidebar + titlebar`

---

### [x] Session U1b — Primitives + config store + engine probe

**Files:** `settings/qt6-app/qml/primitives/` (full library), `settings/qt6-app/src/config_store.h/.cpp`, `settings/qt6-app/src/engine_probe.h/.cpp`, `settings/qt6-app/src/reloader.h/.cpp`, `settings/qt6-app/tests/test_config_store.cpp`

**Objectives:**
- Complete QML primitive library: `XToggle`, `XSlider`, `XPill`, `XSegmented`, `XRow`, `XCard`, `XSection`, `XBars`, `XButton`, `XInput`, `XKbd`, `XLogList`, `XPhraseRow`, `XMiniBar`, `XAppRow`, `XEngineDot`
- `ConfigStore` C++ backend: all `XTypeConfig` fields exposed as QML properties; TOML round-trip via toml++; atomic save (write `.tmp` → rename); 8 unit tests all green
- `EngineProbe` polls `metrics.json` and `recent_events.json` on a timer and emits change signals to the sidebar
- `Reloader` issues `fcitx5-remote -r` and surfaces the result to a reload banner in the UI
- Fixed `ReloadCenter` root type and fcitx5 state detection logic

**Commit:** `feat(settings): primitive library (toggle, slider, pill, segmented, row, bars, …)`
**Commit:** `feat(settings): config store with toml round-trip + atomic save + tests`
**Commit:** `feat(settings): engine probe + reloader + live sidebar metrics`

---

### [x] Session U1c — Full page suite + KDE integration

**Files:** `settings/qt6-app/qml/pages/` (all six pages), `settings/qt6-app/data/xtype-settings.desktop`, UI layout fixes throughout

**Objectives:**
- Six pages implemented from the HTML/JSX design reference: PageGeneral, PageBlockList, PagePerApp, PageModel, PagePersonalisation, PageAbout
- KDE `.desktop` entry registers `xtype-settings` in application menus
- Several layout issues resolved after initial port: `Flickable` content area had zero width (added `anchors.fill`), `pixelSize` required an explicit int cast, `Gradient.Horizontal` not valid in Qt6 QML

**Outcome:** `xtype-settings` binary runs standalone and displays all pages. The engine ↔ UI filesystem bridge is functional: settings written by the app are read by the engine after `fcitx5-remote -r`.

**Commit:** `feat(settings): pages — general, blocklist, per-app, model, personalisation, about`
**Commit:** `feat(settings): qt6/qml standalone settings app — full prototype port`

---

## Phase E — Engine-UI Gap Closure

> **Root cause:** `XTypeEngine` was default-constructing `XTypeConfig` and never reading `~/.config/xtype/config.toml`. The settings app wrote TOML correctly; the engine ignored it. Sessions E0–E6 close every gap between what the UI exposes and what the engine applies.

### [x] Session E0 — Engine TOML config loader

**Files:** `fcitx5-engine/src/config_loader.h`, `fcitx5-engine/src/config_loader.cpp`, `fcitx5-engine/src/toml/toml.hpp` (vendored), `fcitx5-engine/tests/test_config_loader.cpp`

**Objectives:**
- `ConfigLoader::load(path)` reads all `XTypeConfig` sections from TOML via toml++; missing keys fall back to `XTypeConfig{}` defaults without error
- `XTypeEngine::reloadConfig()` override calls loader and re-applies every field
- Legacy migration: `tab_accepts_word = false` → `partial_accept = false` with a logged warning
- 5 loader tests: missing file, malformed TOML, full round-trip, empty file, legacy migration

**Outcome:** Ghost text was simultaneously broken by empty stop-token strings being forwarded to Ollama. Fixed alongside E0: empty entries are now filtered from the stop-token list before payload construction. Also discovered that `fcitx5-remote -r` alone does not call `reloadConfig()`; the addon conf must declare `Configurable=True` for the reload signal to reach the engine.

**Commit:** `feat(engine): load config.toml on startup and reload`
**Commit:** `fix(engine+settings): ghost text broken by empty stop token and reload wiring`

---

### [x] Session E1 — Accept key wiring + phrase blocklist + threads

**Files:** `fcitx5-engine/src/xtype.cpp`, `fcitx5-engine/src/phrase_blocklist.h`, `fcitx5-engine/tests/test_phrase_blocklist.cpp`, `settings/qt6-app/qml/pages/PageGeneral.qml`, `settings/qt6-app/qml/pages/PageModel.qml`

**Objectives:**
- Accept key made configurable: Enter and → (Right) both handled correctly in `keyEvent`
- `PhraseBlocklist`: substring match, case-insensitive, reloaded on `reloadConfig()`; 9 unit tests including word-boundary cases
- Threads slider wires `num_thread` into the Ollama payload when non-zero
- Settings UI: removed `comingSoon` guards from accept-key selector and threads slider; fixed threads slider QML binding (`.valid` is always falsy on QML integers — replaced with `?? 0` null-coalescing)

**Commit:** `feat(engine): wire AcceptKey::Enter and AcceptKey::Right in keyEvent`
**Commit:** `feat(engine): implement phrase blocklist substring matching`
**Commit:** `feat(engine): include num_thread in Ollama payload when threads is configured`
**Commit:** `feat(settings): remove comingSoon from accept-key Enter/→ and threads slider`

---

### [x] Session E2 — Observability bridge + live test input

**Files:** `fcitx5-engine/src/xtype.cpp` (metrics + events writers), `fcitx5-engine/src/engine_metrics.h`, `fcitx5-engine/src/recent_events.h`, `settings/qt6-app/qml/pages/PageGeneral.qml`

**Objectives:**
- Engine writes `~/.local/share/xtype/metrics.json` and `recent_events.json` atomically (`.tmp` → rename) after each inference round-trip
- `EngineProbe` in the settings app polls both files; sidebar shows live suggestions/min, average latency, corpus line count, and recent event list
- PageGeneral live test input: replaced the static typewriter animation with a real `XInput` field that sends text to the running engine and displays its suggestion inline

**Commit:** `feat(observability): bridge engine metrics + recent events to settings app`
**Commit:** `feat(settings): replace ghost-text typewriter with live test input on General page`

---

### [x] Session E3 — User voice profile

**Files:** `fcitx5-engine/src/xtype.cpp` (`buildSystemPrompt`), `settings/qt6-app/qml/pages/PagePersonalisation.qml`

**Objectives:**
- Engine reads `user_prompt.description`, `user_prompt.tone`, and `user_prompt.avoid_phrases` from config and injects them into the assembled system prompt
- Description hard-capped at 500 chars in `applyPrompt()` (engine) and via `maximumLength` on the QML textarea (settings) to protect the 2000-char prompt budget
- Personalisation page: description textarea, tone segmented control, avoid-phrases pill list with add/remove

**Commit:** `feat(engine): wire user_prompt config and voice_strength into system prompt`
**Commit:** `feat(settings): user voice profile UI — description, tone, avoid phrases`

---

### [x] Session E4 — Voice strength slider

**Files:** `fcitx5-engine/src/style_profile.cpp`, `settings/qt6-app/qml/pages/PagePersonalisation.qml`

**Objectives:**
- `voice_strength` (0.0–1.0) proportionally scales how many corpus-derived style exemplars are included in the system prompt: 0 suppresses all exemplars, 1 uses the full allocation
- UI slider uses `onCommitted` to debounce saves — no TOML write on every frame

**Commit:** `feat(engine): proportional voice_strength exemplar control`
**Commit:** `feat(settings): enable voice-strength slider`

---

### [x] Session E5 — Corpus rolling window

**Files:** `fcitx5-engine/src/corpus_collector.cpp`, `settings/qt6-app/qml/pages/PagePersonalisation.qml`

**Objectives:**
- `forget_after_days`: entries in `corpus.txt` older than N days are pruned on each background flush; 0 disables pruning
- `pruneOldEntries()` rewrites only the lines that pass the age check — no full-file load
- Personalisation page: toggle + day-count input wired through `ConfigStore`

**Commit:** `feat(engine): corpus rolling window for forget_after_days`
**Commit:** `feat(settings): enable forget-after-days toggle`

---

### [x] Session E6 — Per-app config overrides + advanced inference settings

**Files:** `fcitx5-engine/src/xtype.cpp`, `settings/qt6-app/qml/pages/PagePerApp.qml`, `settings/qt6-app/qml/pages/PageModel.qml`, `settings/qt6-app/qml/pages/PagePersonalisation.qml`, `settings/qt6-app/src/config_store.h/.cpp`

**Objectives:**
- Engine reads `[apps.X]` TOML subtables and overlays per-app suggestion length and completion mode at `activate()` time
- Per-App page: each known app row exposes suggestion-length and mode controls
- Advanced inference knobs added to `ConfigStore` and wired through `PageModel`: temperature, top_p, stop tokens
- Corpus settings section added to `PagePersonalisation`
- `comingSoon` guards removed from Trigger-mode and Quantisation controls (not yet implemented; controls removed entirely rather than left dimmed)

**Commit:** `feat(engine): apply per-app config overrides in keyEvent and requestInference`
**Commit:** `feat(settings): enable per-app suggestion-length and mode controls`
**Commit:** `feat(settings): ConfigStore — temperature, top_p, stop_tokens, corpus knobs`
**Commit:** `feat(settings): PageModel advanced inference section — temperature, top_p, stop_tokens`
**Commit:** `feat(settings): PagePersonalisation corpus settings section`

---

### Pre-release fixes

- `isBlocked` word-boundary check: prevents partial app-name matches from over-blocking (e.g. `konsole` no longer blocks `org.kde.konsole.desktop` substrings unintentionally)
- Typed-buffer overflow preserves sentence boundaries: when the 500-char buffer fills, the oldest whole sentence is dropped rather than splitting mid-word
- Inference log separated from `fcitx5.log`: engine now writes to `~/.local/share/xtype/inference.log` so the settings app `LogTail` can show inference activity independently
- User description capped at 500 chars in both engine (`applyPrompt`) and settings (`maximumLength`) to protect prompt budget

**Commit:** `fix(engine): isBlocked word-boundary check prevents over-blocking`
**Commit:** `fix(engine): typed-buffer overflow preserves sentence boundaries`
**Commit:** `fix(engine): move inference log to ~/.local/share/xtype/inference.log`
**Commit:** `fix(engine/settings): cap user description at 500 chars to protect prompt budget`
