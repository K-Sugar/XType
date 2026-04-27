# XType Config Migration Notes

## `tab_accepts_word` → `partial_accept` (Session U1a)

The config key `tab_accepts_word` has been renamed to `partial_accept` for
clarity. The xtype-settings UI reads both keys during a one-release deprecation
window:

- If `partial_accept` is present, it is used.
- If only `tab_accepts_word` is present, it is read and a warning is logged.
- Subsequent saves write only `partial_accept`.

`xtype-settings` (the GUI app in `settings/qt6-app/`) performs this migration
automatically: opening the app and letting it save once is sufficient.
The old key is never written back.

To migrate manually, update `~/.config/xtype/config.toml`:

```toml
# Old:
# tab_accepts_word = true

# New:
partial_accept = true
```
