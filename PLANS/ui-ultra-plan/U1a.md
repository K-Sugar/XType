# Sub-session U1a — Foundation (Steps 0, 1, 2, 2.5, 3, 4)

> **Scope:** Steps 0–4 (5 commits). Plan housekeeping, engine scaffold,
> Qt project skeleton, blur-spike, Theme singleton, window chrome.
>
> **Prerequisite:** none (this is the first sub-session).
>
> **Deliverable at end:** binary `xtype-settings` opens a transparent,
> blurred (or fallback-grain) window with full chrome (titlebar +
> sidebar + content stack + modal + toast scaffolding). Sidebar
> metrics are zero placeholders; pages render as blank text. Engine
> structs (config / metrics / recent-events / phrase-blocklist) are
> scaffolded.
>
> **Reference:** read `00-shared.md` first. Per-step references below
> use `§N` to point at sections in `00-shared.md`.

---

## Step 0 — Amend PLAN.md / UI-PLAN.md before any code lands

CLAUDE.md mandates "one commit per session or major change, message
from PLAN.md". Each sub-session is one such commit, but the message
strings need to live in `UI-PLAN.md` first. Do this before any code:

- In `UI-PLAN.md`: rename `Session U1` → `Session U1a / U1b / U1c`
  with explicit headlines and the commit messages used by each
  sub-session's final step:
  - U1a final: `feat(settings): qt6 foundation + window chrome` (this
    sub-session bundles Steps 0–4 into one headline commit at end of
    Step 4; intermediate commits in §5 are for IDE-resume convenience
    and may be squashed before push).
  - U1b final: `feat(settings): primitives + config store + engine probe`.
  - U1c final: `feat(settings): qt6/qml standalone settings app — full prototype port`.
- In `PLAN.md`: collapse `Session 22` into a one-line note "(merged
  into Session U1c — see ui-ultra-plan/)".
- This file (`U1a.md`) and the sibling `U1b.md` / `U1c.md` are the
  working reference.

Commit (this step alone): `docs(plan): split session U1 into U1a/U1b/U1c for qt6 ui pivot`.

---

## Step 1 — Engine scaffold (UI-PLAN §U1.2)

§U1.2 has **not** landed yet — verified by reading
`fcitx5-engine/src/config.h`, which still has the legacy
`tab_accepts_word`, no `engine_enabled`, no `trigger_mode`, no
`UserPromptConfig`, no `apps` map, no `engine_metrics.h`, no
`recent_events.h`, no `phrase_blocklist.h`. **Execute §U1.2 in full
here, before any Qt-app work.** The Qt app's TOML writer in U1b
Step 6 assumes those new keys; without them, every save is
byte-corruption to a real user's existing config.

Files changed (identical to UI-PLAN §U1.5 engine block):
- `fcitx5-engine/src/config.h` — add `TriggerMode`, `AcceptKey`
  enums; extend `BehaviourConfig` (`engine_enabled`, `trigger_mode`,
  `accept_full_key`, `partial_accept` (renamed from
  `tab_accepts_word`), `esc_dismisses`, `blocked_phrases`); extend
  `InferenceConfig` (`threads`); add `UserPromptConfig`,
  `AppOverride`, `apps` map.
- `fcitx5-engine/src/engine_metrics.h` — NEW.
- `fcitx5-engine/src/recent_events.h` — NEW.
- `fcitx5-engine/src/phrase_blocklist.h` — NEW (stub).
- `fcitx5-engine/src/xtype.h` / `.cpp` — add accessors + members +
  scaffold metric increments + phrase-blocklist call.
- TOML loader: dual-key fallback for `tab_accepts_word` →
  `partial_accept` with `qWarning` on legacy form.
- Log path unification: write to `~/.local/share/xtype/fcitx5.log`.
- `docs/config-migration.md` — NEW (rename note).
- `docs/per-app-modes.md` — NEW (allowed mode strings).

Acceptance:
- `cmake -B build -G Ninja && ninja -C build && ninja -C build test`
  passes.
- `-Wall -Wextra -Wshadow` clean.
- Existing user `config.toml` with `tab_accepts_word = false` still
  loads, with one `qWarning` line.

Commit: `feat(engine): scaffold config + metrics + recent-events for settings UI`.

---

## Step 2 — Qt6 project scaffold

Files (new):
- `settings/qt6-app/CMakeLists.txt`
- `settings/qt6-app/src/main.cpp`
- `settings/qt6-app/qml/App.qml`
- `settings/qt6-app/qml/Theme.qml`
- `settings/qt6-app/data/xtype-settings.desktop`
- `settings/qt6-app/data/icons/xtype.svg`
- `settings/qt6-app/src/build_info.h.in`
- `settings/qt6-app/src/toml/toml.hpp` (vendored from
  `https://github.com/marzer/tomlplusplus/releases/download/v3.4.0/toml.hpp`)

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
    # … TUs added in U1b (config_store, engine_probe, …) — list grows then.
)

qt_add_qml_module(xtype-settings
    URI XType.Settings
    VERSION 1.0
    QML_FILES
        qml/App.qml
        qml/Theme.qml
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

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("xtype-settings");
    QGuiApplication::setOrganizationName("xtype");
    QQuickStyle::setStyle("Basic");                       // strip Breeze
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

Checkpoint:
`cmake -B build -G Ninja && ninja -C build && ./build/xtype-settings`
opens an empty transparent window.

Commit: `feat(settings): qt6 project scaffold + main + cmake`.

---

## Step 2.5 — Translucent-blurred-window spike (de-risk before §5–§8)

The single highest-risk visual assumption in this plan is that
`Qt::FramelessWindowHint` + `setColor(Qt::transparent)` +
`KWindowEffects::enableBlurBehind` actually produces the prototype's
glassy-purple chrome on Plasma 6 Wayland. Qt 6 + xdg-decoration + KWin
SSD/CSD negotiation has historically broken this combination; better
to know in 30 minutes than after 600 lines of primitives.

Spike steps (no commit — throwaway code on top of Step 2):

1. In `App.qml`, set `flags: Qt.FramelessWindowHint`,
   `color: "transparent"`, draw a single rounded `Rectangle` filling
   the window with a 0.4-alpha purple gradient, no children.
2. In `main.cpp` after `engine.loadFromModule(...)`, find the root
   window via `engine.rootObjects().first()->findChild<QWindow*>()`,
   then `KWindowEffects::enableBlurBehind(rootWindow, true)` if
   `KWindowSystem::isPlatformWayland()`. Wrap in
   `#ifdef HAVE_KF6_WINDOW_SYSTEM` (CMake adds the define when
   `KF6WindowSystem` is found).
3. Run on the target machine. Three outcomes:
   - **Blur visible** → keep KWin path; non-KDE fallback is the QML
     grain+gradient as planned. Continue to Step 3.
   - **Window transparent but no blur** (e.g. `gnome-shell`, `sway`)
     → expected; the QML fallback handles it. Continue.
   - **Server-side decoration we can't get rid of, OR transparency
     fails** → stop, spike a `QWaylandClientExtension`
     xdg-decoration override OR fall back to a non-translucent dark
     window. Update `00-shared.md` §4 #1 with the decision before
     proceeding.

The spike's job is decision support, not delivered code. After the
decision, leave one commented-out `KWindowEffects::enableBlurBehind`
line at the end of `main.cpp` to be uncommented in Step 4.

No commit for this step.

---

## Step 3 — Theme singleton + grain overlay + fonts

Files: `qml/Theme.qml` (singleton, `pragma Singleton`); `src/main.cpp`
(register `Theme` as singleton via `qmlRegisterSingletonType`); SVG
grain image provider in `src/main.cpp`; `qml/primitives/XGrain.qml`.

Workflow:

1. Add `scripts/gen-theme-colors.py` (run once; output committed) —
   uses Python `colour` package (or vetted CSS Color 4 reference) to
   convert each `oklch(L C H)` literal in `prototype.css :root` to a
   sRGB hex. Emits `settings/qt6-app/qml/theme_colors.json`.
2. `Theme.qml` inlines the values as `readonly property color` (drop
   the JSON read for simplicity; regenerate only when hue changes —
   rare).
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

(Hex values illustrative — regenerate from the script.)

`XGrain.qml` is a simple `Image { source: "image://grain/window";
fillMode: Image.Tile; opacity: theme.grain }`. The provider is
registered in `main.cpp` **before** `engine.loadFromModule()`
(see §8 gotcha "Image providers must register before
loadFromModule").

Acceptance: `import XType.Settings 1.0` and `Theme.purple` resolves to
a sensible RGBA inside a `Rectangle { color: Theme.purple }`. Grain
overlay tiles correctly and respects `theme.grain` opacity.

Commit: `feat(settings): theme singleton + grain provider + bundled fonts`.

---

## Step 4 — Window chrome (titlebar, sidebar, content stack, modal, toast)

Files: `qml/App.qml` (full version), `qml/primitives/XTitleBar.qml`,
`qml/primitives/XSidebar.qml`, `qml/primitives/XEngineDot.qml`,
`qml/primitives/XMiniBar.qml`.

Reproduce `prototype/index.html` lines 146–234 in QML:

- `ApplicationWindow` with
  `flags: Qt.FramelessWindowHint | Qt.Window`,
  `color: "transparent"`, default size 1180×760 (matches the
  prototype's `inset: 28px` on a 1240×820 outer).
- The window has a `Rectangle` child = `prototype.css .window` —
  radius 14, gradient stops via `LinearGradient`, KWin blur enabled
  via `Component.onCompleted: KWindowEffects.enableBlurBehind(...)`
  if available (uncomment the line left dormant in Step 2.5).
- `XTitleBar`: 36 px row — left engine-dot + state text, centered
  title "XType — Settings", right minimize / maximize / close (close
  hover turns red per `prototype.css .win-btn.close:hover`). Drag is
  `MouseArea { onPressed: window.startSystemMove() }`.
- `XSidebar`: 220 px column — brand block (gradient mark with 'X'
  glyph + name + version), `nav-item` repeater driven by a model
  identical to JSX `NAV`, spacer, status block (Model pill, CPU
  mini-bar, RAM mini-bar, engine running line). Active nav item gets
  a 2 px purple left border + tinted background.
- Content area: `StackView` (or `Loader` keyed by `currentPage`) so
  page-change re-runs the entry transition (`@keyframes page-in`).
- Modal: `Dialog` from QQC2 styled minimal — backdrop blur faked with
  tiled grain + dim rectangle. Toast: `Rectangle` anchored bottom +
  `SequentialAnimation` for slide-in/auto-dismiss. `z: 100` / `z: 50`
  to match.

For this step the page area is a placeholder
`Rectangle { color: "transparent"; Text { text: currentPage } }`.
Sidebar metrics are placeholder zeros — they bind to `EngineProbe` in
U1b Step 7.

Acceptance: window opens transparent (with KWin blur on KDE Wayland;
fallback grain on others), drag works, three traffic-light buttons
function, sidebar shows six nav items, switching nav animates the
page area in over 280 ms.

Commit: `feat(settings): window chrome + sidebar + titlebar`.

---

## End of U1a — before next sub-session

1. Verify all five commits on the branch:
   ```
   git log --oneline -n5
   # docs(plan): split session U1 into U1a/U1b/U1c for qt6 ui pivot
   # feat(engine): scaffold config + metrics + recent-events for settings UI
   # feat(settings): qt6 project scaffold + main + cmake
   # feat(settings): theme singleton + grain provider + bundled fonts
   # feat(settings): window chrome + sidebar + titlebar
   ```
2. Push to origin.
3. **For the next session, re-read only `00-shared.md` and `U1b.md`.**
   Do not load this file or `U1c.md` — saves ~1500 lines of context.
