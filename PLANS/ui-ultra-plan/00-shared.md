# 00 — Shared reference (read once at session start)

This file holds the cross-cutting context every sub-session needs:
pivot summary, stack decision, architecture diagram, repo layout,
challenges-and-solutions table, acceptance criteria, out-of-scope, and
gotchas. The per-step bodies live in `U1a.md` / `U1b.md` / `U1c.md`.

---

## §0. Pivot summary — what changes vs UI-PLAN §U1

| UI-PLAN §U1 (web prototype scaffold)              | This ultra plan (Qt6 native)                            |
| ------------------------------------------------- | ------------------------------------------------------- |
| `settings/prototype/bridge.py` HTTP server        | C++ `ConfigStore` + `EngineProbe` linked into the app   |
| `settings/prototype/hooks.jsx` (`useConfig` etc.) | QML `ConfigStore` singleton + `Q_PROPERTY` bindings     |
| `settings/prototype/proto-pages.jsx` swap         | QML pages under `settings/qt6-app/qml/pages/`           |
| `settings/prototype/fixtures.json` empty-state    | Same JSON, loaded by `ConfigStore` on first start       |
| Babel-in-browser dev loop                         | `cmake -B build && ninja -C build && ./xtype-settings`  |
| Reload banner clicks `POST /api/engine/reload`    | Reload banner runs `QProcess::startDetached("fcitx5-remote", {"-r"})` |
| Localhost CORS + CSRF guards                      | None needed — process-local                             |

**Kept** from §U1: every U1.2 engine deliverable, every U1.6 acceptance
criterion (translated to Qt), every §U1.8 gotcha.

**Discarded**: Python, HTTP, fixtures-as-server, `bridge.py`,
fetch-error fallbacks, CSRF / Origin guards, the React+Babel dev loop
entirely.

---

## §1. Stack decision

**Qt 6.7+ / QML / C++20 / Qt Quick Controls 2 (minimal use)**.

Why QML, not Qt Widgets:
- The prototype relies on translucency, OKLCH gradients, animated
  toggles/sliders/pills, page transitions, blinking carets, pulsing
  dots, bar-chart growth, custom titlebar, grain overlay, blurred
  window background. These are 5–10 lines of declarative QML each and
  a custom `QStyle` subclass fight in Widgets.
- QML's `Behavior on x { NumberAnimation { … } }` matches the
  prototype's `transition: 0.16s ease` CSS one-for-one.
- `LinearGradient` / `RadialGradient` / `OpacityMask` /
  `MultiEffect.blurEnabled` cover everything in `prototype.css`
  without shaders.

Why **standalone** app, not KCModule:
- The prototype renders its own titlebar with traffic-light buttons,
  custom blur, custom window radius, edge-to-edge purple gradient.
  KCMs are embedded inside System Settings's Breeze chrome and cannot
  draw outside their tab area.
- Standalone apps still surface in System Settings via a `.desktop`
  entry under `Settings/InputMethod/`. A thin embedded KCM shell
  hosting the same QML scene can be added later as a one-session
  follow-up if needed. Out of scope here.

Verified versions on this machine: `Qt 6.11.0`. Target `Qt 6.7` as the
minimum so CachyOS, Arch, Fedora 40+, Ubuntu 24.10+ all work.
Dependencies: `Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
Qt6::QuickControls2`. **No** KF6 beyond optional `KWindowSystem` for
Wayland blur.

---

## §2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  xtype-settings (Qt6 app, ELF binary)                       │
│                                                             │
│  ┌──────────────────────────┐   ┌──────────────────────┐   │
│  │  C++ backend             │   │  QML scene           │   │
│  │                          │   │                      │   │
│  │  ConfigStore             │◀──┤  Theme.qml (sing.)   │   │
│  │  ├─ load(toml)           │   │  Primitives/        │   │
│  │  ├─ save(toml + .bak)    │   │  Pages/             │   │
│  │  └─ Q_PROPERTY x40       │   │  App.qml            │   │
│  │                          │   │                      │   │
│  │  EngineProbe (QTimer)    │──▶│  bound to QML props │   │
│  │  ├─ procfs CPU/RAM       │   │                      │   │
│  │  ├─ fcitx5-remote -s     │   │                      │   │
│  │  └─ EngineState enum     │   │                      │   │
│  │                          │   │                      │   │
│  │  OllamaClient (QProcess) │──▶│  available models   │   │
│  │  ├─ list()               │   │  active model meta  │   │
│  │  └─ activeMeta(name)     │   │                      │   │
│  │                          │   │                      │   │
│  │  LogTail (QFileSysWatch) │──▶│  ListModel of lines │   │
│  │                          │   │                      │   │
│  │  CorpusStats (QProcess)  │──▶│  sentences/words/MB │   │
│  │  └─ xtype-corpus stats   │   │                      │   │
│  │                          │   │                      │   │
│  │  StyleProfileLoader      │──▶│  exemplars/openers  │   │
│  │  (style_profile.json)    │   │                      │   │
│  │                          │   │                      │   │
│  │  Reloader                │   │                      │   │
│  │  └─ QProcess fcitx5-     │   │                      │   │
│  │     remote -r            │   │                      │   │
│  └──────────────────────────┘   └──────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
            ▼                                ▼
   ~/.config/xtype/config.toml      fcitx5-remote, /proc, ollama
   ~/.local/share/xtype/{engine.log, style_profile.json,
                         corpus.txt, .always_blocked}
```

The app is **pure read/write** against the filesystem and CLI tools.
There is no IPC with the running fcitx5 engine beyond `fcitx5-remote -r`
and `fcitx5-remote -s`.

Threading: every filesystem read happens on the GUI thread inside a
`QFutureWatcher` if it can exceed 5 ms (`xtype-corpus stats`,
`ollama list`, log tail seek to EOF). Procfs CPU/RAM is fast enough to
sample on the GUI thread inside a 1500 ms `QTimer`. Writes go through
a 500 ms `QTimer` debounce to coalesce slider drags.

---

## §3. Repo layout

```
settings/qt6-app/
├── CMakeLists.txt
├── README.md                          (1 paragraph: how to run)
├── resources.qrc                      (auto-generated via qt_add_qml_module)
├── data/
│   ├── xtype-settings.desktop         (KDE menu entry)
│   ├── icons/xtype.svg                (256×256 brand-mark)
│   ├── fonts/Inter-Variable.ttf       (bundled — see §4 #7)
│   └── fonts/JetBrainsMono-Variable.ttf
│
├── src/
│   ├── main.cpp
│   ├── config_store.{h,cpp}           (TOML round-trip; ~40 Q_PROPERTYs)
│   ├── engine_probe.{h,cpp}           (CPU/RAM/state poller)
│   ├── ollama_client.{h,cpp}
│   ├── corpus_stats.{h,cpp}
│   ├── style_profile_model.{h,cpp}
│   ├── log_tail.{h,cpp}
│   ├── reloader.{h,cpp}
│   ├── recent_events_model.{h,cpp}
│   ├── apps_known.{h,cpp}
│   ├── build_info.h.in
│   └── toml/
│       └── toml.hpp                   (vendored toml++ v3.4)
│
└── qml/
    ├── App.qml
    ├── Theme.qml                      (singleton)
    ├── primitives/
    │   ├── XToggle.qml
    │   ├── XSlider.qml
    │   ├── XPill.qml
    │   ├── XSegmented.qml
    │   ├── XSection.qml
    │   ├── XRow.qml
    │   ├── XBars.qml
    │   ├── XButton.qml
    │   ├── XInput.qml
    │   ├── XCard.qml
    │   ├── XAppRow.qml
    │   ├── XAppIcon.qml
    │   ├── XKbd.qml
    │   ├── XPhraseRow.qml
    │   ├── XLogList.qml
    │   ├── XEngineDot.qml
    │   ├── XMiniBar.qml
    │   ├── XGrain.qml
    │   ├── XTitleBar.qml
    │   └── XSidebar.qml
    └── pages/
        ├── PageGeneral.qml
        ├── PagePersonalisation.qml
        ├── PageBlockList.qml
        ├── PagePerApp.qml
        ├── PageModel.qml
        ├── PageAbout.qml
        └── LiveDemo.qml
```

CMake: `qt_add_qml_module(xtype-settings URI XType.Settings)` so
`import XType.Settings` works without copy-around tricks.

---

## §4. Challenges and solutions

| # | Challenge | Solution |
|---|-----------|----------|
| 1 | **Translucent blurred window**. Prototype's defining look is `backdrop-filter: blur(28px)` over an oklch-tinted base. Qt 6 windows are opaque by default; KWin needs to be told to blur behind. | `setFlags(Qt::FramelessWindowHint)` + `setColor(Qt::transparent)` on `ApplicationWindow`. On Wayland, request KWin blur via `KWindowEffects::enableBlurBehind(window, true)` (KF6 `KWindowSystem` is the only KF6 dep; gracefully no-op on non-KWin compositors). On X11/non-KDE, fall back to a frosted-glass backdrop drawn in QML using a tiled noise + radial gradient. **Spike result (U1a Step 2.5, Plasma 6 Wayland): `Qt::FramelessWindowHint` + `color: "transparent"` works; `KWindowEffects::enableBlurBehind` called without error; window renders transparent and stays open. KWin blur path is confirmed viable — proceed with that path. Dormant comment left in `main.cpp` for Step 4.** |
| 2 | **OKLCH colors**. CSS uses `oklch(0.62 0.17 288)` extensively; QColor speaks sRGB. | Tweaks panel is dropped (Step 9), so `theme.hue` is **build-time constant** (`288`). Precompute the ~12 theme colors once at build time from `scripts/gen-theme-colors.py` (use `culori` or Python `colour` package — **do not roll the math by hand**: OKLCh→OKLab→linear-sRGB→sRGB-gamma + gamut clip; the clip step is the trap). Output `qml/theme_colors.json`, inlined into `Theme.qml`. |
| 3 | **Custom-shaped controls**. Toggle, Slider, Pill, Segmented, Phrase row are not stock QQC2. | Hand-roll each as a QML `Item` composing `Rectangle` + `Behavior` + `MouseArea`. Total custom-control code ≤ 400 lines. QQC2 is used only for `ScrollView`, `TextField` (inside `XInput`), `ComboBox` (per-app mode select). |
| 4 | **Live demo typewriter**. `useEffect` chain in `LiveDemo` cycles 4 sequences with intra-step timeouts. | Direct port: a single `Timer` driven by an `int phase` state machine. Each phase has a duration; `onTriggered` advances `phase` and resets `interval`. |
| 5 | **Pulsing dot, blinking caret, page-in animation**. Three independent CSS keyframe animations. | Three QML `SequentialAnimation`s with `loops: Animation.Infinite`. Caret is a `Rectangle { width: 2; height: 16 }` with `Behavior on opacity` driven by a 1 s `Timer`. |
| 6 | **Grain overlay**. Prototype uses inline SVG fractalNoise as `background-image`. | Render the SVG once at startup into a `QImage` (Qt SVG renderer handles `feTurbulence`), wrap in a `QQuickImageProvider`, expose as `image://grain/desktop` and `image://grain/window`. QML uses `Image { source: "image://grain/window"; fillMode: Image.Tile; opacity: theme.grain }`. |
| 7 | **Custom fonts**. Prototype loads from Google Fonts CDN; Qt app must work offline. | Bundle Inter Variable + JetBrains Mono Variable TTFs (~700 KB) under `data/fonts/`, register at startup via `QFontDatabase::addApplicationFont(":/fonts/Inter-Variable.ttf")`. Fallback warning if missing. OFL licenses included under `data/fonts/LICENSE-*.txt`. |
| 8 | **TOML round-trip preserves comments?** `toml++` v3 reads but does not write comments. | (i) Vendor `toml++` v3.4 single-header. (ii) Write a header comment `# Generated by xtype-settings — manual edits are preserved across sessions but inline comments are not.` (iii) Always write via `QSaveFile` (atomic temp-file + rename). (iv) Backup to `config.toml.bak.<unix-ts>` before overwriting. Document once in README. |
| 9 | **`std::optional` in TOML**. `AppOverride` and `InferenceConfig::threads` use `optional`; toml++ has no first-class optional. | Convention: an unset optional means "key is absent". `ConfigStore::saveAppOverride` skips `insert` for any field where the QML side has not set the property. Round-trip test mandatory. |
| 10 | **`fcitx5-remote -s` exit code** as engine state proxy. Spec for the exit code is not stable across fcitx5 minor versions. | Treat any non-zero exit as `paused`, any zero exit as `ready`. If `fcitx5-remote` is missing, `state = "error"` with message "fcitx5 not found". Probe every 1500 ms inside `EngineProbe`. Coalesce with the CPU/RAM read so the procfs walk happens once per tick. |
| 11 | **Per-app program names**. Engine keys `XTypeConfig::apps` by lowercased program name (`kate`, `org.mozilla.thunderbird`). The prototype uses synthetic short ids. | The Qt UI shows display labels but stores keys in their canonical form. `apps_known.cpp` carries a curated map `{id → {label, canonicalProgram, iconClass, defaultMode}}`. The "+ add app" picker scans `~/.local/share/applications/*.desktop` + (optionally) `~/.local/share/xtype/seen_programs.txt`. |
| 12 | **Reload banner UX**. Sticky banner appears after any write and clears on successful reload. | A QML singleton `ReloadCenter` exposes `bool dirty` and `bool inflight`. Every `Q_PROPERTY` setter that wrote to disk emits `ReloadCenter.markDirty()`. Banner is a fixed-bottom `Rectangle` in `App.qml` with a `Behavior on y` slide. Click → `Reloader.reload()` → on `finished` → `dirty = false`. |
| 13 | **Disabled-but-persisted controls**. §U1.8 demands controls like Quantisation / Threads / Trigger=Manual / Accept=Enter\|→ / per-app non-Default modes render disabled but still write. | Each primitive accepts an `enabled: bool` and a `comingSoon: bool`. `comingSoon=true` lowers `opacity: 0.45`, sets `Accessible.description: "Not yet wired into the engine"`, but the underlying `MouseArea` still calls `onChange`. Tooltip via `ToolTip.text` and `ToolTip.visible: hovered`. |
| 14 | **Live logs feed**. `QFileSystemWatcher` is debounced — fast bursts can collide. | `LogTail` opens the log read-only with `QFile`, seeks to EOF, on `fileChanged` reads from the saved offset. If size shrunk (rotation), seek to 0. Returns lines as a `QStringListModel` capped at the most recent 200. |
| 15 | **Engine log path is split** (README mentions both `engine.log` and `debug.log`). | Per §U1.8: unify on `~/.local/share/xtype/fcitx5.log`. Engine PR writes to the new path is part of U1a Step 1 (= §U1.2). Qt app reads from the new path; falls back to `engine.log` then `debug.log`. |
| 16 | **Slider drag + debounce** vs **immediate visual feedback**. Prototype updates the displayed value live but only persists after 500 ms idle. | `XSlider` exposes `value` (live, 60 fps) and emits `committed(value)` only on mouse release OR after 500 ms of no movement. `ConfigStore` setters are wired to `committed`, not `value`. |
| 17 | **OllamaClient on a machine without Ollama**. `ollama list` shells out; without ollama installed, hangs or fails. | `QProcess::start("ollama", {"list"})` with `setProcessChannelMode(MergedChannels)` and a 2 s timeout via `QTimer::singleShot`. On timeout / non-zero exit: `available_models` is `[]`, page shows "Ollama not detected — install with `ollama serve` or run `ollama pull qwen2.5:1.5b`". |
| 18 | **HiDPI + fractional scaling**. KDE 6 supports per-display fractional scaling; SVGs need correct intrinsic size. | All raster sources (grain noise) generated at `Screen.devicePixelRatio` × 220 px. SVGs use `sourceSize`. Matched pixel sizes everywhere — `Theme.fontSize.row = 13.5`, `Theme.spacing.section = 28` — Qt scales them. |
| 19 | **Frameless window dragging**. Custom titlebar means we lose Wayland's built-in drag. | QML `MouseArea` on titlebar with `onPressed: window.startSystemMove()` (Qt 6.5+). Maximize/minimize/close hook into `window.showMaximized()` / `showMinimized()` / `close()`. |
| 20 | **Reset-all is destructive**. §U1.8 says back up before overwrite. | `ConfigStore::resetAll()` is the only setter that bypasses debounce. (i) writes `config.toml.bak.<ts>`, (ii) writes a fresh TOML from `XTypeConfig{}` defaults, (iii) shows a 4 s toast with the backup path, (iv) marks the reload banner. |

---

## §6. Acceptance criteria (rolled-up — verify at end of U1c)

A reviewer running `./build/xtype-settings` sees:

- [ ] Every UI control from `prototype/index.html` is present and bound
      to a real `XTypeConfig` field, an engine probe, or a fixture, AND
      every disabled control shows the "coming soon" affordance.
- [ ] No fake numbers anywhere — CPU / RAM / latency / corpus stats are
      either real reads or zeros. The only fixture data is the four
      `DEMO_SEQUENCES` in `LiveDemo`.
- [ ] Toggling any wired control persists across `xtype-settings`
      restarts AND across `fcitx5-remote -r` reloads (TOML round-trip).
- [ ] Reload banner appears after any write and successfully reloads
      the engine on click.
- [ ] Empty-corpus / no-profile / no-Ollama states render without
      errors or visible exception traces.
- [ ] App builds with `-Wall -Wextra -Wshadow -Werror=return-type`
      clean; `qmllint` clean.
- [ ] Existing engine tests pass unchanged
      (`ninja -C fcitx5-engine/build test`).
- [ ] No engine-thread API calls — the app talks only to the
      filesystem, `fcitx5-remote`, `ollama list`, and `xtype-corpus`.
- [ ] App launches from the KDE app launcher via the `.desktop` entry.
- [ ] On KDE Plasma 6 Wayland, the window is translucent with KWin
      blur; on non-KDE compositors, the QML grain+gradient fallback
      renders without blur.

---

## §7. Out of scope

Same as UI-PLAN §U1.7 plus:

- KCModule embedding (deferred to a future "Session 23" if requested;
  the `App.qml` scene is self-contained enough to drop into a
  `KCModule { … }` shell when the time comes).
- AT-SPI / window-detection integrations (Phase F territory).
- Theme persistence across users (Theme.hue is per-app for now).
- i18n / Qt linguist (everything is English; strings live in QML).
- Auto-update / version checks.
- A TUI fallback for headless servers.

---

## §8. Gotchas (in addition to UI-PLAN §U1.8)

- **`QQuickStyle::setStyle("Basic")` must be called before
  `QQmlApplicationEngine::loadFromModule()`** or QQC2 picks up Breeze
  and ComboBox/Dialog look wrong.
- **`Q_PROPERTY` notify signals must be unique per property** in QML
  (or QML silently binds to the first signal). Use
  `NOTIFY engineEnabledChanged` etc., not a single `NOTIFY changed`,
  for fine-grained binding. The `changed()` signal is for the debounce
  timer only — declare per-field `xChanged` signals as well.
- **QML `Image` providers must register before
  `engine.loadFromModule()`** — `engine.addImageProvider("grain", new
  GrainProvider)` first, or `image://grain/window` fails for the first
  frame.
- **`Q_PROPERTY(QVariant threads)` round-trip**: `null` from QML maps
  to `QVariant()` in C++; serialise that as "key absent" in toml++. If
  a user types a number then clears the field, the TOML loses the key
  — that is the desired behaviour.
- **Custom font loaded but not used**: Qt 6 falls back silently if a
  family name typo doesn't match the registered face. Always check
  `QFontDatabase::families().contains("Inter")` at startup and warn
  loudly if absent — the prototype's whole feel rests on Inter.
- **`fcitx5-remote -s` blocks** if fcitx5 is starting up. Use
  `QProcess` `start()` + 800 ms `waitForFinished` then `kill()` if
  still running. No blocking `system()` calls.
- **QML `Behavior` does not animate property changes inside a
  `PropertyChanges` block** — animations on toggle states must use
  `transitions:` on the parent or `NumberAnimation { … }` inside the
  `Behavior`.
- **Dragging a slider while the debounce timer is pending** must
  reset the timer, not pile up writes. `setX` does `timer.start()`
  (not `singleShot`), so each call restarts the 500 ms window.
- **App restart during write**: `QSaveFile::commit()` is atomic, but
  the backup-rotate step is not. Always write the `.bak` first, then
  overwrite the live file.
- **The prototype's `INITIAL_STATE.apps` keys** (`kate`, `tb`, `kons`,
  `firefox`, `discord`) are short ids. The Qt app must not write those
  to TOML — use canonical program names (`kate`,
  `org.mozilla.thunderbird`, `org.kde.konsole`, `firefox`, `discord`)
  as the engine's `apps` map key. The display label/short-id mapping
  lives only in `apps_known.cpp`.

---

## §9. Total estimated work

- C++ backend: ~1200 lines across 9 small TUs.
- QML: ~2000 lines across 22 files.
- CMake + desktop file + main.cpp: ~150 lines.
- Engine scaffold (§U1.2 carry-over): ~150 lines of header changes.
- Vendored toml++: 1 file.

Three sub-sessions:

- **U1a** (Steps 0–4): plan housekeeping + engine scaffold + project +
  blur spike + theme + window chrome.
- **U1b** (Steps 5–7): primitives + ConfigStore + tests + EngineProbe
  wiring.
- **U1c** (Steps 8, 10–12): pages + smoke test + final docs.
