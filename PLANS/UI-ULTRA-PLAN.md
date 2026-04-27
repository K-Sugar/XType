# UI-ULTRA-PLAN — Qt6/QML XType Settings (replaces UI-PLAN §U1)

> Goal: ship a standalone Qt 6 / QML desktop app at `settings/qt6-app/`
> that is a **pixel-and-behaviour-faithful port** of `prototype/index.html`,
> wired to the real engine surface (`config.toml`, `fcitx5-remote -r`,
> `~/.local/share/xtype/`, procfs, `ollama list`). After this plan executes,
> the React + Python bridge scaffolding described in UI-PLAN §U1.3–§U1.4 is
> obsolete — Session 22 ("Settings UI + per-app profiles") collapses into
> the same artefact and `prototype/` is kept only as a design reference.
>
> The U1.2 engine scaffold (config structs, metrics, recent-events ring,
> phrase blocklist stub, accessors) **carries over verbatim** — the Qt app
> reads/writes those same structs via TOML. No bridge process; the Qt app
> talks directly to the filesystem and to `fcitx5-remote`.

---

## 0. Pivot summary — what changes vs UI-PLAN §U1

| UI-PLAN §U1 (web prototype scaffold)              | This ultra plan (Qt6 native)                            |
| ------------------------------------------------- | ------------------------------------------------------- |
| `settings/prototype/bridge.py` HTTP server        | C++ `ConfigStore` + `EngineProbe` linked into the app   |
| `settings/prototype/hooks.jsx` (`useConfig` etc.) | QML `ConfigStore` singleton + `Q_PROPERTY` bindings     |
| `settings/prototype/proto-pages.jsx` swap         | QML pages under `settings/qt6-app/qml/pages/`           |
| `settings/prototype/fixtures.json` empty-state    | Same JSON, loaded by `ConfigStore` on first start       |
| Babel-in-browser dev loop                         | `cmake -B build && ninja -C build && ./xtype-settings`  |
| Reload banner clicks `POST /api/engine/reload`    | Reload banner runs `QProcess::startDetached("fcitx5-remote", {"-r"})` |
| Localhost CORS + CSRF guards                      | None needed — process-local                             |

What is **kept** from §U1:
- Every U1.2 engine deliverable (`config.h` extensions, `engine_metrics.h`,
  `recent_events.h`, `phrase_blocklist.h`, `xtype.h` accessors). The Qt
  app needs the same structs to round-trip TOML correctly.
- Every U1.6 acceptance criterion translated to Qt: every control bound
  to a real field, no fake numbers, TOML round-trip survives restart,
  reload banner works, empty-state renders, `-Wall -Wextra -Wshadow` clean.
- Every §U1.8 gotcha (`tab_accepts_word` rename loader, `optional` TOML
  serialisation, `apps` map keying, disabled-controls-still-write rule,
  log path unification, reset backup, metric coalescing).

What is **discarded**:
- Python, HTTP, fixtures-as-server, `bridge.py`, fetch-error fallbacks,
  CSRF / Origin guards, the React+Babel dev loop entirely.

---

## 1. Stack decision

**Qt 6.7+ / QML / C++20 / Qt Quick Controls 2 (minimal use)**.

Why QML, not Qt Widgets:
- The prototype relies on translucency, OKLCH gradients, animated
  toggles/sliders/pills, page transitions, blinking carets, pulsing dots,
  bar-chart growth, custom titlebar, grain overlay, blurred window
  background. These are 5–10 lines of declarative QML each and a custom
  `QStyle` subclass fight in Widgets.
- QML's `Behavior on x { NumberAnimation { … } }` matches the prototype's
  `transition: 0.16s ease` CSS one-for-one.
- `LinearGradient` / `RadialGradient` / `OpacityMask` /
  `MultiEffect.blurEnabled` cover everything in `prototype.css` without
  shaders.

Why **standalone** app, not KCModule:
- The prototype renders its own titlebar with traffic-light buttons,
  custom blur, custom window radius, edge-to-edge purple gradient. KCMs
  are embedded inside System Settings's Breeze chrome and cannot draw
  outside their tab area. A KCM cannot reproduce the prototype design.
- Standalone apps can still be surfaced in System Settings via a
  `.desktop` entry under `Settings/InputMethod/` (cf. `latte-dock`,
  `kvantum`). That is sufficient for KDE integration.
- A thin embedded KCM shell that hosts the same QML scene **inside** a
  KCModule can be added later as a one-session follow-up if a maintainer
  insists on native System Settings integration. Out of scope here.

Qt versions on this machine: `Qt 6.11.0` (`pkg-config --modversion
Qt6Core` → `6.11.0`). Target `Qt 6.7` as the minimum so CachyOS, Arch,
Fedora 40+, Ubuntu 24.10+ all work. Keep the dependency footprint to:
`Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2`. **No** KF6
beyond `KWindowSystem` for the Wayland blur protocol — keeping this Qt6
pure means it builds on non-KDE desktops too.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  xtype-settings (Qt6 app, ELF binary)                       │
│                                                             │
│  ┌──────────────────────────┐   ┌──────────────────────┐   │
│  │  C++ backend             │   │  QML scene           │   │
│  │                          │   │                      │   │
│  │  ConfigStore             │◀──┤  Theme.qml (sing.)   │   │
│  │  ├─ load(toml)           │   │  Primitives/         │   │
│  │  ├─ save(toml + .bak)    │   │  Pages/              │   │
│  │  └─ Q_PROPERTY x40       │   │  App.qml             │   │
│  │                          │   │                      │   │
│  │  EngineProbe (QTimer)    │──▶│  bound to QML props  │   │
│  │  ├─ procfs CPU/RAM       │   │                      │   │
│  │  ├─ fcitx5-remote -s     │   │                      │   │
│  │  └─ EngineState enum     │   │                      │   │
│  │                          │   │                      │   │
│  │  OllamaClient (QNetwork) │──▶│  available models    │   │
│  │  ├─ list()               │   │  active model meta   │   │
│  │  └─ activeMeta(name)     │   │                      │   │
│  │                          │   │                      │   │
│  │  LogTail (QFileSystemW.) │──▶│  ListModel of lines  │   │
│  │                          │   │                      │   │
│  │  CorpusStats (xtype-     │──▶│  sentences/words/MB  │   │
│  │  corpus stats --json)    │   │                      │   │
│  │                          │   │                      │   │
│  │  StyleProfileLoader      │──▶│  exemplars/openers   │   │
│  │  (~/.local/share/        │   │                      │   │
│  │   xtype/style_profile.   │   │                      │   │
│  │   json)                  │   │                      │   │
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
and `fcitx5-remote -s`. This deliberately mirrors §U1.2's "engine
accessors are reserved for an in-process KCM future" rule — the Qt app
**is not** the in-process variant; it is the bridge equivalent reborn as
native code. The C++ accessors landed in §U1.2 stay unused by this app.

Threading: every filesystem read happens on the GUI thread inside a
`QFutureWatcher` if it can exceed 5 ms (`xtype-corpus stats`,
`ollama list`, log tail seek to EOF). Procfs CPU/RAM is fast enough to
sample on the GUI thread inside a 1500 ms `QTimer`. Writes go through a
500 ms `QTimer` debounce to coalesce slider drags, exactly as the
prototype's `useConfig` debounces.

---

## 3. Repo layout

```
settings/qt6-app/
├── CMakeLists.txt
├── README.md                          (1 paragraph: how to run)
├── resources.qrc                      (auto-generated via qt_add_qml_module)
├── data/
│   ├── xtype-settings.desktop         (KDE menu entry; Categories=Settings;InputMethod;)
│   ├── icons/xtype.svg                (256×256 brand-mark; reuse prototype gradient)
│   ├── fonts/Inter-Variable.ttf       (bundled — see §4 Challenge 7)
│   └── fonts/JetBrainsMono-Variable.ttf
│
├── src/
│   ├── main.cpp                       (QGuiApplication + QQmlApplicationEngine)
│   ├── config_store.{h,cpp}           (TOML round-trip; ~40 Q_PROPERTYs)
│   ├── engine_probe.{h,cpp}           (CPU/RAM/state poller)
│   ├── ollama_client.{h,cpp}          (QNetworkAccessManager → /api/tags)
│   ├── corpus_stats.{h,cpp}           (QProcess wrap of xtype-corpus stats --json)
│   ├── style_profile_model.{h,cpp}    (QAbstractListModel of exemplars/openers)
│   ├── log_tail.{h,cpp}               (QFileSystemWatcher + tail-N)
│   ├── reloader.{h,cpp}               (QProcess fcitx5-remote -r)
│   ├── recent_events_model.{h,cpp}    (fixture-backed; mirrors DEMO_SEQUENCES)
│   ├── apps_known.{h,cpp}             (curated list + .desktop scan)
│   ├── build_info.h.in                (CMake-configured: VERSION, GIT_COMMIT)
│   └── toml/                          (vendored toml++ single-header v3.4)
│       └── toml.hpp
│
└── qml/
    ├── App.qml                        (window, titlebar, sidebar, content stack, modal, toast)
    ├── Theme.qml                      (singleton: colors, font families, easing curves)
    ├── primitives/
    │   ├── XToggle.qml
    │   ├── XSlider.qml
    │   ├── XPill.qml
    │   ├── XSegmented.qml
    │   ├── XSection.qml
    │   ├── XRow.qml
    │   ├── XBars.qml
    │   ├── XButton.qml                (.btn / .btn.primary / .btn.ghost variants)
    │   ├── XInput.qml                 (text input matching prototype.css .input)
    │   ├── XCard.qml                  (default + accent variants)
    │   ├── XAppRow.qml                (per-app row with icon)
    │   ├── XAppIcon.qml               (32×32 gradient swatch with initial)
    │   ├── XKbd.qml                   (kbd-shaped chip)
    │   ├── XPhraseRow.qml             (text + ×)
    │   ├── XLogList.qml               (mono, scroll-pinned)
    │   ├── XEngineDot.qml             (pulsing 7-px circle)
    │   ├── XMiniBar.qml               (3-px progress)
    │   ├── XGrain.qml                 (SVG fractalNoise overlay)
    │   ├── XTitleBar.qml              (window controls + status)
    │   └── XSidebar.qml
    └── pages/
        ├── PageGeneral.qml
        ├── PagePersonalisation.qml
        ├── PageBlockList.qml
        ├── PagePerApp.qml
        ├── PageModel.qml
        ├── PageAbout.qml
        └── LiveDemo.qml               (typewriter + ghost-text + caret)
```

CMake glues this together via `qt_add_qml_module(xtype-settings URI
XType.Settings)` so all QML files become a static resource and `import
XType.Settings` works without copy-around tricks.

---

## 4. Challenges and solutions

| # | Challenge | Solution |
|---|-----------|----------|
| 1 | **Translucent blurred window**. The prototype's defining look is `backdrop-filter: blur(28px)` over an oklch-tinted base. Qt 6 windows are opaque by default; KWin needs to be told to blur behind the surface. | `setFlags(Qt::FramelessWindowHint)` + `setColor(Qt::transparent)` on `ApplicationWindow`. On Wayland, request KWin blur via `KWindowEffects::enableBlurBehind(window, true)` (KF6 `KWindowSystem` is the only KF6 dependency; gracefully no-op on non-KWin compositors with `if (KWindowSystem::isPlatformWayland())`). On X11/non-KDE, fall back to a frosted-glass backdrop drawn in QML using a tiled noise + radial gradient — visually 90% of the way there. The fallback is the default for portability; users who run Plasma get the real blur for free. |
| 2 | **OKLCH colors**. CSS uses `oklch(0.62 0.17 288)` extensively; QColor speaks sRGB. Hue at runtime via `--hue` CSS var let users tune purple from 260→330 in the prototype. | Tweaks panel is dropped (§5 step 9), so `theme.hue` is **build-time constant** (`288`). Precompute the ~12 theme colors once at build time from a small Python script `scripts/gen-theme-colors.py` that uses `culori` (or its Python equivalent `colour` package, or a vetted port — **do not roll the math by hand**: OKLCh→OKLab→linear-sRGB→sRGB-gamma + gamut clip is the correct sequence and the clip step is the trap). Output `qml/theme_colors.json`, loaded by `Theme.qml` at startup. If runtime hue tuning is ever re-introduced, replace the JSON load with the JS port at that point — but defer until needed. |
| 3 | **Custom-shaped controls**. Toggle, Slider, Pill, Segmented, Phrase row are not stock Qt Quick Controls 2 — restyling QQC2 to look like the prototype is fragile. | Hand-roll each as a QML `Item` composing `Rectangle` + `Behavior` + `MouseArea`. Total custom-control code ≤ 400 lines. QQC2 is used only for `ScrollView`, `TextField` (inside `XInput`), and `ComboBox` (per-app mode select). |
| 4 | **Live demo typewriter**. `useEffect` chain in `LiveDemo` cycles 4 sequences with intra-step timeouts. | Direct port: a single `Timer` in QML driven by an `int phase` state machine. Each phase has a duration; `onTriggered` advances `phase` and resets `interval`. Identical behaviour to the JSX. |
| 5 | **Pulsing dot, blinking caret, page-in animation**. Three independent CSS keyframe animations. | Three QML `SequentialAnimation`s with `loops: Animation.Infinite`. Caret is a `Rectangle { width: 2; height: 16 }` with `Behavior on opacity { … }` driven by a 1 s `Timer`. |
| 6 | **Grain overlay**. Prototype uses an inline SVG fractalNoise filter as a `background-image` on `body::after` and `.window::after`. | Render the SVG once at startup into a `QImage` (Qt SVG renderer handles `feTurbulence`), wrap in a `QQuickImageProvider`, expose as `image://grain/desktop` and `image://grain/window`. QML uses `Image { source: "image://grain/window"; fillMode: Image.Tile; opacity: theme.grain }`. Caching a 220×220 PNG eliminates per-frame cost. |
| 7 | **Custom fonts (Inter Variable + JetBrains Mono Variable)**. Prototype loads from Google Fonts CDN; Qt app must work offline and not phone home. | Bundle both TTFs (~700 KB total) under `data/fonts/`, register at startup via `QFontDatabase::addApplicationFont(":/fonts/Inter-Variable.ttf")`. Theme.qml sets `theme.sans = "Inter"; theme.mono = "JetBrains Mono"`. Fallback to `system-ui` and `monospace` if files are missing (build still works without the TTFs landed — log a warning). Licenses (OFL) included under `data/fonts/LICENSE-*.txt`. |
| 8 | **TOML round-trip preserves comments?** `toml++` v3 reads comments but does not write them. Rewriting `config.toml` will drop user comments. | (i) Vendor `toml++` v3.4 single-header into `src/toml/toml.hpp`. (ii) On save, write a header comment `# Generated by xtype-settings — manual edits are preserved across sessions but inline comments are not.` (iii) Always write the file via `QSaveFile` (atomic temp-file + rename). (iv) Backup current file to `config.toml.bak.<unix-ts>` before overwriting (matches §U1.8 reset-backup rule and applies it to every save). Document this once in the README, never alarm the user with a toast. |
| 9 | **`std::optional` in TOML**. `AppOverride` and `InferenceConfig::threads` use `optional`; toml++ has no first-class optional. | Convention: an unset optional means "key is absent". `ConfigStore::saveAppOverride` skips `insert` for any field where the QML side has not set the property. Round-trip test: load → save → reload yields byte-identical TOML for fields that were never touched (mod the header comment). |
| 10 | **`fcitx5-remote -s` exit code** as engine state proxy. Spec for the exit code is not stable across fcitx5 minor versions. | Treat any non-zero exit as `paused`, any zero exit as `ready`. If `fcitx5-remote` itself is missing (no fcitx5 installed), `state = "error"` with message "fcitx5 not found". Probe every 1500 ms inside `EngineProbe`. Coalesce with the CPU/RAM read so the procfs walk happens once per tick. |
| 11 | **Per-app program names**. Engine keys `XTypeConfig::apps` by lowercased program name (e.g. `kate`, `org.mozilla.thunderbird`). The prototype uses synthetic short ids (`kate`, `tb`, `kons`). | The Qt UI shows display labels (`Kate`, `Thunderbird`, `Konsole`) but stores keys in their canonical form. `apps_known.cpp` carries a curated map `{id → {label, canonicalProgram, iconClass, defaultMode}}`. The "+ add app" picker scans `~/.local/share/applications/*.desktop` + `~/.local/share/xtype/seen_programs.txt` (a future log-tail enhancement; for U1 scaffold the file may not exist and the picker just shows the curated list). |
| 12 | **Reload banner UX**. The §U1.4 spec wants a sticky banner that appears after any write and clears on successful reload. | A QML singleton `ReloadCenter` exposes `bool dirty` and `bool inflight`. Every `Q_PROPERTY` setter on `ConfigStore` that wrote to disk emits `ReloadCenter.markDirty()`. The banner is a fixed-bottom `Rectangle` in `App.qml` with a `Behavior on y` slide. Click → `Reloader.reload()` → on `finished` → `dirty = false`. If `fcitx5-remote -r` exits non-zero, banner text changes to "Reload failed — check Reloader output" and a retry button appears. |
| 13 | **Disabled-but-persisted controls**. §U1.8 demands controls like Quantisation / Threads / Trigger=Manual / Accept-key Enter|→ / per-app modes other than Default+Off render disabled but still write their value to TOML. | Each primitive accepts an `enabled: bool` and a `comingSoon: bool`. `comingSoon=true` lowers `opacity: 0.45`, sets `Accessible.description: "Not yet wired into the engine"`, but the underlying `MouseArea` still calls `onChange`. Tooltip via `ToolTip.text` and `ToolTip.visible: hovered`. |
| 14 | **Live logs feed**. `QFileSystemWatcher` on a single file works on Linux but is debounced — fast bursts can collide. | `LogTail` opens the log read-only with `QFile`, seeks to EOF, and on `fileChanged` reads from the saved offset. If size shrunk (rotation), seek to 0. Returns lines as a `QStringListModel` capped at the most recent 200. Parse-on-display: `QML` colours `accept` lines purple by matching `/suggest accepted/`. |
| 15 | **Engine log path is split** (README mentions both `engine.log` and `debug.log`). | Per §U1.8: this plan unifies on `~/.local/share/xtype/fcitx5.log`. The engine PR that writes to the new path is part of §U1.2. The Qt app reads from the new path; if missing, falls back to `engine.log` and `debug.log` in that order, displaying a one-line warning header. |
| 16 | **Slider drag + debounce** vs **immediate visual feedback**. The prototype updates the displayed value live but only persists to bridge after 500 ms idle. | QML side: `XSlider` exposes `value` (live, 60 fps) and emits `committed(value)` only when the user releases the mouse OR after 500 ms of no movement (whichever first). `ConfigStore` setters are wired to `committed`, not `value`. Pressing without moving still commits via the mouse-release path. |
| 17 | **OllamaClient on a machine without Ollama**. `ollama list` shells out; `~ollama list` in a sandbox without ollama hangs or fails. | `QProcess::start("ollama", {"list"})` with `setProcessChannelMode(MergedChannels)` and a 2 s timeout via `QTimer::singleShot`. On timeout or non-zero exit, `available_models` is `[]` and the page shows an empty state "Ollama not detected — install with `ollama serve` or run `ollama pull qwen2.5:1.5b`". No crashes, no hangs. |
| 18 | **HiDPI + fractional scaling**. KDE 6 supports per-display fractional scaling; QML defaults handle it but custom-painted SVGs (icons, brand mark) need correct intrinsic size. | All raster sources (grain noise) are generated at `Screen.devicePixelRatio` × 220 px. SVGs (icons, brand mark) use `sourceSize` to render at native DPI. Matched pixel sizes everywhere — `Theme.fontSize.row = 13.5` (px), `Theme.spacing.section = 28` — Qt scales them automatically. |
| 19 | **Frameless window dragging**. Custom titlebar means we lose Wayland's built-in drag. | QML `MouseArea` on the titlebar with `onPressed: window.startSystemMove()` (Qt 6.5+). Maximize/minimize/close hook into `window.showMaximized()` / `showMinimized()` / `close()`. Snapping behaviour comes free from KWin via `startSystemMove`. |
| 20 | **Reset-all is destructive**. §U1.8 says back up before overwrite. | `ConfigStore::resetAll()` is the only setter that bypasses debounce. It (i) writes `config.toml.bak.<ts>`, (ii) writes a fresh TOML from `XTypeConfig{}` defaults, (iii) shows a 4-second toast with the backup path, (iv) marks the reload banner. |

---

## 5. Step-by-step plan (agentic)

Each step ends with a green-build / explicit visual checkpoint so an
agent can stop, verify, and commit before moving on. Steps are sized so
that a session can stop after any one and the repo is in a coherent
state.

### Step 0 — Amend PLAN.md / UI-PLAN.md before any code lands

CLAUDE.md mandates "one commit per session, message from PLAN.md".
Three sub-sessions × multiple commits violates that contract unless the
plan files are amended *first*. Do this in the very first commit of
the work, before any code:

- In `UI-PLAN.md`: rename `Session U1` → `Session U1a/U1b/U1c` with
  explicit headlines and commit messages identical to those in §5
  (Step 2 = `feat(settings): qt6 project scaffold + main + cmake`,
  Step 5 = `feat(settings): primitive library …`, Step 11 =
  `feat(settings): qt6/qml standalone settings app — full prototype port`).
- In `PLAN.md`: collapse `Session 22` into a one-line note "(merged
  into Session U1c — see UI-ULTRA-PLAN.md)".
- In this file (`UI-ULTRA-PLAN.md`): leave as-is; it is the working
  reference for the agent across all three sub-sessions.

Commit: `docs(plan): split session U1 into U1a/U1b/U1c for qt6 ui pivot`.

### Step 1 — Engine scaffold (UI-PLAN §U1.2)

§U1.2 has **not** landed yet — verified by reading
`fcitx5-engine/src/config.h`, which still has the legacy
`tab_accepts_word`, no `engine_enabled`, no `trigger_mode`, no
`UserPromptConfig`, no `apps` map, no `engine_metrics.h`, no
`recent_events.h`, no `phrase_blocklist.h`. **Execute §U1.2 in full
here, before any Qt-app work.** The Qt app's TOML writer in Step 6
assumes those new keys; without them, every save is byte-corruption
to a real user's existing config.

Files: identical to §U1.5 engine block. Acceptance: identical to §U1.6
engine bullet (`-Wall -Wextra -Wshadow` clean, existing engine tests
pass, dual-key loader accepts both `tab_accepts_word` and
`partial_accept` and warns on the legacy form).

Commit: `feat(engine): scaffold config + metrics + recent-events for settings UI`.

### Step 2 — Qt6 project scaffold

Files (new): `settings/qt6-app/CMakeLists.txt`,
`settings/qt6-app/src/main.cpp`,
`settings/qt6-app/qml/App.qml`,
`settings/qt6-app/qml/Theme.qml`,
`settings/qt6-app/data/xtype-settings.desktop`,
`settings/qt6-app/data/icons/xtype.svg`,
`settings/qt6-app/src/build_info.h.in`,
`settings/qt6-app/src/toml/toml.hpp` (vendored from `https://github.com/marzer/tomlplusplus/releases/download/v3.4.0/toml.hpp`).

`CMakeLists.txt` essentials:

```cmake
cmake_minimum_required(VERSION 3.21)
project(xtype-settings VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.7 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.7)

# Optional KDE blur — graceful no-op without it.
find_package(KF6WindowSystem QUIET)

configure_file(src/build_info.h.in ${CMAKE_CURRENT_BINARY_DIR}/build_info.h @ONLY)

qt_add_executable(xtype-settings
    src/main.cpp
    src/config_store.cpp
    src/engine_probe.cpp
    src/ollama_client.cpp
    src/corpus_stats.cpp
    src/style_profile_model.cpp
    src/log_tail.cpp
    src/reloader.cpp
    src/recent_events_model.cpp
    src/apps_known.cpp
)

qt_add_qml_module(xtype-settings
    URI XType.Settings
    VERSION 1.0
    QML_FILES
        qml/App.qml
        qml/Theme.qml
        qml/pages/PageGeneral.qml
        # … all .qml files …
        qml/pages/LiveDemo.qml
    RESOURCES
        data/fonts/Inter-Variable.ttf
        data/fonts/JetBrainsMono-Variable.ttf
        data/icons/xtype.svg
)

target_include_directories(xtype-settings PRIVATE
    src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(xtype-settings PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    $<$<TARGET_EXISTS:KF6::WindowSystem>:KF6::WindowSystem>
)

install(TARGETS xtype-settings RUNTIME DESTINATION bin)
install(FILES data/xtype-settings.desktop DESTINATION share/applications)
install(FILES data/icons/xtype.svg DESTINATION share/icons/hicolor/scalable/apps RENAME xtype.svg)
```

`src/main.cpp` minimum body:

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFontDatabase>
#include <QQuickStyle>
#ifdef HAVE_KF6_WINDOW_SYSTEM
#include <KWindowEffects>
#endif

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("xtype-settings");
    QGuiApplication::setOrganizationName("xtype");
    QQuickStyle::setStyle("Basic");                       // strip Breeze theming
    QFontDatabase::addApplicationFont(":/qt/qml/XType/Settings/data/fonts/Inter-Variable.ttf");
    QFontDatabase::addApplicationFont(":/qt/qml/XType/Settings/data/fonts/JetBrainsMono-Variable.ttf");
    QQmlApplicationEngine engine;
    engine.loadFromModule("XType.Settings", "App");
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}
```

`qml/App.qml` for now is a 400×300 transparent window that prints
"hello" — purely a "does it build, run, draw?" milestone.

Checkpoint: `cmake -B build -G Ninja && ninja -C build && ./build/xtype-settings` opens an empty transparent window. `setsid -f ./build/xtype-settings` and `wmctrl -l | grep xtype` confirms it surfaces.

Commit: `feat(settings): qt6 project scaffold + main + cmake`.

### Step 2.5 — Translucent-blurred-window spike (de-risk before §5–§8)

The single highest-risk visual assumption in this plan is that
`Qt::FramelessWindowHint` + `setColor(Qt::transparent)` +
`KWindowEffects::enableBlurBehind` actually produces the prototype's
glassy-purple chrome on Plasma 6 Wayland. Qt 6 + xdg-decoration + KWin
SSD/CSD negotiation has historically broken this combination; better
to know in 30 minutes than after 600 lines of primitives.

Spike steps (no commit — throwaway code on top of Step 2):

1. In `App.qml`, set `flags: Qt.FramelessWindowHint`, `color:
   "transparent"`, draw a single rounded `Rectangle` filling the
   window with a 0.4-alpha purple gradient, no children.
2. In `main.cpp` after `engine.loadFromModule(...)`, find the root
   window via `engine.rootObjects().first()->findChild<QWindow*>()`,
   then `KWindowEffects::enableBlurBehind(rootWindow, true)` if
   `KWindowSystem::isPlatformWayland()`.
3. Run on the target machine. Take a screenshot. Three outcomes:
   - **Blur visible** → commit-time decision: keep KWin path,
     non-KDE fallback is the QML grain+gradient as planned. Continue
     to Step 3.
   - **Window is transparent but no blur** (e.g. on `gnome-shell` or
     `sway`) → expected, the QML fallback handles it. Continue.
   - **Window has a server-side decoration we can't get rid of, OR
     transparency fails** → the prototype design needs CSD work.
     Stop, spike a `QWaylandClientExtension` xdg-decoration override
     OR fall back to a non-translucent dark window (still on-brand,
     ~80% of the visual). Update §4 #1 with the decision before
     proceeding.

The spike's job is decision support, not delivered code. Keep one
commented-out line of `KWindowEffects::enableBlurBehind(...)` at the
end of `main.cpp` after the spike and uncomment it for real in Step
4.

### Step 3 — Theme singleton + grain overlay + fonts

Files: `qml/Theme.qml` (singleton, `pragma Singleton`), `src/main.cpp`
(register `Theme` as singleton via `qmlRegisterSingletonType`), the SVG
grain image provider in `src/main.cpp`, `qml/primitives/XGrain.qml`.

Workflow:

1. Add `scripts/gen-theme-colors.py` (run once, committed output) —
   uses the Python `colour` package or a vetted CSS Color 4 reference
   to convert each `oklch(L C H)` literal in `prototype.css :root` to a
   sRGB hex. Emits `settings/qt6-app/qml/theme_colors.json`.
2. `Theme.qml` reads the JSON via
   `JSON.parse(Qt.application.applicationDirPath … resources)` *or*
   directly inlines the values as `readonly property color` — pick
   inline for simplicity, regenerate only when hue changes (rare).
3. Variables exposed mirror `prototype.css :root` exactly:

```qml
pragma Singleton
import QtQuick

QtObject {
    id: theme
    readonly property real hue: 288             // build-time constant
    readonly property real bgAlpha: 0.78
    readonly property real grain: 0.18
    readonly property string sansFamily: "Inter"
    readonly property string monoFamily: "JetBrains Mono"

    // Generated by scripts/gen-theme-colors.py — DO NOT EDIT BY HAND.
    readonly property color purple:      "#9678e9"
    readonly property color purpleSoft:  "#b9a4f0"
    readonly property color purpleDeep:  "#5d4393"
    readonly property color ink100:      "#f0ecfa"
    readonly property color ink80:       Qt.rgba(0.94,0.93,0.98, 0.82)
    /* … ink65 ink45 ink25 ink12 ink06 ink03 … */

    readonly property var spacing: ({ row: 12, section: 28, page: 22 })
    readonly property var radius:  ({ window: 14, card: 10, pill: 999, btn: 7 })
    readonly property var ease:    ({ standard: [0.4,0,0.2,1], bouncy: [0.34,1.4,0.6,1] })
}
```

(The hex values above are illustrative — regenerate from the script.)

Acceptance: `import XType.Settings 1.0` and `Theme.purple` resolves to a
sensible RGBA inside a `Rectangle { color: Theme.purple }`. Slider in
the future Tweaks panel changes Theme.hue and the rectangle re-tints
within one frame.

Commit: `feat(settings): theme singleton + oklch helper + grain provider`.

### Step 4 — Window chrome (titlebar, sidebar, content stack, modal, toast)

Files: `qml/App.qml` (full version), `qml/primitives/XTitleBar.qml`,
`qml/primitives/XSidebar.qml`, `qml/primitives/XEngineDot.qml`,
`qml/primitives/XMiniBar.qml`.

Reproduce `index.html` lines 146–234 in QML:
- `ApplicationWindow` with `flags: Qt.FramelessWindowHint | Qt.Window`,
  `color: "transparent"`, default size 1180×760 (matches the prototype's
  `inset: 28px` on a 1240×820 outer).
- The window has a `Rectangle` child that is the `prototype.css .window`
  — radius 14, gradient stops via `LinearGradient`, KWin blur enabled
  via `Component.onCompleted: KWindowEffects.enableBlurBehind(...)` if
  available.
- `XTitleBar` is a 36 px row: left `engine-dot` + state text, centered
  title "XType — Settings", right minimize / maximize / close (close
  hover turns red per `prototype.css .win-btn.close:hover`). Drag is
  `MouseArea { onPressed: window.startSystemMove() }`.
- `XSidebar` is a 220 px column: brand block (gradient mark with 'X'
  glyph + name + version), `nav-item` repeater driven by a model
  identical to JSX `NAV`, spacer, status block (Model pill, CPU
  mini-bar, RAM mini-bar, engine running line). Active nav item gets a
  2 px purple left border + tinted background, identical to
  `prototype.css .nav-item.active`.
- Content area is a `StackView` (or `Loader` keyed by `currentPage`) so
  page-change re-runs the entry transition (`prototype.css @keyframes
  page-in`).
- Modal: `Dialog` from QQC2 styled minimal — backdrop blur is faked
  with a tiled grain + dim rectangle. Toast: a `Rectangle` anchored
  bottom + `SequentialAnimation` for slide-in/auto-dismiss. Both have
  `z: 100`/`z: 50` to match.

For this step the page area is just a placeholder
`Rectangle { color: "transparent"; Text { text: currentPage } }`. The
sidebar metrics are placeholder zeros — they'll bind to `EngineProbe`
in step 9.

Acceptance: window opens transparent with blur on KDE Wayland, drag
works, three traffic-light buttons function, sidebar shows six nav
items, switching nav animates the page area in.

Commit: `feat(settings): window chrome + sidebar + titlebar`.

### Step 5 — Primitives library

Files: every `qml/primitives/X*.qml` listed in §3.

For each primitive, port the matching CSS rule from `prototype.css` and
the matching JSX from `proto-primitives.jsx`. Concrete map:

| QML | Source CSS | Source JSX | Notes |
|-----|------------|------------|-------|
| `XToggle` | `.toggle`, `.toggle.on`, `.toggle::after` | `Toggle` | 36×20 rounded with sliding 14×14 dot. `Behavior on x { NumberAnimation { duration: 220; easing: Theme.ease.bouncy } }`. |
| `XSlider` | `.slider*` | `Slider` | 4 px track, gradient fill, 14 px knob with purple ring, 56-px-min mono value label on right. Live `value`, debounced `committed`. |
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
| `XEngineDot` | `.engine-dot` + `@keyframes pulse` | implicit | Pulsing 7-px circle with glow. |
| `XMiniBar` | `.mini-bar*` | inline | 3 px progress bar. Used twice in sidebar, plus inside `XSlider` if needed. |

Each primitive is a small file (~30–60 lines). Total: ~600 lines of
QML.

Dev loop hint: previewing primitives without a full C++ rebuild is
much faster via the standalone QML runtime —
`qml -I qml qml/primitives/Demo.qml` reloads on file save with no
ninja step. Use this for iterating on shapes / animations; switch back
to `ninja -C build` only when wiring through `Config.*`.

Acceptance: build a single `qml/primitives/Demo.qml` (gitignored — for
manual test only) that shows every primitive in a column. Visually
diff against the prototype rendering of `prototype/index.html` opened
in a browser: same colours, same shapes, same animation timings, same
hover affordances.

Commit: `feat(settings): primitive library (toggle, slider, pill, segmented, row, bars, …)`.

### Step 6 — `ConfigStore` C++ backend

Files: `src/config_store.{h,cpp}`.

`ConfigStore` is a `QObject` subclass, exposed as a singleton via
`qmlRegisterSingletonInstance(&store, "XType.Settings", 1, 0, "Config")`.

Properties (selected — full list ≈ 40):

```cpp
class ConfigStore : public QObject {
    Q_OBJECT
    // Behaviour
    Q_PROPERTY(bool engineEnabled READ engineEnabled WRITE setEngineEnabled NOTIFY changed)
    Q_PROPERTY(QString triggerMode READ triggerMode WRITE setTriggerMode NOTIFY changed)
    Q_PROPERTY(QString acceptKey READ acceptKey WRITE setAcceptKey NOTIFY changed)
    Q_PROPERTY(bool partialAccept …)
    Q_PROPERTY(bool escDismisses …)
    Q_PROPERTY(QStringList blocklistApps …)
    Q_PROPERTY(QStringList blockedPhrases …)
    // Inference
    Q_PROPERTY(QString model …)
    Q_PROPERTY(int debounceMs …)
    Q_PROPERTY(int numPredict …)
    Q_PROPERTY(int contextWindow …)
    Q_PROPERTY(QVariant threads …)            // null = unset
    // Learning
    Q_PROPERTY(bool learningEnabled …)
    Q_PROPERTY(int maxCorpusMb …)
    Q_PROPERTY(int minSentenceChars …)
    // UserPrompt
    Q_PROPERTY(QString userDescription …)
    Q_PROPERTY(QString userTone …)
    Q_PROPERTY(QStringList userAvoidPhrases …)
    // Apps map
    Q_PROPERTY(QVariantMap apps …)            // {program: {enabled, mode, num_predict, …}}

public slots:
    void resetAll();
signals:
    void changed();                            // any field changed → debounce save
    void saved();                              // toml write succeeded
    void saveFailed(QString reason);
};
```

`load()` parses `~/.config/xtype/config.toml` via toml++. Missing keys
fall back to `XTypeConfig{}` defaults. Legacy `tab_accepts_word` is
read into `partialAccept` and a `qWarning()` is emitted (engine has the
same fallback in §U1.2 — the dual read keeps existing user files
working).

`save()` writes via `QSaveFile` and emits `saved()`. A 500 ms
`QTimer::singleShot` lives inside `setX()` so rapid setter calls
coalesce into a single write. `resetAll()` bypasses the timer and
backs up first (§4 #20).

QML usage:

```qml
XToggle { on: Config.engineEnabled; onChange: Config.engineEnabled = checked }
XSlider { value: Config.debounceMs; min: 100; max: 800; onCommitted: v => Config.debounceMs = v }
```

Acceptance: edit any control → file at `~/.config/xtype/config.toml` is
rewritten within ~600 ms; restart app → control reads back the saved
value; `git diff` of the toml shows only the changed key (no comment
loss for the header note we added; user-added comments are removed
with a one-time toast on first overwrite).

**Round-trip test (mandatory)**. Add `tests/test_config_store.cpp`
under `settings/qt6-app/tests/`, hook into the project's existing
Catch2 setup (re-use `find_package(Catch2 3 REQUIRED)` from
`fcitx5-engine/CMakeLists.txt` — guard with
`option(BUILD_TESTS "" OFF)`). Cases:

1. Load `data/fixtures/config_default.toml` → assert every
   `XTypeConfig{}` default matches.
2. Load → mutate every `Q_PROPERTY` once → save → reload → assert all
   values survive byte-for-byte (mod the header comment).
3. Load a TOML with the legacy `tab_accepts_word = false` → assert
   `partialAccept == false` and a one-time `qWarning` was emitted.
4. Save with `apps = {"kate": {enabled: true}}` → reload → assert
   `apps["kate"].enabled` is `true`, all other `AppOverride` fields
   are absent (not `null`, not default).
5. `resetAll()` → backup file `config.toml.bak.<ts>` exists → main
   file matches `config_default.toml`.

This catches every TOML bug §4 #8/#9 warns about. Add a CI hook later
(out of scope here).

Commit: `feat(settings): config store with toml round-trip + atomic save + tests`.

### Step 7 — `EngineProbe` + `Reloader` + sidebar wiring

Files: `src/engine_probe.{h,cpp}`, `src/reloader.{h,cpp}`,
`qml/primitives/XSidebar.qml` (update).

`EngineProbe` polls every 1500 ms:
- CPU%: read `/proc/<ollama-pid>/stat` (find pid via `pgrep -x ollama`
  cached 60 s); compute delta against last sample. If ollama not
  running, sample `/proc/self/stat` so the bar is never literally zero
  during scaffolding — gated by an `XTYPE_DEMO_METRICS=1` env var so
  production stays honest. Default = real ollama or 0%.
- RAM MB: `/proc/<ollama-pid>/status` `VmRSS` line, divide by 1024.
- State: `fcitx5-remote -s` exit code (see §4 #10).

Exposes `Q_PROPERTY(int cpuPct …)`, `Q_PROPERTY(double ramGb …)`,
`Q_PROPERTY(QString state …)` ("ready" / "paused" / "error"),
`Q_PROPERTY(QString stateMessage …)`. Singleton-registered as
`Engine`.

`Reloader::reload()` runs `QProcess::startDetached("fcitx5-remote",
{"-r"})` and emits `reloadStarted` / `reloadFinished(bool ok)` after
the process exits. `App.qml` shows the sticky reload banner driven by a
`ReloadCenter` QML singleton:

```qml
QtObject {
    id: reloadCenter
    property bool dirty: false
    property bool inflight: false
    Connections { target: Config; function onSaved() { reloadCenter.dirty = true } }
    Connections { target: Reloader; function onFinished(ok) {
        reloadCenter.inflight = false
        if (ok) reloadCenter.dirty = false
    } }
}
```

Sidebar mini-bars now bind: `XMiniBar { value: Engine.cpuPct }` and
`{ value: Engine.ramGb / 4.0 * 100 }`.

Acceptance: launch `ollama serve` → CPU/RAM bars react within 1.5 s.
Stop ollama → bars drop to 0. `pkill fcitx5` → engine dot dims and
state text becomes "paused". Click reload banner after editing a
slider → banner clears within ~1 s.

Commit: `feat(settings): engine probe + reloader + live sidebar metrics`.

### Step 8 — Pages

One file per page, each ~120 lines of QML. Implement in this order
(simplest → most state):

1. **PageGeneral.qml** — direct port of `proto-pages.jsx::PageGeneral`.
   `XSection 01 Engine`: enable toggle, trigger segmented (Manual is
   `comingSoon`), trigger-delay slider, max-suggestion-length slider.
   `XSection 02 Acceptance`: accept-key pills (Enter and → are
   `comingSoon`), partial-accept toggle, dismiss-on-Esc toggle.
   `XSection Live preview`: embed `LiveDemo.qml`. `LiveDemo.qml`
   reproduces `proto-pages.jsx::LiveDemo` lines 12–56 — same four
   `DEMO_SEQUENCES`, same phase machine, same caret. `LiveDemo` reads
   `RecentEvents.first()` from `recent_events_model.cpp` if available;
   absent → falls back to fixture cycle. (For scaffold, fixture is
   always used because §U1.2's recent-events ring has no producers.)
2. **PageBlockList.qml** — direct port of `PageBlockList`. Five curated
   app pills bound to `Config.blocklistApps`; "+ add app" opens an
   `AppPicker` popup that lists `apps_known.knownApps()` minus the
   already-blocked. Phrase input (`XInput`) + "Block" `XButton` adds to
   `Config.blockedPhrases`; phrase rows render `XPhraseRow` with × that
   removes.
3. **PagePerApp.qml** — direct port of `PagePerApp`. Left column
   `Repeater` of `XAppRow` over `apps_known.knownApps()`. Selecting one
   sets `selectedApp` (local state). Right `XCard` reads from
   `Config.apps[selectedApp]` and writes through `Config.setApp(id,
   key, value)`. Mode select uses QQC2 `ComboBox` styled to match
   `prototype.css .input`. Suggestion-length slider with `comingSoon`
   tooltip "Per-app override — engine still uses global value (Session 20)".
4. **PageModel.qml** — direct port of `PageModel`. Active-model card
   reads `OllamaClient.activeMeta(Config.model)` (returns size/family
   /quant from the static lookup table in §U1.3 — translate that table
   to `src/ollama_client.cpp`). Latency / RAM / CPU stats read from
   `Engine` and from a future-zeroed `EngineMetrics` (already scaffold
   in §U1.2). Quantisation segmented and Threads slider both
   `comingSoon`. Available-models repeater pulls from
   `OllamaClient.availableModels()` async; clicking an inactive row
   opens the existing modal (`App.qml`'s `Dialog`) and on confirm calls
   `Config.setModel(name)` + `Reloader.reload()`. Import-GGUF row
   shows a toast "Use `ollama pull <model>` from a terminal".
5. **PagePersonalisation.qml** — port of `PagePersonalisation`. Stat
   cards `Words / Accept rate / Time saved` bind to `CorpusStats`
   properties (currently zero — §U1 spec). Style learning rows bound
   to `Config.learningEnabled`, `Config.maxCorpusMb`,
   `Config.minSentenceChars`. Voice profile card reads first 3
   exemplars from `StyleProfileLoader::exemplars()` and renders them in
   purple-soft; falls back to the static prose from JSX line 159 when
   no profile.json. Reset / Export buttons → `Reset` opens a confirm
   modal and on confirm runs
   `QProcess::startDetached("xtype-corpus", {"wipe", "--yes"})`;
   `Export` opens a `QFileDialog::getSaveFileName` and copies
   `~/.local/share/xtype/style_profile.json` to the chosen path.
6. **PageAbout.qml** — port of `PageAbout`. Brand card pulls version /
   commit / license from `build_info.h` (CMake-configured). Live logs
   `XLogList` bound to `LogTail.lines` (200-line ring). Diagnostics
   buttons: `Copy debug bundle` runs an in-process bundler that
   produces a `.tar.gz` to `/tmp/xtype-bundle-<ts>.tar.gz` and
   highlights it in a toast (use `QProcess` shelling out to `tar` —
   the engine bundle endpoint specified in §U1.3 is not needed since
   we're in-process); for U1 scaffold the bundle contains
   `config.toml` + last 1000 log lines + sanitised `style_profile.json`
   (exemplars stripped via a small JSON pass) + `xtype-corpus stats
   --json` output. `Reset all settings` opens a hard-confirm modal then
   calls `Config.resetAll()`. `Report a bug` opens
   `QDesktopServices::openUrl(...)` to the GitHub Issues URL. URLs
   hard-coded in `qml/About/Links.qml`:
   `https://github.com/<owner>/xtype` (placeholder; the `<owner>` slot
   is set in `data/links.json`, generated by CMake from a configurable
   `XTYPE_REPO_URL` cache variable so forks override it).

Acceptance per page: every control matches the prototype's visual,
every wired control persists across app restart, every `comingSoon`
control shows the disabled affordance, every page renders cleanly with
empty state (no corpus, no profile, no ollama).

Commit (one per page is fine, or one combined):
`feat(settings): pages — general, blocklist, per-app, model, personalisation, about`.

### Step 9 — Tweaks panel (designer-host only) — OUT OF SCOPE

Drop. The prototype's tweaks panel (purple hue / opacity / grain) is
called out in §U1 as designer-only. It does not ship to users. Skip
entirely; `Theme.hue` etc. remain settable via QML console for designer
sessions but no UI exposes them.

### Step 10 — `.desktop` entry + KDE menu integration

Files: `data/xtype-settings.desktop`:

```
[Desktop Entry]
Name=XType Settings
Comment=Configure ghost-text autocomplete
Exec=xtype-settings
Icon=xtype
Terminal=false
Type=Application
Categories=Settings;InputMethod;Qt;
StartupNotify=true
StartupWMClass=xtype-settings
```

Acceptance: `cp data/xtype-settings.desktop ~/.local/share/applications/`
→ entry shows up in Plasma's app launcher under Settings; click
launches the binary.

Commit: `feat(settings): kde menu entry`.

### Step 11 — Build & install + smoke test

`README.md` (one paragraph):

```
cd settings/qt6-app
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/xtype-settings        # run from source
sudo ninja -C build install   # system install (writes /usr/local/bin + /usr/local/share/applications)
```

Smoke test checklist (live, on KDE Plasma 6 Wayland):
1. Launch app cold — window opens transparent + blurred.
2. Drag titlebar — window moves, snaps to screen edges.
3. Click each of 6 nav items — page transitions in 280 ms with the
   page-in animation.
4. Toggle "Enable XType globally" → reload banner appears → click reload
   → fcitx5 reloads (verify with `fcitx5-remote -r` echo in `journalctl
   --user -u plasma-fcitx5` or the live logs panel) → banner clears.
5. Drag every slider — value debounces, committed value persists across
   restart.
6. Quantisation / Threads / Trigger=Manual / Accept=Enter / Per-app
   non-Default mode — controls react and persist but show the
   "coming soon" tooltip.
7. Stop ollama (`pkill ollama`) — Model page goes to empty state without
   crashing; sidebar CPU/RAM drops to 0.
8. With ollama running: switch model — modal confirms, app pauses
   visually for ~5 s while reload completes, sidebar model pill
   updates.
9. Wipe corpus from Personalisation → confirm modal → corpus.txt
   removed → stats card drops to zeros.
10. Reset all settings → confirm modal → backup file
    `config.toml.bak.<ts>` exists → all controls return to defaults.
11. Open About → live logs scrolls. Trigger an event in fcitx5 (type
    in Kate) → new line appears within 1 s.
12. `qmllint qml/**/*.qml` clean.
13. `cmake --build build -- -k 0` clean (`-Wall -Wextra -Wshadow`,
    `-Werror=return-type`).

Commit: `feat(settings): qt6/qml standalone settings app — full prototype port`.

This is the headline commit that ends the U1 scope.

### Step 12 — Update plans + collapse Session 22 into Session U1

- Mark `[x] Session U1` in `UI-PLAN.md`.
- Strikethrough Session 22 in `PLAN.md` (or rename it "(merged into U1)")
  — or keep it as a placeholder for "future native KCModule embedding"
  but explicitly out of scope for now.
- Update `README.md`'s "Configuration" section to document
  `xtype-settings` as the recommended path (TOML editing kept as a
  CLI fallback).
- Update `docs/config-migration.md` (created in §U1.2) to mention that
  `xtype-settings` automatically migrates the legacy `tab_accepts_word`
  key on first save.

Commit: `docs(settings): collapse session 22 into u1; document xtype-settings`.

---

## 6. Acceptance criteria (rolled-up)

A reviewer running `./build/xtype-settings` sees:

- [ ] Every UI control from `prototype/index.html` is present and bound
      to a real `XTypeConfig` field, an engine probe, or a fixture, AND
      every disabled control shows the "coming soon" affordance.
- [ ] No fake numbers anywhere — CPU / RAM / latency / corpus stats are
      either real reads or zeros. The only fixture data is the four
      `DEMO_SEQUENCES` in `LiveDemo` (matched to §U1.8 gotcha "fixtures
      must look real").
- [ ] Toggling any wired control persists across `xtype-settings`
      restarts AND across `fcitx5-remote -r` reloads (TOML round-trip).
- [ ] Reload banner appears after any write and successfully reloads
      the engine on click.
- [ ] Empty-corpus / no-profile / no-Ollama states render without
      errors or visible exception traces.
- [ ] App builds with `-Wall -Wextra -Wshadow -Werror=return-type`
      clean; `qmllint` clean.
- [ ] Existing engine tests pass unchanged (`ninja -C
      fcitx5-engine/build test`).
- [ ] No engine-thread API calls — the app talks only to the
      filesystem, `fcitx5-remote`, `ollama list`, and `xtype-corpus`.
- [ ] App launches from the KDE app launcher via the `.desktop` entry.
- [ ] On KDE Plasma 6 Wayland, the window is translucent with KWin
      blur; on non-KDE compositors, the QML grain+gradient fallback
      renders without blur (tested under `gnome-shell` Wayland or
      `sway`).

---

## 7. Out of scope for this ultra plan

Same as §U1.7 plus:

- KCModule embedding (deferred to a future "Session 23" if a maintainer
  files for it; the `App.qml` scene is already self-contained enough to
  drop into a `KCModule { … }` shell when the time comes).
- AT-SPI / window-detection integrations (Phase F territory).
- Theme persistence across users (Theme.hue is per-app for now; could
  later live in `~/.config/xtype/ui.toml`).
- i18n / Qt linguist (everything is English; strings live in QML).
- Auto-update / version checks.
- A TUI fallback for headless servers (not a use case).

---

## 8. Gotchas (in addition to §U1.8)

- **`QQuickStyle::setStyle("Basic")` must be called before
  `QQmlApplicationEngine::loadFromModule()`** or QQC2 will pick up
  Breeze and Combo/Dialog will look wrong.
- **`Q_PROPERTY` notify signals must be unique per property** in QML
  (or QML will silently bind to the first signal). Use
  `NOTIFY engineEnabledChanged` etc., not a single `NOTIFY changed`,
  for fine-grained binding. The `changed()` signal in §5 step 6 is for
  the debounce timer only — declare per-field `xChanged` signals as
  well.
- **QML `Image` providers must register before
  `engine.loadFromModule()`** — `engine.addImageProvider("grain", new
  GrainProvider)` first, or `image://grain/window` fails for the first
  frame.
- **`Q_PROPERTY(QVariant threads)` round-trip**: `null` from QML maps
  to `QVariant()` in C++; serialise that as "key absent" in
  toml++. If a user types a number then clears the field, the TOML
  loses the key — that is the desired behaviour (matches §4 #9).
- **Custom font loaded but not used**: Qt 6 falls back silently if a
  family name typo doesn't match the registered face. Always check
  `QFontDatabase::families().contains("Inter")` at startup and warn
  loudly if absent — the prototype's whole feel rests on Inter.
- **`fcitx5-remote -s` blocks** if fcitx5 is starting up. Run it via
  `QProcess` with `start()` + 800 ms `waitForFinished` then `kill()` if
  still running. Do not use blocking `system()` calls.
- **QML `Behavior` does not animate property changes inside a
  `PropertyChanges` block** — animations on toggle states must use
  `transitions:` on the parent or `NumberAnimation { … }` inside the
  `Behavior`. Trip wire when porting `prototype.css :hover` rules.
- **Dragging a slider while the debounce timer is pending** must
  reset the timer, not pile up writes. `setX` on the C++ side does
  `timer.start()` (not `singleShot`), so each call restarts the 500 ms
  window.
- **App restart during write**: `QSaveFile::commit()` is atomic, but
  the backup-rotate step is not. Always write the `.bak` first, then
  overwrite the live file. Crash between the two leaves both files
  intact and the user can restore manually.
- **The prototype's `INITIAL_STATE.apps` keys (`kate`, `tb`, `kons`,
  `firefox`, `discord`)** are short ids. The Qt app must not write
  those to TOML — use canonical program names (`kate`,
  `org.mozilla.thunderbird`, `org.kde.konsole`, `firefox`, `discord`)
  as the engine's `apps` map key. The display label/short-id mapping
  lives only in `apps_known.cpp`.

---

## 9. Total estimated work

- C++ backend: ~1200 lines across 9 small TUs (`config_store` is the
  bulkiest at ~400 lines).
- QML: ~2000 lines across 22 files (16 primitives + 6 pages + App +
  Theme).
- CMake + desktop file + main.cpp: ~150 lines.
- Engine scaffold (§U1.2 carry-over): unchanged from the original
  estimate — ~150 lines of header changes.
- Vendored toml++: 1 file, no changes.

Roughly 4–6 sessions if Claude Code follows the steps in §5
sequentially, or two large sessions if Steps 5/6/7/8 are bundled. The
single-session U1 budget in `UI-PLAN.md` does **not** fit the Qt6
pivot; budget at least three sessions, mapped through Step 0:

- U1a (Steps 0–4): plan housekeeping + engine scaffold + project +
  theme + window chrome + blur spike.
- U1b (Steps 5–7): primitives + ConfigStore + tests + EngineProbe
  wiring.
- U1c (Steps 8–12): pages + smoke test + final docs commit.

Each sub-session ends with one commit using its commit message from
§5 — keeps CLAUDE.md's "one commit per session" rule intact (which is
exactly why Step 0 amends `UI-PLAN.md` first).

Commits ladder cleanly with `git rebase` / squash if a single
`feat(settings): qt6 settings app` headline is preferred for the
release-notes-friendly history.
