# Fcitx5 Engine — KDE Plasma Wayland Integration Testing

## Environment

- OS: CachyOS, KDE Plasma on Wayland
- Fcitx5: 5.1.19
- Engine: `libxtype-fcitx5.so` (Session 12)
- Model: `qwen2.5:1.5b` via Ollama

## One-time setup

### 1. Install the addon

```bash
cmake -B build -G Ninja -DFCITX_INSTALL_USE_FCITX_SYS_PATHS=ON
ninja -C build
sudo ninja -C build install
```

Installed files:
- `/usr/lib/fcitx5/libxtype-fcitx5.so`
- `/usr/share/fcitx5/addon/xtype.conf`
- `/usr/share/fcitx5/inputmethod/xtype.conf`

### 2. IM environment variables

`~/.config/environment.d/input-method.conf`:
```
QT_IM_MODULE=fcitx
GTK_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
```

### 3. KDE virtual keyboard

System Settings → Input & Output → Keyboard → Virtual Keyboard → **Fcitx5**

This tells KWin to launch fcitx5 as the Wayland IM compositor. Fcitx5 must be KWin-launched
to function on Wayland — running it as a standalone daemon will not work.

### 4. Disable IBus autostart

System Settings → Startup and Shutdown → Autostart — disable or remove any IBus entry
to prevent conflicts.

### 5. Log out / log back in

After logout+login: verify Ollama is running (`ollama list`), then verify fcitx5 loaded
XType via `fcitx5-configtool` (XType should appear in Input Methods).

---

## Test matrix

### Kate (Qt6 / text-input-v2)

| # | Step | Expected | Result |
|---|------|----------|--------|
| K1 | Open Kate, click in editor | Engine receives focus | Confirmed |
| K2 | Type a few words | Characters appear normally | Confirmed |
| K3 | Pause ~300ms | Ghost text (grey underline) appears | Confirmed |
| K4 | Press Tab | Next word of suggestion committed | Confirmed |
| K5 | Press Shift+Tab | Entire remaining suggestion committed | Confirmed |
| K6 | Type more, press Escape | Ghost text clears, nothing committed | Confirmed |
| K7 | Type more, press Backspace with ghost text | Ghost text clears, typed char stays | Confirmed |
| K8 | Click away to another window mid-suggestion | Suggestion dismissed (not inserted) | Confirmed |
| K9 | Click back into Kate | Fresh state, no stale preedit | Confirmed |

**Notes:**

---

### Firefox (GTK3 / text-input-v3)

Launched with `GTK_IM_MODULE=fcitx` until env vars take effect after next login.

| # | Step | Expected | Result |
|---|------|----------|--------|
| Z1 | Open Firefox, click into a text field | Engine receives focus | |
| Z2 | Type a few words | Characters appear normally | |
| Z3 | Pause ~300ms | Ghost text appears | |
| Z4 | Press Tab | Next word committed | |
| Z5 | Press Shift+Tab | Full suggestion committed | |
| Z6 | Switch tabs mid-suggestion | Suggestion dismissed cleanly | |
| Z7 | Click away mid-suggestion | Suggestion dismissed cleanly | |

**Notes:**

---

### Chromium (Blink / text-input-v1)

Launch with `--wayland-text-input-version=3` for preedit support.

| # | Step | Expected | Result |
|---|------|----------|--------|
| C1 | Open Chromium (with flag), click a text field | Engine receives focus | |
| C2 | Type and pause | Ghost text appears | |
| C3 | Tab / Shift+Tab | Accept word / all | |

**Notes:**

---

### Konsole (Qt6 — must be blocklisted)

| # | Step | Expected | Result |
|---|------|----------|--------|
| T1 | Open Konsole, click in terminal | No ghost text appears | |
| T2 | Type normally | No preedit, no glitches | |
| T3 | Type rapidly | No IM interference whatsoever | |

**Notes:**

---

### Electron app (Chromium / text-input-v1)

Launch with `ELECTRON_OZONE_PLATFORM_HINT=wayland` (e.g. VS Code, Obsidian).

| # | Step | Expected | Result |
|---|------|----------|--------|
| E1 | Open app, click a text field | Engine receives focus | |
| E2 | Type and pause | Ghost text appears | |
| E3 | Tab / Shift+Tab | Accept word / all | |

**Notes:**

---

### LibreOffice (GTK3 / text-input-v3)

| # | Step | Expected | Result |
|---|------|----------|--------|
| L1 | Open LibreOffice Writer, click in document | Engine receives focus | |
| L2 | Type and pause | Ghost text appears | |
| L3 | Tab / Shift+Tab | Accept word / all | |
| L4 | Click away mid-suggestion | Suggestion dismissed cleanly | |

**Notes:**

---

## Focus-out cross-app check

| # | Scenario | Expected | Result |
|---|----------|----------|--------|
| F1 | Kate → Alt+Tab mid-suggestion | Suggestion dismissed without inserting | |
| F2 | Firefox → click Kate | Suggestion dismissed cleanly | |
| F3 | Kate → Ctrl+W closes window | Suggestion dismissed before close | |

---

## Blocklist verification

`ic->program()` values observed (fill in from fcitx5 log):

| App | program() value | Blocked? |
|-----|----------------|----------|
| Konsole | | |
| Kate | | |
| Firefox | | |

---

## Known issues / observations

<!-- Fill in during testing -->
