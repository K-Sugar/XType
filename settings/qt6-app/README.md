## XType Settings

Standalone Qt 6 / QML settings app for the XType ghost-text engine.

```
cd settings/qt6-app
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/xtype-settings        # run from source
sudo ninja -C build install   # system install (copies binary + .desktop + icon)
```

Configuration is stored at `~/.config/xtype/config.toml`. The app writes changes
automatically; click **Reload** in the banner to apply them to the running engine.
TOML editing remains supported as a CLI fallback — the app migrates the legacy
`tab_accepts_word` key to `partial_accept` on first save.
