# IBus Engine — End-to-End Testing (KDE Plasma / Wayland)

## Environment setup

### One-time: IM module env vars

`~/.config/environment.d/ibus.conf` (already written by Session 8):

```
QT_IM_MODULE=ibus
GTK_IM_MODULE=ibus
XMODIFIERS=@im=ibus
```

Log out and back in (or run `systemctl --user import-environment QT_IM_MODULE GTK_IM_MODULE XMODIFIERS`) for the vars to take effect in all apps.

### Start the engine

```bash
ibus-daemon --replace -d
cd /home/saint/Desktop/XType
source ibus-engine/.venv/bin/activate.fish
python -m ibus-engine.engine.main
```

Logs are written to `~/.local/share/xtype/engine.log`. Tail during a test run:

```bash
tail -f ~/.local/share/xtype/engine.log
```

---

## Test matrix

### Kate (Qt6 / text-input-v2)

| # | Step | Expected | Result |
|---|------|----------|--------|
| K1 | Open Kate, click in editor | Engine receives focus_in | Confirmed |
| K2 | Type a few words | Characters appear normally, no duplication | Confirmed |
| K3 | Pause ~300ms | Ghost text (grey underline) appears after cursor | Confirmed |
| K4 | Press Tab | Next word of suggestion committed inline | Confirmed |
| K5 | Press Shift+Tab (sends ISO_Left_Tab on Linux) | Entire remaining suggestion committed | Confirmed |
| K6 | Type more, then press Escape | Ghost text clears, no text committed | Confirmed |
| K7 | Type more, then press Backspace with ghost text visible | Ghost text clears, last typed char stays | Confirmed |
| K8 | Click away to another window mid-suggestion | Suggestion dismissed (not inserted into document) | Confirmed |
| K9 | Click back into Kate | Fresh state, no stale preedit | Confirmed |

**Issues / notes:**
- Predicted text is still missing prefix space after punctuation (.,?!) — model/prompt issue, deferred.

---

### Zen Browser (GTK3 / text-input-v3)

Requires a full logout/login for `~/.config/environment.d/ibus.conf` to take effect. Until then, launch with: `GTK_IM_MODULE=ibus zen-browser`.

| # | Step | Expected | Result |
|---|------|----------|--------|
| Z1 | Open Zen, click into address bar or text field | Engine receives focus_in |Confirmed, app_id not showing up correctly though |
| Z2 | Type a few words in a text field (e.g. search box or `about:blank` textarea) | Characters appear normally | Confirmed |
| Z3 | Pause ~300ms | Ghost text appears | Confirmed |
| Z4 | Press Tab | Next word committed | Confirmed |
| Z5 | Press Shift+Tab | Full suggestion committed | Confirmed |
| Z6 | Switch tabs (focus change) mid-suggestion | Suggestion dismissed cleanly (not inserted) | Confirmed |
| Z7 | Click away mid-suggestion | Suggestion dismissed cleanly (not inserted) | Confirmed |

**Issues / notes:**
- First run failed: `GTK_IM_MODULE` not set (env vars not imported before logout/login). Re-test after logout/login.

---

### Alacritty (terminal — must be blocklisted)

| # | Step | Expected | Result |
|---|------|----------|--------|
| A1 | Open Alacritty, click in terminal | No predicted text appears; engine passes all keys through | focus_in (fallback) is shown in logs, no False for all keys returned but no text predicted in Terminal window. Passed in my eyes |
| A2 | Type normally | No ghost text, no preedit glitches, no visual artifacts | Confirmed |
| A3 | Type rapidly | No interference from engine whatsoever | Confirmed |

**Issues / notes:**

---

## Focus-out commit — cross-app check

| # | Scenario | Expected | Result |
|---|----------|----------|--------|
| F1 | Kate → Alt+Tab to Alacritty mid-suggestion | Suggestion dismissed (log: `focus_out: suggestion=... — dismissing`) | Confirmed |
| F2 | Zen textarea → click Kate | Suggestion dismissed cleanly | Confirmed |
| F3 | Kate → Ctrl+W closes window | Suggestion dismissed before close | Confirmed |

---

## Log analysis checklist

After testing, review `~/.local/share/xtype/engine.log` for:

- [ ] `focus_in (fallback)` or `focus_in_id` logged on each app switch
- [ ] **Note:** On KDE Wayland, `do_focus_in_id` is never called (KWin IBus bridge doesn't forward client identity). `app_id` is always empty — blocklist is inactive. Alacritty passthrough works because terminals don't send printable chars through IM, not because of blocklisting.
- [ ] No Python tracebacks or `ERROR` level lines
- [ ] Inference requests fired after debounce delay (look for `_request_inference`)
- [x] `do_focus_out` dismiss lines visible when switching apps mid-suggestion
- [ ] No duplicate key events or double-commits

---

## Known issues / observations

- **Context accumulation without separator**: Accepted suggestion text is folded back into the ContextBuffer alongside new typed text, with no delimiter. The model receives run-on context (e.g. `"...this time!What isthe "`), which can degrade suggestion quality after multiple Tab-accepts. Deferred to a later session.
- **Missing prefix space after punctuation**: Model does not insert a leading space when context ends with `.,?!`. Prompt/post-processing fix, deferred.
- **Blocklist inactive on KDE Wayland**: `do_focus_in_id` is never called by the KWin IBus bridge — `app_id` is always empty. Alacritty passthrough works incidentally (terminals don't route printable chars through IM). Blocklist will need an alternative mechanism (e.g. KWin D-Bus query) for the Fcitx5 phase.
- **Zen Browser requires explicit `GTK_IM_MODULE=ibus`**: Until the next login, must launch with `GTK_IM_MODULE=ibus zen-browser`. After logout/login the `environment.d` conf takes effect automatically.
