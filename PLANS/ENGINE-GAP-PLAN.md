# XType — Engine ↔ UI Gap Plan

## Critical finding

The fcitx5 engine (`fcitx5-engine/`) **does not read from the config
TOML that the settings app writes.** `XTypeEngine::_cfg` is constructed
with `XTypeConfig{}` defaults; no TOML loader exists anywhere in the
engine source. All UI controls that write to `~/.config/xtype/config.toml`
are currently decorative — the engine runs on hardcoded defaults.

**Session E0 (TOML loader) must land before any other session has
functional effect.**

## Context

- Settings UI: built and shipped in `ui-ultra-plan/` (U1a/U1b/U1c — complete)
- Engine: operational, but uses compile-time defaults for all settings
- Previous plan: `ui-ultra-plan/` and `UI-ULTRA-PLAN.md` — keep as history
- Branch for new work: `main` (or a new feature branch per session)

## Secondary gaps (all blocked on E0)

| Gap | Source | UI Status | Engine Status |
|-----|--------|-----------|---------------|
| `accept_full_key` Enter/→ | `config.h` | comingSoon | Tab only wired |
| `threads` | `inference_client.cpp` | comingSoon | not in Ollama payload |
| Phrase blocklist | `phrase_blocklist.h` | wired | stub (always false) |
| Latency metrics | `engine_metrics.h` | scaffold 0 | never calculated |
| Recent events | `recent_events.h` | scaffold empty | push() never called |
| `user_prompt.*` | `prompt_builder.h` | no UI | fields unread in applyPrompt() |
| `voice_strength` | `config.h` | comingSoon | field exists, not used |
| `forget_after_days` | `config.h` | comingSoon | field exists, not used |
| Per-app overrides | `config.h` | comingSoon | AppOverride never applied |
| `temperature/top_p` | `inference_client.cpp` | no UI | wired to Ollama (only from defaults) |
| `stop_tokens` | `inference_client.cpp` | no UI | wired to Ollama (only from defaults) |

## Sessions

| ID | Title | Scope | Blocks | Status |
|----|-------|-------|--------|--------|
| **E0** | Engine TOML config loader | Engine only | All others | — |
| **E1** | Engine completions batch | Engine only | E1-UI | — |
| **E1-UI** | UI unlock: accept key + threads | Settings only | — | — |
| **E2** | Observability bridge | Engine + Settings | — | — |
| **E3** | User prompt integration | Engine + Settings | — | — |
| **E4** | ComingSoon removals: voice + forget + per-app | Engine + Settings | — | — |
| **E5** | New UI controls: advanced inference + learning knobs | Settings only | — | — |
| **E6** | Engine code quality | Engine only | — | — |

## Session files

```
engine-gap-plan/
├── 00-shared.md        cross-cutting context (read first in every session)
├── E0.md               Session E0 — engine TOML config loader (CRITICAL)
├── E1.md               Session E1 — accept key / phrase blocklist / threads engine
├── E2.md               Session E2 — latency + recent events observability
├── E3.md               Session E3 — user prompt engine wiring + voice UI
├── E4.md               Session E4 — voice_strength / forget_after_days / per-app
├── E5.md               Session E5 — advanced inference UI + learning knobs UI
├── E6.md               Session E6 — engine code quality fixes
└── Agent-Prompt.md     meta-prompt for agentic sub-session execution
```

## Acceptance — overall done when:

- [ ] Changing any control in `xtype-settings`, clicking Reload, and typing in an app
      produces behaviour that matches the new setting (not compile-time defaults).
- [ ] Every comingSoon control in the UI is either wired and green, or removed and
      replaced by a clearly-documented future-session stub.
- [ ] All engine unit tests green (`ninja -C fcitx5-engine/build test`).
- [ ] All settings app unit tests green (`ctest --test-dir settings/qt6-app/build`).
- [ ] No `-Wall -Wextra -Wshadow -Werror=return-type` errors in either build.
