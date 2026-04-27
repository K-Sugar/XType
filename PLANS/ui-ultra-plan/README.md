# UI-ULTRA-PLAN — Qt6/QML XType Settings

> Goal: ship a standalone Qt 6 / QML desktop app at `settings/qt6-app/`
> that is a **pixel-and-behaviour-faithful port** of `prototype/index.html`,
> wired to the real engine surface (`config.toml`, `fcitx5-remote -r`,
> `~/.local/share/xtype/`, procfs, `ollama list`). After this plan executes,
> the React + Python bridge scaffolding described in UI-PLAN §U1.3–§U1.4 is
> obsolete — Session 22 ("Settings UI + per-app profiles") collapses into
> the same artefact and `prototype/` is kept only as a design reference.

## How to use these files

This plan is split across three sub-session files plus a shared
reference. **Read `00-shared.md` once at session start; then load only
the file matching the active sub-session.** Cross-references inside
each sub-session file point back to `00-shared.md` sections by §
number.

| File | Sub-session | Steps | Commits |
|---|---|---|---|
| `00-shared.md` | reference, all sessions | — | — |
| `U1a.md` | U1a (foundation) | 0, 1, 2, 2.5, 3, 4 | 5 |
| `U1b.md` | U1b (primitives + backend) | 5, 6, 7 | 3 |
| `U1c.md` | U1c (pages + ship) | 8, 10, 11, 12 | 4 |

Sub-sessions are **strictly sequential**: U1b assumes U1a is committed
to `main`; U1c assumes U1b is committed.

## Session prompt template

Paste at the top of each sub-session:

```
Read ui-ultra-plan/00-shared.md and ui-ultra-plan/<U1a|U1b|U1c>.md.
We are executing Sub-session <U1a|U1b|U1c>, Steps <X-Y>.
Stop after each step's checkpoint; commit with the message in that step.
Do not read the other sub-session files unless I ask.
```

## Source-of-truth note

The original single-file `UI-ULTRA-PLAN.md` at the repo root is kept
as a frozen reference; edits should land in this folder. If the two
diverge, the folder wins.
