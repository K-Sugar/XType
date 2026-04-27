# Sub-session U1c — Pages + ship (Steps 8, 10, 11, 12)

> **Scope:** Steps 8, 10, 11, 12 (4 commits). Six page implementations,
> KDE menu integration, smoke test on real Plasma 6 Wayland, and final
> docs housekeeping.
>
> **Prerequisite:** U1b complete on `main` (verify
> `ls settings/qt6-app/src/config_store.h` and
> `ctest --test-dir settings/qt6-app/build` is green).
>
> **Deliverable at end:** every page from `prototype/index.html` ports
> to QML pixel-close; smoke test passes on KDE Plasma 6 Wayland;
> `.desktop` entry surfaces in the app launcher; `PLAN.md` and
> `UI-PLAN.md` reflect U1 closure.
>
> **Reference:** `00-shared.md` §4 #10/#11/#13/#14/#15/#17/#20, §6
> acceptance criteria, §8 gotchas. **Do not** load `U1a.md` or
> `U1b.md`.
>
> **Note on Step 9:** Step 9 (Tweaks panel) is dropped per `00-shared`
> §4 #2 — designer-only and out of scope. Skip it; the numbering
> reflects the original linear plan.

---

## Step 8 — Pages

One file per page, each ~120 lines of QML. Implement in this order
(simplest → most state). All pages live under `qml/pages/`.

Additional C++ TUs needed (added to `qt_add_executable` source list
and to the Catch2 build if relevant):
- `src/ollama_client.{h,cpp}`
- `src/corpus_stats.{h,cpp}`
- `src/style_profile_model.{h,cpp}`
- `src/log_tail.{h,cpp}`
- `src/recent_events_model.{h,cpp}`
- `src/apps_known.{h,cpp}`

All registered as singletons under `XType.Settings` 1.0 with names:
`Ollama`, `CorpusStats`, `StyleProfile`, `Logs`, `RecentEvents`,
`AppsKnown`.

### 8.1 — PageGeneral.qml

Direct port of `proto-pages.jsx::PageGeneral`.

- `XSection 01 Engine`:
  - "Enable XType globally" → `XToggle` bound to `Config.engineEnabled`.
  - "Trigger" → `XSegmented` with values `pause` / `manual`. `manual`
    is `comingSoon`. Bound to `Config.triggerMode`.
  - "Trigger delay" → `XSlider 100..800 step 25` formatted `${v} ms`.
    Bound to `Config.debounceMs`.
  - "Max suggestion length" → `XSlider 4..48` formatted `${v} tok`.
    Bound to `Config.numPredict`.
- `XSection 02 Acceptance`:
  - "Accept key" → three `XPill`s `Tab` / `Enter` / `→`. Enter and →
    are `comingSoon`. Bound to `Config.acceptKey`.
  - "Partial accept (word-by-word)" → `XToggle` bound to
    `Config.partialAccept`.
  - "Dismiss on Esc" → `XToggle` bound to `Config.escDismisses`.
- `XSection Live preview`: embed `LiveDemo.qml`.

`LiveDemo.qml` reproduces `proto-pages.jsx::LiveDemo` lines 12–56 —
same four `DEMO_SEQUENCES`, same phase machine, same caret. Reads
`RecentEvents.first()` if available (the ring is empty in scaffold
since §U1.2 has no producers); falls back to fixture cycle. Pauses
when `!Config.engineEnabled` or any modal is open.

### 8.2 — PageBlockList.qml

Direct port of `PageBlockList`.

- `XSection 01 Never suggest in…`:
  - Five curated `XPill`s from `AppsKnown.curatedBlockerCandidates()`
    (canonical: `keepassxc`, `1password`, `bitwarden`, `discord`,
    `org.kde.konsole`). Display labels per §4 #11 lookup. Bound to
    `Config.blocklistApps` — clicking toggles membership.
  - `+ add app` `XPill` (`add` variant) opens an `AppPicker` popup
    that lists `AppsKnown.knownApps()` minus already-blocked. Picker
    scans `~/.local/share/applications/*.desktop` plus the curated
    list.
- `XSection 02 Blocked phrases`:
  - `XInput` with `Block` `XButton` adds to `Config.blockedPhrases`.
  - Repeater of `XPhraseRow` rows; × removes from list.
  - Empty state: "No phrases blocked yet." (`prototype.css .empty`).

### 8.3 — PagePerApp.qml

Direct port of `PagePerApp`.

- Two-column grid (50/50).
- Left: `Repeater` of `XAppRow` over `AppsKnown.knownApps()` (curated
  five for scaffold: kate, thunderbird, konsole, firefox, discord).
  Click sets local `selectedApp`. Each row has a per-app `XToggle`
  bound to `Config.apps[id].enabled`.
- Right: `XCard` showing
  - "Configuring · {label}" eyebrow.
  - "Enabled in this app" `XToggle`.
  - "Mode" QQC2 `ComboBox` styled to match `prototype.css .input` —
    options: `Default`, `Code-aware`, `Email tone`, `Casual`, `Off`.
    `comingSoon=true` for non-`Default`/`Off` values; tooltip
    "Per-app mode — engine still uses global prompt (Session 20)".
  - "Suggestion length" `XSlider 4..48` `comingSoon=true` tooltip
    "Per-app override — engine still uses global value (Session 20)".

Writes go through `Config.setApp(id, key, value)` which updates the
QVariantMap → triggers debounced TOML save. Use canonical program
names as keys (§4 #11) — display labels never reach the TOML.

### 8.4 — PageModel.qml

Direct port of `PageModel`.

- `XSection 01 Active model`:
  - `XCard.accent`. Reads `Ollama.activeMeta(Config.model)` →
    `{name, family, size_mb, quant, throughput_estimate_tps}` from a
    static lookup table baked into `ollama_client.cpp` (port the
    UI-PLAN §U1.3 model table).
  - Stat grid: Latency / RAM / CPU. Latency bound to
    `Engine.latencyP50` (§U1.2 scaffold returns 0). RAM:
    `Engine.ramGb`. CPU: `Engine.cpuPct`.
- `XSection 02 Inference settings`:
  - "Quantisation" `XSegmented` `F16/Q8/Q4/Q3` — all `comingSoon`
    (§4 #13).
  - "Context window" `XSlider 512..4096 step 256` bound to
    `Config.contextWindow`.
  - "Threads" `XSlider 1..12` `comingSoon`. Bound to `Config.threads`
    (QVariant → null means unset).
- `XSection 03 Available models`:
  - `Repeater` of `XAppRow` over `Ollama.availableModels()` minus
    active model. Async load via `QProcess "ollama list"` with 2 s
    timeout (§4 #17).
  - Click row → modal: `Switch to {name}? XType will pause for ~5
    seconds while the new model loads.` On confirm:
    `Config.setModel(name); Reloader.reload()`.
  - "+ Import GGUF model" row → toast "Use `ollama pull <model>` from
    a terminal".
  - Empty state when `Ollama.availableModels().length === 0`:
    "Ollama not detected — install with `ollama serve` or run
    `ollama pull qwen2.5:1.5b`".

### 8.5 — PagePersonalisation.qml

Direct port of `PagePersonalisation`.

- Stat grid (3 cards): "Words learned" / "Accept rate · 7d" /
  "Time saved · 7d" with `XBars` mini bar charts.
  - Words: `CorpusStats.words` — currently 0 from `xtype-corpus stats
    --json`.
  - Accept rate: 0 (no producers in §U1.2 metrics yet).
  - Time saved: 0.
- `XSection 01 Style learning`:
  - "Learn from accepted text" → `XToggle` bound to
    `Config.learningEnabled`. Description shows
    `${CorpusStats.sentences.toLocaleString()} sentences absorbed`.
  - "Voice match strength" → `XSlider 0..100 step 5`. **No engine
    field exists yet** — bind to a new
    `Q_PROPERTY(int voiceStrength)` on ConfigStore that writes to a
    `[learning] voice_strength` key (extend the engine
    `LearningConfig` struct in this step; ~5 lines in `config.h`).
    Mark `comingSoon=true` since the engine doesn't read it yet.
  - "Forget after 30 days" → `XToggle`; same treatment — extend
    `LearningConfig::forget_after_days` (default 0 = disabled),
    `comingSoon=true`.
- `XSection 02 Your voice profile`:
  - `XCard` with prose. If `StyleProfile.exemplars().length > 0`,
    show first 3 exemplars with purple-soft accents. Else show the
    static prose from JSX line 159 ("Your writing tends to be
    concise…").
  - Buttons: "Reset profile" → confirm modal → on confirm:
    `QProcess::startDetached("xtype-corpus", {"wipe", "--yes"})` →
    refresh `CorpusStats` → toast "Corpus wiped — backup at …".
    "Export profile" → `QFileDialog::getSaveFileName` and copy
    `~/.local/share/xtype/style_profile.json` to chosen path.

### 8.6 — PageAbout.qml

Direct port of `PageAbout`.

- Brand card: 64×64 gradient swatch with 'X' glyph, name "XType",
  version line `v{VERSION} · Fcitx5 engine · MIT License` from
  `build_info.h`. Two `XButton`s: "GitHub" and "Release notes" open
  configured URLs (`data/links.json`, generated by CMake from
  `XTYPE_REPO_URL` cache variable).
- `XSection 01 Live logs`:
  - `XLogList` bound to `Logs.lines` (200-line ring; § 4 #14).
    Source: `~/.local/share/xtype/fcitx5.log` with fallback to
    `engine.log`/`debug.log` (§4 #15). Lines containing
    `suggest accepted` get `accent` colour; lines with `error`/`fail`
    get `err` colour.
- `XSection 02 Diagnostics`:
  - "Copy debug bundle" → in-process bundler that writes
    `/tmp/xtype-bundle-<ts>.tar.gz` containing `config.toml`, last
    1000 log lines, sanitised `style_profile.json` (exemplars
    stripped via small JSON pass), `xtype-corpus stats --json`
    output. Show toast with the path.
  - "Reset all settings" → hard-confirm modal → `Config.resetAll()`
    (§4 #20).
  - "Report a bug" → `QDesktopServices::openUrl` to GitHub Issues.

Acceptance per page: every control matches the prototype's visual,
every wired control persists across app restart, every `comingSoon`
control shows the disabled affordance, every page renders cleanly
with empty state (no corpus, no profile, no ollama).

Commit: `feat(settings): pages — general, blocklist, per-app, model, personalisation, about`.

---

## Step 10 — `.desktop` entry + KDE menu integration

File: `data/xtype-settings.desktop` (already created in U1a Step 2 as
a placeholder — fill it out now):

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

Acceptance: `cp data/xtype-settings.desktop
~/.local/share/applications/` → entry appears in Plasma's app
launcher under Settings; click launches the binary.

Commit: `feat(settings): kde menu entry`.

---

## Step 11 — Build & install + smoke test

`README.md` (one paragraph) under `settings/qt6-app/`:

```
cd settings/qt6-app
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/xtype-settings        # run from source
sudo ninja -C build install   # system install
```

### Smoke test checklist (live, on KDE Plasma 6 Wayland)

1. Launch app cold — window opens transparent + blurred (or
   grain-fallback on non-KDE).
2. Drag titlebar — window moves; KWin snapping engages.
3. Click each of 6 nav items — page transitions in ~280 ms with the
   page-in animation.
4. Toggle "Enable XType globally" → reload banner appears → click
   reload → fcitx5 reloads (verify in
   `journalctl --user -u plasma-fcitx5` or live logs panel) →
   banner clears.
5. Drag every slider — value debounces, committed value persists
   across app restart.
6. Quantisation / Threads / Trigger=Manual / Accept=Enter / Per-app
   non-Default mode — controls react and persist but show "coming
   soon" tooltip.
7. Stop ollama (`pkill ollama`) — Model page goes to empty state
   without crashing; sidebar CPU/RAM drops to 0.
8. With ollama running: switch model — modal confirms, app pauses
   ~5 s while reload completes, sidebar model pill updates.
9. Wipe corpus from Personalisation → confirm modal → corpus.txt
   removed → stats card drops to zeros.
10. Reset all settings → confirm modal → backup file
    `config.toml.bak.<ts>` exists → all controls return to defaults.
11. Open About → live logs scrolls. Trigger an event in fcitx5 (type
    in Kate) → new line appears within 1 s.
12. `qmllint qml/**/*.qml` clean.
13. `cmake --build build -- -k 0` clean
    (`-Wall -Wextra -Wshadow -Werror=return-type`).

Cross-reference each item against `00-shared.md` §6 acceptance —
every checkbox there should be ticked before the next commit.

Commit: `feat(settings): qt6/qml standalone settings app — full prototype port`.

This is the headline commit that ends the U1 scope.

---

## Step 12 — Update plans + collapse Session 22

- Mark `[x] Session U1c` (and `U1a`/`U1b` retroactively if not yet)
  in `UI-PLAN.md`.
- In `PLAN.md`: rename Session 22 to "(merged into Session U1c — see
  ui-ultra-plan/)", or strikethrough.
- Update `README.md`'s "Configuration" section to document
  `xtype-settings` as the recommended path; TOML editing kept as a
  CLI fallback.
- Update `docs/config-migration.md` (created in U1a Step 1) to note
  that `xtype-settings` automatically migrates the legacy
  `tab_accepts_word` key on first save.

Commit: `docs(settings): collapse session 22 into u1; document xtype-settings`.

---

## End of U1c — final verification

1. Verify four commits on the branch:
   ```
   git log --oneline -n4
   # feat(settings): pages — general, blocklist, per-app, model, personalisation, about
   # feat(settings): kde menu entry
   # feat(settings): qt6/qml standalone settings app — full prototype port
   # docs(settings): collapse session 22 into u1; document xtype-settings
   ```
2. Re-run the smoke test top-to-bottom; tick every box in
   `00-shared.md` §6.
3. `ninja -C fcitx5-engine/build test` still green.
4. `ctest --test-dir settings/qt6-app/build` still green.
5. Push to origin.

UI-PLAN U1 is closed. Next phase work resumes from
`PLAN.md`'s remaining unblocked sessions.
