# XType — Claude Code Context

## Current Phase
Phase B → Session 10: InferenceClient C++ (libcurl)

See Summary.md for full project context and architecture.
See PLAN.md for the session-by-session plan.

## Session workflow
1. Read PLAN.md, find the first session not marked [x]
2. Explore — read all relevant files before writing anything
3. Plan — summarise your approach; pause before touching more than 3 files
4. Code, run tests, fix failures
5. Self-critique: re-read implementation against session objectives + PLAN.md gotchas
6. Mark session [x], commit with exact message from PLAN.md, push

## Commits
- One commit per session, exact message from PLAN.md with additions if relevant or applicable
- Never commit with failing tests

## Hard rules
- Always use the venv for development to keep dependencies clean.
- All IBus/Fcitx5 API calls on main thread only — never from inference thread
- No X11 input injection (no xdotool)
- No cloud, no telemetry
- focus-out MUST commit preedit

## Context management
- Run /compact after planning, before implementation on long sessions
