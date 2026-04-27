# Sub-session U1b — Primitives + backend (Steps 5, 6, 7)

> **Scope:** Steps 5–7 (3 commits). Hand-rolled QML primitive library,
> `ConfigStore` C++ backend with TOML round-trip + tests, `EngineProbe`
> + `Reloader` + reload banner + sidebar metric wiring.
>
> **Prerequisite:** U1a complete on `main` (verify
> `ls settings/qt6-app/qml/Theme.qml` and `ls fcitx5-engine/src/engine_metrics.h`).
>
> **Deliverable at end:** every prototype primitive renders pixel-close
> to the React reference; `ConfigStore` round-trips
> `~/.config/xtype/config.toml` with Catch2 tests passing; sidebar
> CPU/RAM/state bars react to live ollama and fcitx5 changes; reload
> banner appears after edits and clears on click.
>
> **Reference:** `00-shared.md` §3 (repo layout), §4 #2/#3/#8/#9/#12/
> #13/#16, §8 gotchas. **Do not** load `U1a.md` or `U1c.md`.

---

## Step 5 — Primitives library

Files: every `qml/primitives/X*.qml` listed in §3 that wasn't built
in U1a Step 4 (the four already there — `XTitleBar`, `XSidebar`,
`XEngineDot`, `XMiniBar` — stay).

For each primitive, port the matching CSS rule from `prototype.css`
and the matching JSX from `proto-primitives.jsx`. Concrete map:

| QML | Source CSS | Source JSX | Notes |
|-----|------------|------------|-------|
| `XToggle` | `.toggle`, `.toggle.on`, `.toggle::after` | `Toggle` | 36×20 rounded with sliding 14×14 dot. `Behavior on x { NumberAnimation { duration: 220; easing: Theme.ease.bouncy } }`. |
| `XSlider` | `.slider*` | `Slider` | 4 px track, gradient fill, 14 px knob with purple ring, 56-px-min mono value label on right. Live `value`, debounced `committed` (see §4 #16). |
| `XPill` | `.pill`, `.pill.on`, `.pill.add` | `Pill` | Radius 999, 1 px border, three states (idle/on/add-dashed). Pure click toggle. |
| `XSegmented` | `.segmented*` | inline JSX | Container `Row` of `Rectangle` children with one selected. |
| `XSection` | `.section*` | `Section` | 10.5 px uppercase letter-spaced title with optional purple mono "num" badge and a fading horizontal rule. |
| `XRow` | `.row*` | `Row` | label + desc on left, control slot on right, 1 px ink-06 bottom border (none on `:last-child` — implement with index check). |
| `XBars` | `.bars*` | `Bars` | Repeater of 7 `Rectangle`s; last bar gets `Theme.purple` + glow; height-percent driven by an `animateKey` to retrigger. |
| `XButton` | `.btn*` | inline JSX | Variants: default / primary / ghost. Hover/pressed states. |
| `XInput` | `.input*` | inline `<input>` | Wraps QQC2 `TextField` with custom `background:` and `placeholderText`. Mono font. Focus state matches `.input:focus`. |
| `XCard` | `.card`, `.card.accent` | inline | Slot-children `default property` + `accent: bool`. |
| `XAppRow` | `.app-row*` | inline | Used in PerApp + PageModel. `selected` prop draws purple border + tinted background. |
| `XAppIcon` | `.app-icon*` | inline | 32×32 rounded square. Six gradient presets (`kate / tb / kons / fox / disc / kde`) keyed by `cls` prop. |
| `XKbd` | `.kbd` | inline | Small chip with bottom-thicker border. Used in PageGeneral header "Tab to accept". |
| `XPhraseRow` | `.phrase-row*` | inline | Mono text + × icon button. |
| `XLogList` | `.logs*` | inline | `ListView` with mono 11 px lines, optional `accent`/`err` colour by line classification. Auto-scroll-to-bottom. |

Each primitive: ~30–60 lines. Total ~600 lines of QML.

Apply §4 #13 to every interactive primitive: accept `enabled: bool`
and `comingSoon: bool`. `comingSoon=true` lowers `opacity: 0.45`,
sets `Accessible.description: "Not yet wired into the engine"`, but
the `MouseArea` still fires `onChange`.

Apply §4 #16 to `XSlider`: live `value` updates on every drag tick;
emit `committed(v)` only on mouse release OR after 500 ms of no
movement.

Dev loop hint: `qml -I qml qml/primitives/Demo.qml` reloads on file
save with no ninja step. Use this for shapes / animations; switch
back to `ninja -C build` only when wiring through `Config.*` (Step
6).

Acceptance: build a single `qml/primitives/Demo.qml` (gitignored —
manual test only) that shows every primitive in a column. Visually
diff against `prototype/index.html` opened in a browser: same colours,
shapes, animation timings, hover affordances.

Commit: `feat(settings): primitive library (toggle, slider, pill, segmented, row, bars, …)`.

---

## Step 6 — `ConfigStore` C++ backend + round-trip tests

Files: `src/config_store.{h,cpp}`,
`settings/qt6-app/tests/test_config_store.cpp`,
`settings/qt6-app/tests/CMakeLists.txt`,
`settings/qt6-app/data/fixtures/config_default.toml`,
`settings/qt6-app/data/fixtures/config_legacy_tab_accepts.toml`.

`ConfigStore` is a `QObject` subclass exposed as a singleton via
`qmlRegisterSingletonInstance(&store, "XType.Settings", 1, 0, "Config")`
in `main.cpp` (added to `qt_add_executable` source list now).

Properties (≈ 40 — selected here; full list mirrors `XTypeConfig`
exactly):

```cpp
class ConfigStore : public QObject {
    Q_OBJECT
    // Behaviour
    Q_PROPERTY(bool engineEnabled READ engineEnabled WRITE setEngineEnabled NOTIFY engineEnabledChanged)
    Q_PROPERTY(QString triggerMode READ triggerMode WRITE setTriggerMode NOTIFY triggerModeChanged)
    Q_PROPERTY(QString acceptKey READ acceptKey WRITE setAcceptKey NOTIFY acceptKeyChanged)
    Q_PROPERTY(bool partialAccept READ partialAccept WRITE setPartialAccept NOTIFY partialAcceptChanged)
    Q_PROPERTY(bool escDismisses READ escDismisses WRITE setEscDismisses NOTIFY escDismissesChanged)
    Q_PROPERTY(QStringList blocklistApps READ blocklistApps WRITE setBlocklistApps NOTIFY blocklistAppsChanged)
    Q_PROPERTY(QStringList blockedPhrases READ blockedPhrases WRITE setBlockedPhrases NOTIFY blockedPhrasesChanged)
    // Inference
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int debounceMs READ debounceMs WRITE setDebounceMs NOTIFY debounceMsChanged)
    Q_PROPERTY(int numPredict READ numPredict WRITE setNumPredict NOTIFY numPredictChanged)
    Q_PROPERTY(int contextWindow READ contextWindow WRITE setContextWindow NOTIFY contextWindowChanged)
    Q_PROPERTY(QVariant threads READ threads WRITE setThreads NOTIFY threadsChanged) // null = unset
    // Learning
    Q_PROPERTY(bool learningEnabled …)
    Q_PROPERTY(int maxCorpusMb …)
    Q_PROPERTY(int minSentenceChars …)
    // UserPrompt
    Q_PROPERTY(QString userDescription …)
    Q_PROPERTY(QString userTone …)
    Q_PROPERTY(QStringList userAvoidPhrases …)
    // Apps map
    Q_PROPERTY(QVariantMap apps …)              // {program: {enabled, mode, num_predict, …}}

public slots:
    void resetAll();
signals:
    void changed();                              // any field changed → debounce save
    void saved();                                // toml write succeeded
    void saveFailed(QString reason);
    // … plus per-property xChanged signals listed above …
};
```

Per-field `xChanged` signals are mandatory (see §8 "`Q_PROPERTY` notify
signals must be unique"). The `changed()` signal exists only to drive
the debounce timer.

`load()` parses `~/.config/xtype/config.toml` via toml++. Missing keys
fall back to `XTypeConfig{}` defaults. Legacy `tab_accepts_word` is
read into `partialAccept` and a `qWarning()` is emitted (matches the
engine's same fallback in U1a Step 1).

`save()` writes via `QSaveFile` (atomic temp-file + rename) and emits
`saved()`. A `QTimer` with 500 ms interval — restarted on every
`setX` call — coalesces rapid setter calls. `resetAll()` bypasses the
timer and backs up first (§4 #20):

1. Write `config.toml.bak.<unix-ts>` (full copy of current file).
2. Write a fresh TOML from `XTypeConfig{}` defaults.
3. Show a 4 s toast with the backup path.
4. Mark the reload banner.

Header comment on every save: `# Generated by xtype-settings — manual
edits are preserved across sessions but inline comments are not.`

`std::optional` round-trip (§4 #9): unset `optional` ⇒ key absent in
TOML. `setThreads(QVariant())` clears the key. `setApps(...)` with a
sub-map missing a field ⇒ that field is absent for that program.

QML usage:

```qml
XToggle  { on: Config.engineEnabled; onChange: Config.engineEnabled = checked }
XSlider  { value: Config.debounceMs; min: 100; max: 800; onCommitted: v => Config.debounceMs = v }
```

### Round-trip tests (mandatory)

Hook into the existing Catch2 setup — re-use `find_package(Catch2 3
REQUIRED)` from `fcitx5-engine/CMakeLists.txt`. Guard with
`option(BUILD_TESTS "" OFF)` in the qt6-app CMake.

Cases in `tests/test_config_store.cpp`:

1. Load `data/fixtures/config_default.toml` → assert every
   `XTypeConfig{}` default matches.
2. Load → mutate every `Q_PROPERTY` once → save → reload → assert
   all values survive byte-for-byte (mod the header comment).
3. Load `data/fixtures/config_legacy_tab_accepts.toml` (contains
   `tab_accepts_word = false`) → assert `partialAccept == false` and
   exactly one `qWarning` was emitted.
4. Save with `apps = {"kate": {enabled: true}}` → reload → assert
   `apps["kate"].enabled` is `true`, all other `AppOverride` fields
   absent (not `null`, not default).
5. `resetAll()` → backup file `config.toml.bak.<ts>` exists → main
   file matches `config_default.toml` mod the header comment.

Run: `ctest --test-dir build` from `settings/qt6-app/`.

Acceptance: edit any QML control → file at
`~/.config/xtype/config.toml` is rewritten within ~600 ms; restart
app → control reads back the saved value; `git diff` of the toml
shows only the changed key (mod the header comment); all 5 tests
pass.

Commit: `feat(settings): config store with toml round-trip + atomic save + tests`.

---

## Step 7 — `EngineProbe` + `Reloader` + sidebar wiring

Files: `src/engine_probe.{h,cpp}`, `src/reloader.{h,cpp}`,
`qml/primitives/XSidebar.qml` (update — currently shows zeros from
U1a Step 4), `qml/App.qml` (add reload banner).

`EngineProbe` polls every 1500 ms and exposes:

```cpp
Q_PROPERTY(int cpuPct READ cpuPct NOTIFY cpuPctChanged)
Q_PROPERTY(double ramGb READ ramGb NOTIFY ramGbChanged)
Q_PROPERTY(QString state READ state NOTIFY stateChanged)        // "ready"|"paused"|"error"
Q_PROPERTY(QString stateMessage READ stateMessage NOTIFY stateMessageChanged)
```

Implementation:
- CPU%: read `/proc/<ollama-pid>/stat` (find pid via `pgrep -x
  ollama` cached 60 s); compute delta against last sample. If ollama
  not running → 0. (`XTYPE_DEMO_METRICS=1` env var optionally samples
  `/proc/self/stat` instead, for designer demos — production stays
  honest.)
- RAM MB: `/proc/<ollama-pid>/status` `VmRSS` line / 1024 → GB.
- State: `fcitx5-remote -s` exit code via `QProcess::start()` +
  800 ms `waitForFinished` (§8 "fcitx5-remote -s blocks if fcitx5 is
  starting up"). Zero exit = `ready`; non-zero = `paused`; binary
  missing = `error` with message "fcitx5 not found". Coalesce with
  the procfs walk so each tick is one syscall sweep.

Singleton-registered as `Engine`:
`qmlRegisterSingletonInstance(&probe, "XType.Settings", 1, 0, "Engine")`.

`Reloader::reload()` runs `QProcess::startDetached("fcitx5-remote",
{"-r"})` and emits `reloadStarted` / `reloadFinished(bool ok)` after
the process exits. Singleton-registered as `Reloader`.

`ReloadCenter` QML singleton (in `qml/ReloadCenter.qml`):

```qml
pragma Singleton
import QtQuick
import XType.Settings 1.0

QtObject {
    id: reloadCenter
    property bool dirty: false
    property bool inflight: false
    Connections { target: Config; function onSaved() { reloadCenter.dirty = true } }
    Connections { target: Reloader; function onReloadStarted() { reloadCenter.inflight = true } }
    Connections { target: Reloader; function onReloadFinished(ok) {
        reloadCenter.inflight = false
        if (ok) reloadCenter.dirty = false
    } }
}
```

Reload banner in `App.qml`: a fixed-bottom `Rectangle` with
`Behavior on y { NumberAnimation { duration: 200 } }`. Visible iff
`ReloadCenter.dirty`. Click → `Reloader.reload()`. If
`reloadFinished(false)`, banner text becomes "Reload failed — check
logs" with a retry button (§4 #12).

Sidebar mini-bars now bind:
- `XMiniBar { value: Engine.cpuPct }` (replaces the 0 placeholder).
- `XMiniBar { value: Engine.ramGb / 4.0 * 100 }`.
- Engine running line text:
  `Engine.state === "ready" ? "engine running" : Engine.state`.

Acceptance:
- Launch `ollama serve` → CPU/RAM bars react within 1.5 s.
- Stop ollama → bars drop to 0.
- `pkill fcitx5` → engine dot dims, state text becomes "paused".
- Edit a slider → reload banner appears → click → fcitx5 reloads
  (verify with `journalctl --user -u plasma-fcitx5`) → banner clears.
- No app freezes during fcitx5 startup (`fcitx5-remote -s` has a
  `waitForFinished` timeout).

Commit: `feat(settings): engine probe + reloader + live sidebar metrics`.

---

## End of U1b — before next sub-session

1. Verify three commits on the branch:
   ```
   git log --oneline -n3
   # feat(settings): primitive library …
   # feat(settings): config store with toml round-trip + atomic save + tests
   # feat(settings): engine probe + reloader + live sidebar metrics
   ```
2. Run `ctest --test-dir settings/qt6-app/build` — all 5 tests green.
3. Push to origin.
4. **For the next session, re-read only `00-shared.md` and `U1c.md`.**
   Do not load this file or `U1a.md`.
