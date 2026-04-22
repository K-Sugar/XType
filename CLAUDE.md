# XType — Claude Code Context

See Summary.md for full project context, architecture decisions, and tech stack.

## Current Phase
Phase A → Session 4: IBus engine skeleton + XML registration

## Session workflow
1. Read `PLAN.md` and find the first session not marked `[x]`
2. Complete all objectives for that session
3. Mark it `[x]`, commit with the exact message format in PLAN.md, push

## Key conventions
- Python: use `uv` for deps, `pyproject.toml`, no requirements.txt
- All IBus API calls must happen on the main GLib thread (use GLib.idle_add from inference thread)
- Ollama runs at http://localhost:11434
- Target <200ms time-to-first-token

## Do not
- Use xdotool or any X11 input injection
- Call IBus/Fcitx5 APIs from background threads
- Add cloud/telemetry of any kind
