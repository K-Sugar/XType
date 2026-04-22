# IBus engine — local rules
- Always use the venv at `ibus-engine/.venv` for all Python work — never use pacman to check Python packages
- Activate with: source ibus-engine/.venv/bin/activate
- All IBus API calls must happen on the GLib main thread
- Use GLib.idle_add() to marshal from inference thread
- Never import fcitx5 modules here
- Run tests with: cd ibus-engine && python -m pytest tests/

## Python environment
- Shell is Fish — activate with: source .venv/bin/activate.fish
- tomllib is stdlib (Python 3.11+) — never try to pip install it
- Venv location: ibus-engine/.venv
- Install deps: uv pip install -e ".[dev]"
- Run tests: python -m pytest tests/
- Never query pacman for Python package status — check with `python -c "import ibus"` or `uv pip list` instead
- IBus Python bindings come from the system GObject introspection — if `import gi` fails, the venv needs: `uv pip install pygobject`
