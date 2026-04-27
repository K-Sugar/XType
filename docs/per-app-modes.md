# XType Per-App Modes

The `mode` field in each `[apps.<program>]` TOML section accepts these values:

| Value | Description |
|-------|-------------|
| `"Default"` | Use global settings (no per-app prompt modification) |
| `"Code-aware"` | Adds a code-aware prompt addendum; avoids prose completions |
| `"Email tone"` | Professional email tone; prefer formal phrasing |
| `"Casual"` | Casual/informal writing style |
| `"Off"` | Disable suggestions for this app (alias for `enabled = false`) |

The `mode` field is scaffolded in Session U1a and wired to prompt assembly in
Session 20. Until then, setting `mode` has no effect on engine behaviour.

## TOML example

```toml
[apps.kate]
mode = "Code-aware"

[apps.thunderbird]
mode = "Email tone"
debounce_ms = 300
```
