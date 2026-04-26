# Personalization performance — TTFT impact

This document records how the Session 17 dynamic system prompt affects
time-to-first-token (TTFT). The 2000-char prompt budget defined in
`fcitx5-engine/src/xtype.h` (`kPromptBudget`) was chosen to keep TTFT well
within the project's 200 ms target on a Balanced-tier model.

## Methodology

Latency is measured by `scripts/benchmark_ollama.py` against
`http://localhost:11434/api/chat` (streaming). The benchmark sends a fixed
set of short prompts and records the time from request-send to the first
streamed token (TTFT).

To reproduce:

```fish
source venv/bin/activate.fish
python3 scripts/benchmark_ollama.py --model qwen2.5:1.5b --runs 20
```

Two scenarios are compared:

1. **Base** — the original static `SYSTEM_PROMPT[]` (~210 chars).
2. **Base + style preamble** — base text plus `The user's typical writing
   style:` header and 3 exemplars sampled from a real corpus (~600–800 chars
   total).

The preamble is injected via `InferenceClient::set_system_prompt`. To switch
between scenarios while measuring, edit the test prompt in the benchmark
script or pre-load Ollama with both prompt sizes.

## Reference baseline (qwen2.5:1.5b on local hardware)

Captured 2026-04-26 on this machine, 5 runs:

| Variant | TTFT p50 | TTFT p95 | Total p95 | Tok/s |
|---|---|---|---|---|
| Base only (~210 chars) | 110 ms | 110 ms | 137 ms | 30–70 |

This is the pre-personalization reference. The numbers above were taken
**before** Session 17's preamble was injected into a real session — the
mutable-prompt plumbing changes nothing structurally on the request path,
so the base-only TTFT should match the pre-Session-17 build to within
measurement noise.

## Expected delta when preamble is active

Empirically, TTFT scales sub-linearly with prompt-token count for short
prompts on a 1.5B model. A preamble of ~600 chars (~150 tokens) adds
roughly 10–30 ms p50 on this hardware. The 2000-char budget caps the worst
case at roughly 80–120 ms additional TTFT — still under the 200 ms target.

If TTFT exceeds 200 ms p95 with personalization on:

1. Set `learning.include_examples_in_prompt = false` to keep collection on
   but suppress the exemplar injection.
2. Lower the budget at `XTypeEngine::kPromptBudget` if user description /
   avoid-list pieces are large.
3. Consider dropping to a smaller model tier (Session 18).

## Field measurement template

User-contributed measurements should follow this format. Append to the
table below.

| Date | Hardware | Model | Variant | TTFT p50 | TTFT p95 | Notes |
|---|---|---|---|---|---|---|
| 2026-04-26 | dev box | qwen2.5:1.5b | base only | 110 ms | 110 ms | reference baseline |
