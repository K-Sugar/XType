# XType — Claude Code Context

## Active plan
Engine-UI gap closure — sessions E0 through E6 (+ E1-UI).
Entry point: `PLANS/engine-gap-plan/Agent-Prompt.md`.
Shared context: `PLANS/engine-gap-plan/00-shared.md`.
Legacy general work: `PLAN.md` (Sessions 18–21 still open).

---

## Build commands

```bash
# Engine (fcitx5 addon)
cmake -B fcitx5-engine/build -G Ninja -S fcitx5-engine
ninja -C fcitx5-engine/build
ninja -C fcitx5-engine/build test          # Catch2

# Settings app (Qt6/QML)
cmake -B settings/qt6-app/build -G Ninja -S settings/qt6-app
ninja -C settings/qt6-app/build
ctest --test-dir settings/qt6-app/build
./settings/qt6-app/build/xtype-settings   # smoke run

# Reload engine after a build
fcitx5-remote -r
```

---

## Session workflow

1. Read the active plan's shared context and session file before writing anything.
2. Explore — read all relevant source files first; do not guess at APIs.
3. Call `advisor()` before writing any file (reading/exploring is not substantive work).
4. Code → build → run tests → fix failures. Never commit with failing tests.
5. After each step's Checkpoint/Verify block passes, commit with the verbatim
   message from the session file. Do not paraphrase.
6. Push to origin after every commit.
7. Self-critique: re-read your implementation against the session objectives and
   the §7 gotchas in 00-shared.md before declaring done.

---

## Hard rules — cross-cutting

- **No telemetry, no cloud.** Nothing leaves the local machine except Ollama HTTP
  (localhost only).
- **Corpus data is local-only.** Never log, transmit, or write corpus content
  outside `~/.local/share/xtype/`.
- **AI suggestions must never enter the corpus.** Only user-typed characters
  (via `appendChar`) may be recorded. `acceptNextWord`/`acceptAll` output is
  forbidden from corpus writes.
- **Password managers must stay hard-blocked for corpus collection** regardless
  of user config: `keepassxc`, `1password`, `bitwarden`, `gnome-keyring`,
  `seahorse`.
- **System prompt ≤ 2000 chars** (base + style exemplars + user description +
  per-app addendum combined). `buildSystemPrompt` enforces this via `kPromptBudget`.
- **focus-out must not commit ghost text.** Call `commitString("")` before
  `resetState()` in `deactivate`.
- **Never use destructive git ops** (force-push, reset --hard, branch -D) without
  explicit user confirmation.
- **Never commit with `--no-verify`.**

---

## Hard rules — engine (fcitx5-engine/)

- **C++17.** No exceptions in the hot path.
- **Never call fcitx5 APIs from `std::thread`.** Use
  `_instance->eventDispatcher().schedule()` or `scheduleWithContext()` to
  marshal back to the main thread.
- **Atomic file writes** for all engine-written JSON/data files: write to
  `.tmp`, then `std::filesystem::rename()` over the target.
- **No X11 input injection.** No `xdotool` or equivalent.

---

## Hard rules — settings app (settings/qt6-app/)

- **Qt6/QML only.** No Qt5 APIs. No QtWidgets.
- **ConfigStore is the single source of truth** for all settings. QML reads and
  writes only via `Config.*` (the `ConfigStore` context property). Do not bypass
  it with direct file I/O from QML.
- **Engine and settings app are separate processes** — they communicate only via
  the filesystem. Never attempt IPC, sockets, or direct API calls across them.
- **comingSoon controls still write to TOML** even when dimmed (the MouseArea
  fires; only opacity changes). Do not alter this behaviour when removing
  `comingSoon`.
- **`maximumLength` on description inputs** must match the engine's 500-char cap
  in `applyPrompt()`.
- **Slider debounce:** UI sliders must not write to TOML on every frame — use
  `onCommitted` (or a 300 ms timer) to debounce saves.

---

## Cross-process file contracts

| File | Path | Writer | Reader |
|------|------|--------|--------|
| `config.toml` | `~/.config/xtype/config.toml` | settings app | engine |
| `metrics.json` | `~/.local/share/xtype/metrics.json` | engine | settings app |
| `recent_events.json` | `~/.local/share/xtype/recent_events.json` | engine | settings app |
| `fcitx5.log` | `~/.local/share/xtype/fcitx5.log` | engine | settings app (LogTail) |
| `corpus.txt` | `~/.local/share/xtype/corpus.txt` | engine | settings app (CorpusStats) |
| `style_profile.json` | `~/.local/share/xtype/style_profile.json` | engine | settings app |
| `inference.log` | `~/.local/share/xtype/inference.log` | engine | settings app (LogTail) |

---

## Testing requirements

Both suites must be green before any commit that touches shared logic:

```bash
ninja -C fcitx5-engine/build test       # engine (Catch2)
ctest --test-dir settings/qt6-app/build # settings app
```

For engine-only changes, only the engine suite is required. For settings-only
changes, only the settings suite is required. When both are touched, both must pass.

---

## Agentic operating contract

- **One commit per step** (as defined in the session file). Do not bundle steps.
- **Verbatim commit messages** — copy the exact string from the "Commit:" line.
- **Call `advisor()` before writing any file.** Exploring and reading are free;
  writing is not.
- **If an observation contradicts the session file** (wrong signature, missing
  field, API mismatch): stop, document it, call `advisor()` before proceeding.
- **Update `PLANS/engine-gap-plan/00-shared.md §7`** when you discover a new
  gotcha or resolve an existing one. Do not silently deviate.
- **Scope discipline:** do not add features, refactor, or clean up code outside
  the active step. Small scope per step is intentional.
- Do NOT read or modify `prototype/` — it is a design reference only.
- Do NOT create `*.md` files unless the session body explicitly asks for one.

---

## Context management

Run `/compact` after the planning phase and before implementation on long sessions.
