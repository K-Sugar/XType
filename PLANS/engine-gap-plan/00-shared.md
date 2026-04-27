# 00 — Shared reference (engine-gap-plan)

Read this file at the start of every session in this plan. It holds
cross-cutting facts that all sessions depend on.

---

## §1. Root cause

`XTypeEngine::_cfg` (type `XTypeConfig`) is default-constructed and
**never read from `~/.config/xtype/config.toml`**. The settings UI writes
TOML correctly; the engine ignores it. Every session in this plan flows
from E0 closing that gap.

Grep confirms: no `toml`, `config.toml`, or `loadConfig` symbol exists
anywhere under `fcitx5-engine/src/`. Check before proceeding if in
doubt: `grep -r "toml" fcitx5-engine/src/` must return zero results to
validate the root cause still applies.

---

## §2. Architecture of the cross-process bridge

The settings app and the fcitx5 engine are **separate processes**. They
share state only via the filesystem. Never attempt IPC, sockets, or
direct API calls between them.

```
xtype-settings (Qt app)               fcitx5-engine (fcitx5 addon)
──────────────────────                 ────────────────────────────
ConfigStore ──write──▶ config.toml ◀──read── ConfigLoader (E0)
EngineProbe ◀──read── metrics.json ◀──write── MetricsWriter (E2)
RecentEventsModel ◀─── recent_events.json ◀── EventsWriter (E2)
LogTail ◀─────────── fcitx5.log ◀──────────── dbg() calls (existing)
CorpusStats ◀─────── corpus.txt ◀─────────── CorpusCollector (existing)
StyleProfileLoader ◀─ style_profile.json ◀── StyleProfile (existing)
```

**File paths (canonical):**

| File | Path | Writer | Reader |
|------|------|--------|--------|
| `config.toml` | `~/.config/xtype/config.toml` | settings app | **engine (after E0)** |
| `metrics.json` | `~/.local/share/xtype/metrics.json` | **engine (after E2)** | settings app |
| `recent_events.json` | `~/.local/share/xtype/recent_events.json` | **engine (after E2)** | settings app |
| `fcitx5.log` | `~/.local/share/xtype/fcitx5.log` | engine | settings app (LogTail) |
| `corpus.txt` | `~/.local/share/xtype/corpus.txt` | engine | settings app (CorpusStats) |
| `style_profile.json` | `~/.local/share/xtype/style_profile.json` | engine | settings app (StyleProfileLoader) |

All engine writes use `std::filesystem` + atomic rename (write to
`.tmp`, rename over target) to prevent the settings app seeing
half-written files.

---

## §3. Config reload flow

User changes a setting → ConfigStore writes `config.toml` → reload
banner appears → user clicks Reload → `fcitx5-remote -r` fires →
fcitx5 calls `XTypeEngine::reloadConfig()` → engine re-reads TOML and
re-applies settings.

`reloadConfig()` is declared in `fcitx::AddonInstance` (base of
`InputMethodEngine`). Override it in `XTypeEngine`. It runs on the
main thread (fcitx5 guarantees this) so it may safely touch all engine
state.

---

## §4. Toml++ vendoring in the engine

The settings app already has `toml++` v3.4 at
`settings/qt6-app/src/toml/toml.hpp`. Copy that single header to
`fcitx5-engine/src/toml/toml.hpp`. Both builds vendor their own copy;
they may drift to different versions independently without risk.

`TOML_HEADER_ONLY` compile mode is the default — no `.cpp` file needed.
`#include "toml/toml.hpp"` from `config_loader.cpp`.

Add `${CMAKE_CURRENT_SOURCE_DIR}/src` to `target_include_directories`
in `fcitx5-engine/CMakeLists.txt` if not already present. Verify by
checking that `toml++` parses without errors in a unit test.

---

## §5. Build commands

```bash
# Engine
cmake -B fcitx5-engine/build -G Ninja -S fcitx5-engine
ninja -C fcitx5-engine/build
ninja -C fcitx5-engine/build test          # unit tests (Catch2)

# Settings app
cmake -B settings/qt6-app/build -G Ninja -S settings/qt6-app
ninja -C settings/qt6-app/build
ctest --test-dir settings/qt6-app/build   # unit tests
./settings/qt6-app/build/xtype-settings   # smoke run
```

---

## §6. Hard rules (carry over from root CLAUDE.md)

- Never call fcitx5 APIs from `std::thread` — use
  `_instance->eventDispatcher().schedule(...)` or
  `scheduleWithContext(...)`.
- Total system prompt ≤ 2000 chars (enforced by `buildSystemPrompt`
  budget; `kPromptBudget = 2000` in `xtype.h`).
- Corpus/profile data: local-only; never sent over the network or
  logged outside `~/.local/share/xtype/`.
- AI-generated text (suggestions) must never be written to corpus.
- Password manager apps must stay in the hard blocklist regardless of
  user config.
- `commitString("")` must be called before `resetState()` to prevent
  ghost text on focus loss.

---

## §7. Gotchas

- **`isBlocked()` substring match**: `_cfg.behaviour.blocklist_apps`
  entries are matched with `prog.find(entry)`. This correctly matches
  `org.kde.konsole` against `konsole`, but would also match a
  hypothetical `konsoleboard` app. Session E6 adds word-boundary logic.

- **`_userTypedSinceLastTerminator` overflow**: When the buffer exceeds
  `kUserBufCap = 2048`, the first half is erased
  (`erase(0, kUserBufCap / 2)`). This can break a sentence in the
  middle. Session E6 replaces the blind half-erase with a
  sentence-boundary-aware truncation: scans forward to the next `.!?`
  and erases through that character, or clears the whole buffer if no
  boundary is found.

- **inference_client.cpp log path hardcoded** to
  `/home/saint/Desktop/XType/fcitx5-engine/thread.log` (line 12). This
  file ships in `.gitignore` but the path is wrong for any other user.
  Session E6 moves it to `~/.local/share/xtype/inference.log`.

- **`applyPrompt()` does not populate `userDescription` or
  `avoidPhrases`** from `_cfg.user_prompt` even though
  `buildSystemPrompt` supports both fields. Session E3 fixes this.

- **E2.4 LiveDemo.qml did not already consume RecentEvents**: The E2 plan
  stated "LiveDemo.qml already checks RecentEvents.first()". The actual file
  used only self-contained fixture sequences. A small Connections hook was added
  to LiveDemo.qml to read real events from RecentEvents.eventsChanged and
  display `typed + ghost`; fixture cycle runs as fallback when no real events exist.

- **`reloadConfig()` vs constructor**: config is loaded in the
  constructor AND in `reloadConfig()`. After reload, the inference
  client must be told the new system prompt (`applyPrompt()`) and
  the corpus collector must be re-initialised if the corpus path
  changed. Do NOT reinitialise if learning was disabled in config but is
  running — check for actual change before teardown.

- **Full fcitx5 restart required after installing a new .so**: `ReloadAddonConfig` and
  `fcitx5-remote -r` only call `reloadConfig()` on the addon already in memory — they
  do not reload the shared library from disk. After `sudo ninja install`, run
  `fcitx5 --replace -d` to get fcitx5 to load the new binary. Symptom: the old
  behaviour persists even though the install succeeded and the config was reloaded.
  Diagnosis: `cat /proc/$(pgrep -x fcitx5)/maps | grep xtype` shows `(deleted)`.

- **comingSoon controls must still write to TOML**: the settings app
  already implements this (comingSoon lowers opacity but the MouseArea
  still fires). Do not change this behaviour when removing comingSoon.
  The engine side just starts reading the value once wired.

- **metrics.json write cadence**: write on every inference completion
  (on_done callback), but not more than once per second (gate with a
  `std::time_t last_write`). The settings app reads on its 1500 ms
  EngineProbe poll; sub-second engine writes are wasted I/O.

- **recent_events.json write cadence**: write on every push to
  `_recent`, but gate at 500 ms minimum interval (atomic timestamp
  flag). The settings app reads when it polls EngineProbe.

- **E3 QML primitive API drift** (discovered in E3):
  The E3 session plan's QML snippets reference primitive APIs that do not match
  the actual components:
  - `XSegmented` uses `options:` (not `model:`) and `selectedIndex:` (not
    `currentIndex:`). The signal name `selected(int)` is correct.
  - `XPhraseRow` emits `removeClicked()` (not `remove()`); the handler is
    `onRemoveClicked:`. Use index-based splice (pattern from PageBlockList.qml)
    rather than filter-by-value to handle duplicate phrases.
  - `XInput` does not forward `editingFinished` from the internal TextField.
    Add `signal editingFinished()` + a Connections block to XInput.qml so the
    plan's `onEditingFinished:` pattern works.
  - Repeater delegates under `pragma ComponentBehavior: Bound` must declare
    `required property string modelData` (and `required property int index` if
    index-based splice is used). Mirror the pattern in PageBlockList.qml:83-84.

---

## §8. Session ordering and dependency

```
E0 (TOML loader)
  ├─▶ E1 (accept key / phrase blocklist / threads engine)
  │     └─▶ E1-UI (remove comingSoon in settings)
  ├─▶ E2 (observability bridge)
  │     └─▶ E2-UI (replace LiveDemo with live test input)
  ├─▶ E3 (user prompt engine + UI)
  ├─▶ E4 (voice_strength / forget_after_days / per-app)
  └─▶ E6 (code quality — independent but benefits from E0 being present)
E5 (new UI controls) — depends only on ConfigStore API; can run after E0
```

Sessions E1 through E6 are all independently runnable after E0 lands.
If parallelising across agents, E2, E3, E4, E5, E6 can run concurrently
on separate branches that rebase onto E0's commit. E1-UI must follow E1.

---

## §9. Acceptance summary (per session)

| Session | Done when |
|---------|-----------|
| E0 | Changing `debounce_ms` in the UI, clicking Reload, then typing → suggestion fires after the new delay (verified in Kate/GTK app) |
| E1 | Switching accept key to Enter → Enter accepts suggestion (not just dismisses); threads slider changes `num_thread` in Ollama payload |
| E1-UI | Enter, → pills and Threads slider no longer show comingSoon opacity in xtype-settings |
| E2 | Latency display in Model page shows non-zero values after a few suggestions; LiveDemo shows real recent events when engine is active |
| E2-UI | General page shows a live text-input area; typing with XType active produces an inline underlined ghost-text suggestion; Tab accepts |
| E3 | Filling user description + clicking Reload → description appears in system prompt visible in debug log; avoid_phrases excluded from suggestions |
| E4 | Disabling XType for Kate (per-app) → suggestions stop; voice slider at 0 → no exemplars in prompt; setting forget_after_days=7 → old corpus entries pruned on next flush |
| E5 | Temperature slider at 0.9 → Ollama receives `"temperature":0.9`; max_corpus_mb at 20 → corpus trimmed to 20 MB |
| E6 | `grep -n "konsoleboard"` scenario blocked; inference.log appears at correct path; no half-sentence harvest from overflow |
