
# Session U1a / U1b / U1c — Qt6/QML Settings App (pivoted from web prototype scaffold)

> **Pivot note (2026-04-27):** Session U1 has been split into three sub-sessions
> (U1a, U1b, U1c) and pivoted from the React+Python bridge approach to a
> standalone Qt 6 / QML native app. The working plan lives in `ui-ultra-plan/`.
> This file is kept as a **frozen reference** for the §U1.1–§U1.5 inventory and
> engine scaffold spec (§U1.2). If this file and `ui-ultra-plan/` diverge, the
> folder wins.
>
> **Sub-session headline commits (all merged ✓):**
> - [x] U1a (Steps 0–4): `feat(settings): qt6 foundation + window chrome`
> - [x] U1b (Steps 5–7): `feat(settings): primitives + config store + engine probe`
> - [x] U1c (Steps 8, 10–12): `feat(settings): qt6/qml standalone settings app — full prototype port`

## U1.1 — Inventory of prototype surfaces

Each table column:
- **UI control** — what the prototype renders today.
- **Status** — `exists` (already in `XTypeConfig` / engine), `partial` (data path partly built), `missing` (no backend at all).

## U1.1 — Inventory of prototype surfaces

Each table column:
- **UI control** — what the prototype renders today.
- **Status** — `exists` (already in `XTypeConfig` / engine), `partial` (data path partly built), `missing` (no backend at all).
- **Scaffold deliverable** — the field / method / endpoint to add this session.

### Page: General (`PageGeneral`)

| UI control | Status | Scaffold deliverable |
|---|---|---|
| Toggle "Enable XType globally" | missing (engine only enables/disables via fcitx5 layer) | `BehaviourConfig::engine_enabled` (default `true`); `keyEvent` checks it and short-circuits when false (TODO marker for later — for now just read it) |
| Segmented "Trigger: typing pause / manual ⌘." | missing | `enum class TriggerMode { Pause, Manual }`; `BehaviourConfig::trigger_mode = Pause`; engine never reads it yet |
| Slider "Trigger delay 100–800 ms" | exists | bind to `inference.debounce_ms` |
| Slider "Max suggestion length 4–48 tok" | exists | bind to `inference.num_predict` |
| Pills "Accept key Tab / Enter / →" | missing | `enum class AcceptKey { Tab, Enter, Right }`; `BehaviourConfig::accept_full_key = Tab`; key handler keeps Tab hard-coded for now, TODO marker for branch |
| Toggle "Partial accept (word-by-word)" | partial — collides with `tab_accepts_word` | rename `tab_accepts_word` → `BehaviourConfig::partial_accept` (semantic preserved); document the rename in `docs/config-migration.md` |
| Toggle "Dismiss on Esc" | missing | `BehaviourConfig::esc_dismisses = true`; key handler reads but currently always dismisses |
| Live demo box | missing | `GET /api/engine/recent` bridge endpoint returning a fixture list `[{typed, ghost, app, accepted, latency_ms, ts}]`; engine member `RingBuffer<RecentEvent, 32> _recent` with no producers wired yet |

### Page: Personalisation (`PagePersonalisation`)

Covered in detail in §1–§7 above. Scaffold-session deliverables that overlap:

| UI control | Scaffold deliverable |
|---|---|
| Stats: sentences / words / corpus bytes | `GET /api/learning/status` (already specified in §3) — return zeroed shape if no corpus |
| Toggle "Collect writing samples" | `learning.enabled` (exists) |
| Slider "Max corpus size" | `learning.max_corpus_mb` (exists) |
| Slider "Min sentence length" | `learning.min_sentence_chars` (exists) |
| Exemplars list | from `style_profile.json` (exists; Session 16) |
| "Common openers" pills | from `style_profile.json::common_openers` (exists) |
| "Always blocked" badge | hard-coded list in `corpus_collector.cpp::kAlwaysBlocked` (exists); bridge surfaces it via `/api/learning/status::always_blocked` |
| Personal prompt textarea / tone select / avoid-phrases | `UserPromptConfig{description, tone, avoid_phrases}` — Session 19 schema. **Add the struct in this scaffold session**, leave it unread by the engine; bridge round-trips |
| Reset / Export buttons | `xtype-corpus wipe --yes` and `style_profile.json` static download (specified in §3) |

### Page: Block list (`PageBlockList`)

| UI control | Status | Scaffold deliverable |
|---|---|---|
| Pills "Never suggest in pwd / 1pw / kpx / discord / konsole / + add" | exists | bind to `behaviour.blocklist_apps`; bridge `GET/PUT /api/config/behaviour/blocklist`; bridge `GET /api/apps/known` returns a curated suggestion list (kate, thunderbird, firefox, discord, libreoffice, konsole, alacritty, keepassxc, 1password, bitwarden) |
| "Add app" picker | missing — currently demo toast | bridge endpoint reuses `/api/apps/known`; UI picker reads it |
| Input "Block phrase" | missing | `BehaviourConfig::blocked_phrases: vector<string>`; `bool XTypeEngine::isPhraseBlocked(string_view tail) const` returning `false` (stub); regex-vs-literal detection is a future session — for scaffolding store raw strings |
| List "Blocked phrases" with × remove | missing | bridge `GET/PUT /api/config/behaviour/blocked_phrases` |

### Page: Per-app (`PagePerApp`)

| UI control | Status | Scaffold deliverable |
|---|---|---|
| App rows (kate, thunderbird, konsole, firefox, discord) with toggle | partial — Session 20 planned | introduce `XTypeConfig::apps: map<string, AppOverride>` early (so the UI has a real key to write to); `resolveForApp` returns base config (no overlay yet) |
| Per-app card: enabled toggle | partial | `AppOverride::enabled: optional<bool>` (Session 20 schema, scaffold the struct field) |
| Per-app card: mode select (Default / Code-aware / Email tone / Casual / Off) | missing | `AppOverride::mode: optional<string>`; engine ignores; document allowed values in `docs/per-app-modes.md` (mode → `prompt_addendum` mapping deferred to Session 20+) |
| Per-app card: suggestion length slider | missing | `AppOverride::num_predict: optional<int>` (extends Session 20 schema beyond what's currently planned) |

### Page: Model (`PageModel`)

| UI control | Status | Scaffold deliverable |
|---|---|---|
| Active model card (name, size, speed, quant) | partial | `GET /api/model/active` from bridge — combines `inference.model` with a static lookup table for size / family / typical-throughput. Add table in `settings/prototype/bridge.py` as data; *not* engine code |
| Stat: Latency | missing | `EngineMetrics{latency_p50_ms, latency_p95_ms}` struct in new `engine_metrics.h`; populated by no-op stubs; `GET /api/engine/metrics` returns zeros |
| Stat: RAM | missing | bridge reads `/proc/<ollama-pid>/status` if available; falls back to 0; same endpoint |
| Stat: CPU current | missing | same — bridge reads procfs sample over 200 ms; falls back to 0 |
| Segmented Quantisation (F16 / Q8 / Q4 / Q3) | not user-tunable (Ollama-managed) | UI keeps the segmented control but disabled in scaffold; tooltip "Managed by Ollama — choose a quantised model variant instead". No backend field |
| Slider Context window 512–4096 tok | exists | bind to `inference.context_window` |
| Slider Threads 1–12 | missing | `InferenceConfig::threads: optional<int>` (default unset = let Ollama decide); engine doesn't pass it yet |
| Available models list + Load button | missing | bridge `GET /api/models/available` calls `ollama list` and returns parsed rows; `POST /api/model/switch {name}` writes `inference.model` and triggers `fcitx5-remote -r` |
| Switch-model modal | already in UI | wire confirm to `POST /api/model/switch` |
| Import GGUF button | out of scope (Ollama handles via `ollama create -f Modelfile`) | scaffold: button shows toast `"Use \`ollama pull <model>\` from a terminal"` — no endpoint added |

### Page: About (`PageAbout`)

| UI control | Status | Scaffold deliverable |
|---|---|---|
| Version / build info | partial | `GET /api/build/info` returns `{version, git_commit, license, default_model}`; bridge reads from a generated `settings/prototype/build_info.json` (placeholder for now, regenerated on release) |
| GitHub / Release notes buttons | static links | UI hard-codes the URLs; no backend |
| Live logs feed | missing | `GET /api/logs/recent?limit=N` tails `~/.local/share/xtype/fcitx5.log` (engine logs to be unified to that path in this session — currently scattered across `engine.log` and `debug.log`) |
| Copy debug bundle | missing | `POST /api/diagnostics/bundle` returns a `.tar.gz` containing `config.toml`, last 1000 log lines, `style_profile.json` (sanitised — exemplars removed), `xtype-corpus stats --json`. Bridge endpoint **defined** but produces an empty tar in scaffold |
| Reset all settings | missing | `POST /api/config/reset` rewrites `config.toml` from `XTypeConfig{}` defaults. Confirm body required: `{"confirm": true}` |
| Report a bug | static link | UI hard-codes the URL |

### Sidebar status (always visible across pages)

| UI element | Status | Scaffold deliverable |
|---|---|---|
| Engine ready / paused dot | partial | `GET /api/engine/status` returns `{state: "ready" \| "paused" \| "error", message?: string}`. Engine doesn't actively push state; bridge derives from `fcitx5-remote -s` exit + ollama health-check |
| Active model pill | exists | reuses `/api/model/active::name` |
| CPU mini-bar live | missing | reuses `/api/engine/metrics::cpu_pct` |
| RAM mini-bar live | missing | reuses `/api/engine/metrics::ram_mb` |
| Engine running indicator (line) | partial | reuses `/api/engine/status` |

### Tweaks panel (designer-host only)

Out of scope. Hue / opacity / grain are CSS variables at design time; not part of the user-facing app. No scaffold needed.

---

## U1.2 — Engine scaffold work (C++17)

All in `fcitx5-engine/src/`:

### `config.h` — extend structs (no behaviour)

```cpp
enum class TriggerMode  { Pause, Manual };
enum class AcceptKey    { Tab, Enter, Right };

struct BehaviourConfig {
    bool                     engine_enabled       = true;        // NEW
    TriggerMode              trigger_mode         = TriggerMode::Pause;   // NEW
    AcceptKey                accept_full_key      = AcceptKey::Tab;       // NEW
    bool                     partial_accept       = true;                 // RENAMED from tab_accepts_word
    bool                     esc_dismisses        = true;                 // NEW
    bool                     passthrough_terminals= true;
    std::vector<std::string> blocklist_apps       = { /* unchanged */ };
    std::vector<std::string> blocked_phrases      = {};                   // NEW
};

struct InferenceConfig {
    // ...existing fields...
    std::optional<int>       threads;             // NEW — unset = Ollama default
};

struct AppOverride {
    std::optional<bool>        enabled;
    std::optional<std::string> model;
    std::optional<int>         debounce_ms;
    std::optional<int>         num_predict;       // NEW (extends Session 20)
    std::optional<std::string> mode;              // NEW — Default/Code-aware/Email tone/Casual/Off
    std::optional<std::string> prompt_addendum;
};

struct UserPromptConfig {                          // Session 19 schema, scaffold here
    std::string              description;
    std::vector<std::string> avoid_phrases;
    std::string              tone;
};

struct XTypeConfig {
    InferenceConfig                       inference;
    BehaviourConfig                       behaviour;
    LearningConfig                        learning;
    UserPromptConfig                      user_prompt;            // NEW
    std::map<std::string, AppOverride>    apps;                   // NEW (Session 20 schema)
};
```

Migration note: `tab_accepts_word` → `partial_accept` is a backwards-incompatible rename. Add a TOML loader fallback that reads either name during a one-release deprecation window; log on the legacy key.

### `engine_metrics.h` (NEW)

```cpp
struct EngineMetrics {
    std::atomic<int>      latency_p50_ms{0};
    std::atomic<int>      latency_p95_ms{0};
    std::atomic<int>      cpu_pct{0};
    std::atomic<long>     ram_mb{0};

    // Increment-on-acceptance counters scaffolded but unused.
    std::atomic<uint64_t> suggestions_generated{0};
    std::atomic<uint64_t> suggestions_accepted{0};
    std::atomic<uint64_t> chars_accepted{0};

    void snapshot(/*out*/ EngineMetricsSnapshot& s) const;
};
```

`xtype.h`/`.cpp`: add `EngineMetrics _metrics;` member. Increment `suggestions_generated` from `requestInference`, `suggestions_accepted` and `chars_accepted` from `acceptNextWord` / `acceptAll` (these are 1-line scaffold increments — actual rate computation lives in the bridge or a future session).

### `recent_events.h` (NEW)

```cpp
struct RecentEvent {
    std::string typed;
    std::string ghost;
    std::string app;
    bool        accepted;
    int         latency_ms;
    std::time_t ts;
};

class RecentEventsRing {
    // 32-slot lock-free-ish ring (mutex is fine; called rarely)
    void push(RecentEvent);
    std::vector<RecentEvent> snapshot() const;  // newest first
};
```

Engine scaffolds a member `RecentEventsRing _recent;` but **does not push** to it yet — the producers go in a follow-up session. The accessor exists so the bridge can pull a (currently empty) snapshot via a new `GET` method on the engine's existing fcitx5-remote-style status surface, OR — simpler — bridge serves only the fixture in scaffold and engine plumbing comes later.

### `xtype.h`/`.cpp` — public read-only accessors

```cpp
const EngineMetrics&      metrics() const noexcept;
const RecentEventsRing&   recentEvents() const noexcept;
const XTypeConfig&        config() const noexcept;
```

These are the surface the bridge talks to **once** Session 22's KCModule replaces the bridge. For now, the bridge reads config.toml directly; metrics/recent come from procfs and fixtures respectively. The accessors exist so the C++ ABI is stable.

### `phrase_blocklist.h` (NEW, body stub)

```cpp
class PhraseBlocklist {
public:
    void load(const std::vector<std::string>& phrases);  // store raw
    bool matches(std::string_view tail) const;           // returns false (stub)
};
```

`xtype.cpp::keyEvent` calls `if (_phraseBlock.matches(_ctx.contextText())) return;` — the stub never matches, so behaviour is unchanged. Future session implements regex-vs-literal compilation and matching.

---

## U1.3 — Bridge scaffold (`settings/prototype/bridge.py`)

Single Python 3.12 stdlib file. Endpoints with response shapes:

| Method · Path | Body / Response |
|---|---|
| `GET /api/build/info` | `{version, git_commit, license, default_model}` (from generated `build_info.json`) |
| `GET /api/engine/status` | `{state: "ready"\|"paused"\|"error", message?}` (derived from `fcitx5-remote -s` exit) |
| `GET /api/engine/metrics` | `{latency_p50_ms, latency_p95_ms, cpu_pct, ram_mb}` (zeros + procfs sample) |
| `GET /api/engine/recent?limit=N` | `[]` fixture, returns 4 sample rows so the live demo isn't empty |
| `GET /api/model/active` | `{name, family, size_mb, quant, throughput_estimate_tps}` (from static table keyed by `inference.model`) |
| `GET /api/models/available` | `[{name, family, size_mb, quant}]` from `ollama list` |
| `POST /api/model/switch` | `{name}` → write `inference.model` to TOML, trigger reload, return `{ok: true}` |
| `GET/PUT /api/config/behaviour` | full `BehaviourConfig` shape |
| `GET/PUT /api/config/inference` | full `InferenceConfig` shape |
| `GET/PUT /api/config/learning` | full `LearningConfig` shape (already in §3) |
| `GET/PUT /api/config/user_prompt` | full `UserPromptConfig` shape |
| `GET/PUT /api/config/apps` | `{[program]: AppOverride}` map |
| `GET /api/apps/known` | `[{id, label, category}]` curated list of apps for the picker |
| `GET /api/learning/status` | spec from §3 |
| `POST /api/learning/wipe` | spec from §3 |
| `GET /api/learning/profile.json` | spec from §3 |
| `GET /api/logs/recent?limit=N` | `[{ts, level, message}]` parsed from log file |
| `POST /api/diagnostics/bundle` | `application/gzip` tar of config + logs + sanitised profile + corpus stats; in scaffold returns an empty 1-byte tar |
| `POST /api/config/reset` | requires `{confirm: true}`; rewrites TOML from `XTypeConfig{}` defaults |
| `POST /api/engine/reload` | runs `fcitx5-remote -r` |

Every handler must:
- Return `{}` or zeros when the underlying source is unavailable, never 500.
- CORS-restrict to `127.0.0.1` only.
- Refuse mutating verbs without an `Origin: http://127.0.0.1:8765` header (cheap CSRF guard for a localhost service).

Static fixture file `settings/prototype/fixtures.json` holds the empty-state defaults for each endpoint, loaded on first request. Future sessions delete fixture branches as endpoints get real implementations.

---

## U1.4 — UI scaffold work (`settings/prototype/`)

- **`hooks.jsx` (NEW)** — extracted hooks: `useEngineStatus()`, `useEngineMetrics()`, `useLearningStatus()`, `useConfig(section)`, `useReloadEngine()`. Each polls its endpoint, debounces writes 500 ms, falls back to fixture on fetch error.
- **`proto-pages.jsx`** — every page replaces its `INITIAL_STATE`-driven local state with `useConfig(section)`. The shape of `s` and `set(key, value)` is preserved so the JSX doesn't change much; the values now come from / write to the bridge.
- **`index.html`** — the top-level `INITIAL_STATE` object is removed; `App` only owns `page` and `modal` state (everything else moves into hooks). The sidebar's CPU / RAM / model labels read from `useEngineMetrics` and `useEngineStatus`.
- **Sticky reload banner** — appears whenever any `useConfig` write resolves; click reloads engine, banner clears on success.
- **Disabled state for unimplemented controls** — Quantisation segmented, Threads slider, Trigger=Manual segmented, Accept-key Enter/→ pills, Per-app modes other than Default/Off render with a grey "coming soon" tooltip and `aria-disabled="true"`. They write to TOML so values persist, but the UI surfaces that the engine doesn't act on them yet.

---

## U1.5 — Files touched

```
fcitx5-engine/src/
├── config.h                      EDIT  add new structs and enums
├── engine_metrics.h              NEW   metrics struct + counters
├── recent_events.h               NEW   ring buffer struct
├── phrase_blocklist.h            NEW   stub class
├── xtype.h                       EDIT  add accessors + member instances
├── xtype.cpp                     EDIT  scaffold metric increments + phrase-blocklist call
└── (no other engine source files change)

settings/prototype/
├── bridge.py                     NEW   ~300 lines stdlib (extends UI-PLAN §1–§7)
├── fixtures.json                 NEW   empty-state defaults
├── hooks.jsx                     NEW   ~150 lines, useConfig / useMetrics / useStatus
├── proto-pages.jsx               EDIT  swap INITIAL_STATE for hooks
├── index.html                    EDIT  remove INITIAL_STATE, add reload banner
└── build_info.json               NEW   placeholder; release script regenerates

scripts/
└── xtype-corpus                  EDIT  add --json + --yes (already in §1–§7)

docs/
├── config-migration.md           NEW   tab_accepts_word → partial_accept rename note
└── per-app-modes.md              NEW   document the mode strings (semantic deferred)
```

No new C++ TUs (`.cpp`) — every new header is either header-only or its `.cpp` lands in a follow-up session. CMake gets the new headers in `add_library` for IDE / clangd indexing, no build cost.

---

## U1.6 — Acceptance criteria

A reviewer running through the prototype with the bridge attached must see:

- [ ] Every UI control either reflects a real config value or is visibly disabled with a "not yet wired" affordance.
- [ ] No fake numbers anywhere — every stat is either a real read or a zero.
- [ ] Toggling any wired control persists across `python bridge.py` restarts (TOML round-trip).
- [ ] The reload banner appears after any write and successfully reloads the engine on click.
- [ ] Empty-corpus / no-profile / no-Ollama states render without errors.
- [ ] Engine builds with no warnings on the new structs (`-Wall -Wextra -Wshadow`).
- [ ] Existing engine tests pass unchanged — scaffolds are read but not consumed in hot paths.
- [ ] No engine-thread API calls from the bridge process (bridge talks to the *filesystem*, never to the engine directly; engine accessors are reserved for the Session 22 in-process KCModule).

---

## U1.7 — Out of scope for this scaffold session

- Real metric collection (acceptance counters increment, but rate / time-saved math doesn't run anywhere).
- Real phrase-blocklist matching (regex vs literal).
- Real per-app overlay logic (Session 20 owns that).
- Real prompt-builder integration (Session 17 / 19 own that).
- KCModule / Qt6 native UI (Session 22 owns that — this session only touches the HTML prototype).
- Hot-reload without `fcitx5-remote -r`.
- Auth, telemetry, or any non-localhost bridge surface.

---

## U1.8 — Gotchas

- **Rename `tab_accepts_word` carefully.** Existing user TOMLs in the wild (per the README config snippet) use the old key. Loader must accept both names and warn on legacy. Drop the alias only after a release boundary.
- **`std::optional` in TOML.** The TOML library should serialise `nullopt` as a missing key, not `null`. Verify with toml++ before landing the new override fields, or per-app overlays will round-trip incorrectly.
- **`apps` is a `map<string, AppOverride>`.** Keys are lowercased program names. The UI sends the canonical program string from `ic->program()` (e.g. `kate`, `org.mozilla.thunderbird`); document that the UI must lowercase before write.
- **Disabled UI controls still write.** The Quantisation / Threads / Mode controls visibly disabled in scaffold still persist to TOML. That's intentional — when a future session wires them, the user's pre-set values activate immediately. Make sure the PUT handlers don't reject "unsupported" values; they're just opaque strings.
- **Bridge fixtures must look real.** A "live demo" feed of obvious lorem-ipsum reduces the prototype's value as a design artefact. Use the four `DEMO_SEQUENCES` from `proto-pages.jsx` as the fixture data so the live-demo box looks identical to its current state until the engine starts producing real events.
- **Don't unify `engine.log` and `debug.log` casually.** Both filenames appear in the README. Pick one (`~/.local/share/xtype/fcitx5.log`) and update the README in the same PR; otherwise log-tailing endpoints break for users on either path.
- **Reset-all is destructive.** `POST /api/config/reset` must back up the current `config.toml` to `config.toml.bak.<timestamp>` before overwriting. Document the location in the toast.
- **Sidebar metrics poll cadence.** The current prototype ticks CPU / RAM every 1.5 s. Bridge polling at 1.5 s spawns a procfs read per tick across two metrics — keep it, but coalesce reads server-side so one `GET /api/engine/metrics` call serves both bars.
