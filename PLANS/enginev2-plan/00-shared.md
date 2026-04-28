# 00 — Shared reference (enginev2-plan)

Read this file at the start of every session in this plan. It holds
cross-cutting facts that all sessions depend on.

---

## §1. Root findings

Five diagnosed problems motivate this plan:

**F1 — Context window misalignment (150 chars, no sentence boundary).**
`requestInference()` takes the last 150 chars from `ContextBuffer` with a
raw `substr()` at xtype.cpp:382. This often starts mid-word or mid-clause.
V1 fixes this by (a) increasing the default to 400 chars and (b) adding a
`sentenceAligned()` helper that scans forward to the first post-boundary
character after that trim.

**F2 — Stop tokens prevent sentence completion.**
`InferenceConfig::stop_tokens` defaults to `{".", "!", "?", "\n"}` in
config.h:20. The model halts at the first clause-ending mark, so it can
never return a complete sentence. V2 changes the default to `{"\n"}` and
adds `max_sentences = 1` post-processing in `on_done` so the engine caps
output at one sentence without structurally blocking the model mid-word.

**F3 — `commonOpeners` and `avgSentenceLen` computed but never used.**
`StyleProfile::commonOpeners()` (style_profile.h:52) and
`StyleProfile::avgSentenceLen()` (style_profile.h:51) are always
populated when the profile is loaded, but neither is ever injected into
the system prompt or used to tune `num_predict`. V2 fixes both.

**F4 — Tail-echo stripping runs only in `on_done`, causing preedit flicker.**
The echo-stripping block at xtype.cpp:425–432 fires after the full stream
is complete. Any token that reproduces recently typed text stays visible
in the preedit until `on_done` removes it. V3 adds an incremental check
in the `on_token` callback.

**F5 — `AppOverride::model` read from TOML but never applied.**
`config_loader.cpp` reads per-app `model` at line 114 into
`AppOverride::model`, but `requestInference()` at xtype.cpp:390–392 only
picks up `num_predict` from the override. V4 adds the one-liner and wires
a model picker into the settings app.

---

## §2. Inference flow (read before touching xtype.cpp)

```
keyEvent()
  └─ debounce timer fires
       └─ requestInference(icRef, icPtr)
            ├─ ctx = _ctx.contextText()            // rolling 500-char deque
            ├─ [V1] sentenceAligned(ctx)           // trim to sentence start
            ├─ trim to context_window chars
            ├─ build InferenceConfig reqCfg
            ├─ [V4] apply AppOverride::model
            └─ _inference.request(ctx, reqCfg, on_token, on_done, on_error)
                 ├─ on_token (worker thread):
                 │    └─ eventDispatcher().scheduleWithContext(icRef, ...)
                 │         ├─ accumulate token into _ctx suggestion
                 │         ├─ [V3] incremental tail-echo check → suppress preedit if echo
                 │         └─ updatePreedit(icPtr)
                 └─ on_done (worker thread):
                      └─ eventDispatcher().scheduleWithContext(icRef, ...)
                           ├─ strip trailing whitespace
                           ├─ tail-echo strip (80-char, 4-char min)
                           ├─ head-echo strip (12-char prefix)
                           ├─ [V2] max_sentences truncation
                           └─ updatePreedit / recordLatency / writeRecentEvents
```

**Threading contract:** `on_token` and `on_done` are called from the CURL
worker thread. Every access to fcitx5 APIs or engine state must be
marshalled back to the main thread via
`_instance->eventDispatcher().scheduleWithContext(icRef, fn)`. Never
touch `_ctx`, `_metrics`, or any fcitx5 object directly from the callback.

---

## §3. Key files

| File | Role |
|------|------|
| `fcitx5-engine/src/config.h` | All config defaults: `context_window`, `num_predict`, `stop_tokens`, `max_sentences` (added in V2) |
| `fcitx5-engine/src/config_loader.cpp` | TOML → config struct; reads AppOverride fields including `model` (line 114) |
| `fcitx5-engine/src/xtype.cpp` | Engine core: `requestInference()`, `applyPrompt()`, `on_token`/`on_done` callbacks |
| `fcitx5-engine/src/inference_client.cpp` | Ollama HTTP client; `kBaseSystemPrompt` (line 31); `build_payload()` |
| `fcitx5-engine/src/prompt_builder.h` | `PromptInputs` struct; `buildSystemPrompt()` declaration |
| `fcitx5-engine/src/prompt_builder.cpp` | `assemble()` + `buildSystemPrompt()` implementation |
| `fcitx5-engine/src/style_profile.h` | `exemplars()`, `avgSentenceLen()`, `commonOpeners()` APIs |
| `fcitx5-engine/tests/` | Catch2 unit tests; each session adds or extends files here |
| `settings/qt6-app/src/` | Settings UI (only touched in V4) |

---

## §4. Config field inventory (current state before this plan)

```
InferenceConfig (config.h):
  std::string              model           = "qwen2.5:1.5b"
  int                      context_window  = 150         ← V1 bumps to 400
  int                      min_context_chars = 3
  int                      num_predict     = 30          ← V2.4 makes dynamic
  float                    temperature     = 0.4
  float                    top_p           = 0.9
  std::vector<std::string> stop_tokens     = {".", "!", "?", "\n"}  ← V2.1 → {"\n"}
  std::optional<int>       threads         = std::nullopt
  // NEW in V2:
  int                      max_sentences   = 1

AppOverride (config.h):
  std::string              app_name
  std::optional<int>       num_predict     ← already applied at runtime
  std::optional<std::string> model         ← config_loader reads; V4.1 applies
  std::optional<std::string> prompt_addendum

PromptInputs (prompt_builder.h):
  std::string              base
  const StyleProfile*      profile
  bool                     includeExamples
  std::string              userDescription
  std::vector<std::string> avoidPhrases
  std::size_t              budgetChars
  std::vector<std::string> exemplarsOverride
  // NEW in V2.3:
  std::vector<std::string> commonOpeners
```

---

## §5. Build commands

```bash
# Engine
cmake -B fcitx5-engine/build -G Ninja -S fcitx5-engine
ninja -C fcitx5-engine/build
ninja -C fcitx5-engine/build test

# Settings app (only needed for V4)
cmake -B settings/qt6-app/build -G Ninja -S settings/qt6-app
ninja -C settings/qt6-app/build
ctest --test-dir settings/qt6-app/build
./settings/qt6-app/build/xtype-settings
```

---

## §6. Hard rules

- **No fcitx5 API calls from `std::thread`.** Use
  `_instance->eventDispatcher().scheduleWithContext()` or
  `eventDispatcher().schedule()`.
- **System prompt ≤ 2000 chars.** `buildSystemPrompt` enforces via
  `kPromptBudget = 2000`. When adding `commonOpeners` to the prompt,
  it must fit within the same budget envelope.
- **Corpus/profile data is local-only.** Never log or transmit content
  from `_ctx`, corpus, or profile outside `~/.local/share/xtype/`.
- **AI-generated text must never enter the corpus.** Only
  `appendChar`-recorded characters may be harvested.
- **Atomic file writes** for all JSON/data files: write to `.tmp`, then
  `std::filesystem::rename()` over the target.
- **Never call `--no-verify`** on commits.
- **`commitString("")` before `resetState()`** in `deactivate` to prevent
  ghost text on focus loss.

---

## §7. Gotchas

- **`sentenceAligned()` must return the full string when no boundary is
  found.** Do not return an empty string — that would suppress the
  prediction entirely. The contract is: return a view starting at the
  first sentence-start boundary found scanning left-to-right; if none,
  return the full input.

- **`min_context_chars` check happens before sentenceAligned in V1.**
  After alignment the context may be shorter than `min_context_chars`. Do
  not re-check after alignment — the original length already passed the
  gate. The alignment is a trimming optimisation, not a new gate.

- **Changing stop_tokens default in V2.1 breaks existing
  `test_config_loader` assertions.** Grep for stop_token assertions
  before editing: `grep -n "stop_token" fcitx5-engine/tests/`. Update
  expectations in the test that checks defaults (not the round-trip test).

- **`max_sentences` post-processing must not strip trailing whitespace
  twice.** The existing on_done code already calls `s.pop_back()` on
  whitespace at line 421. Apply max_sentences truncation AFTER that strip,
  not before, so the terminator character `.`/`!`/`?` is actually present
  in the string when we scan for it.

- **`commonOpeners` in the prompt must count against the 2000-char
  budget.** The `assemble()` function in prompt_builder.cpp must include
  the openers line before the budget check, not after. Otherwise the
  prompt will silently exceed the budget when openers are long.

- **`avgSentenceLen` is in characters, not tokens.** When calibrating
  `num_predict` in V2.4, divide by 4 (rough chars-per-token estimate for
  English) to get a token count. Clamp to `[15, 60]` to avoid extreme
  values.

- **V4.1 model override must snapshot into `reqCfg` before passing to
  `_inference.request()`.** The worker thread reads `cfg` from the `Req`
  struct (not from `_cfg` directly, per E4.3 fix already in place). So
  setting `reqCfg.model` before `request()` is the correct pattern —
  the same pattern already used for `num_predict`.

- **V3: `snapPtr` must be `shared_ptr`, not a raw reference.** A new
  `request()` call can arrive while inner `scheduleWithContext` closures
  are still queued in the event dispatcher. Capturing the snapshot string
  by reference into the inner closure would produce a dangling reference
  if the outer lambda is destroyed first. The correct pattern: create
  `auto snapPtr = std::make_shared<std::string>(ctx)` before the move,
  then capture `snapPtr` by value in both outer and inner lambdas.

- **V4.2 settings UI: read the E4 session output (PageApps.qml) before
  writing any QML.** The per-app configuration UI was added in E4. The
  model picker goes inside the existing per-app override form, not as a
  new top-level section.

- **`test_xtype_helpers.cpp` already exists with `isBlocked` tests.**
  Session V1 says "Create" the file, but it already exists. Append the
  `sentenceAligned` test fixture (including the local replica of the
  helper) after the existing `isBlocked` tests. Do NOT replace the file.
  The CMakeLists.txt entry is also already present — skip that edit too.

---

## §8. Session ordering

```
V1 (context quality) ─┐
V2 (prompt/stop)     ─┤─▶ V4 (per-app model — depends on V2 max_sentences field)
V3 (echo suppression)─┘
```

V1, V2, and V3 are independent of each other and can run on separate
branches that rebase onto main before merging. V4 must follow V1–V3.

---

## §9. Acceptance summary (per session)

| Session | Done when |
|---------|-----------|
| V1 | Typing in the middle of a paragraph → context sent to model starts at a sentence boundary, not mid-word; verified via `XTYPE_DEBUG_VERBOSE=1` log |
| V2 | A typed sentence fragment receives a one-sentence completion (not a clause fragment); debug log shows updated stop tokens and prompt includes "often starts sentences with" when profile is loaded |
| V3 | No visible preedit flicker of typed text during streaming (previously the first token often echoed the last 4+ typed chars before being stripped in on_done) |
| V4 | Setting a per-app model in xtype-settings, clicking Reload, then typing in that app → debug log shows the override model name in the Ollama request |
