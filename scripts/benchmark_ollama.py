#!/usr/bin/env python3
"""Standalone TTFT latency benchmark for the Ollama inference backend.

Usage:
    python3 benchmarks/latency_benchmark.py
    python3 benchmarks/latency_benchmark.py --model qwen2.5:1.5b --runs 20
    python3 benchmarks/latency_benchmark.py --model mistral:7b-instruct --runs 5

Measures time-to-first-token (TTFT) and full-generation latency using the
same streaming /api/generate endpoint and prompt settings as inference.py.
"""

import argparse
import asyncio
import json
import statistics
import sys
import time

import aiohttp

OLLAMA_HOST = "http://localhost:11434"
DEFAULT_MODEL = "qwen2.5:0.5b"
TTFT_TARGET_MS = 200

_SYSTEM_PROMPT = (
    "Output ONLY the completion text, no explanation. "
    "5-15 words max. Stop at sentence boundaries."
)
_PROMPTS = [
    "The quick brown fox jumps over",
    "In machine learning, a neural network",
    "The capital of France is",
    "To be or not to be,",
    "Artificial intelligence will soon",
    "The Python programming language was",
    "Once upon a time in a land",
    "The Eiffel Tower stands at",
    "Quantum computing differs from classical",
    "The human brain contains approximately",
]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _percentile(data: list[float], pct: float) -> float:
    sorted_data = sorted(data)
    idx = max(0, min(len(sorted_data) - 1, int(pct / 100 * len(sorted_data))))
    return sorted_data[idx]


async def _health_check(session: aiohttp.ClientSession, model: str) -> bool:
    try:
        async with session.get(f"{OLLAMA_HOST}/api/tags") as resp:
            data = await resp.json()
            available = [m["name"] for m in data.get("models", [])]
            if model not in available:
                print(f"[ERROR] Model '{model}' not found.")
                print(f"        Available: {available}")
                print(f"        Run: ollama pull {model}")
                return False
            return True
    except aiohttp.ClientConnectorError:
        print(f"[ERROR] Ollama not reachable at {OLLAMA_HOST}")
        print("        Is `ollama serve` running?")
        return False


async def _run_single(
    session: aiohttp.ClientSession,
    model: str,
    prompt: str,
) -> tuple[float, float, int, str]:
    """Returns (ttft_ms, total_ms, token_count, generated_text)."""
    payload = {
        "model": model,
        "prompt": prompt,
        "system": _SYSTEM_PROMPT,
        "stream": True,
        "options": {
            "num_predict": 30,
            "temperature": 0.3,
            "top_p": 0.9,
            "stop": [".", "!", "?", "\n"],
        },
    }

    t_start = time.perf_counter()
    ttft: float | None = None
    tokens = 0
    text = ""

    async with session.post(
        f"{OLLAMA_HOST}/api/generate", json=payload
    ) as resp:
        resp.raise_for_status()
        async for raw in resp.content:
            raw = raw.strip()
            if not raw:
                continue
            data = json.loads(raw)
            token = data.get("response", "")
            if token:
                if ttft is None:
                    ttft = (time.perf_counter() - t_start) * 1000
                tokens += 1
                text += token
            if data.get("done"):
                break

    total_ms = (time.perf_counter() - t_start) * 1000
    return (ttft or total_ms, total_ms, tokens, text.strip())


# ---------------------------------------------------------------------------
# Cancellation smoke-test
# ---------------------------------------------------------------------------


async def _test_cancel(session: aiohttp.ClientSession, model: str) -> None:
    """Fire a request, cancel after first token, verify no crash."""
    payload = {
        "model": model,
        "prompt": "Write a very long essay about the history of computing",
        "system": _SYSTEM_PROMPT,
        "stream": True,
        "options": {"num_predict": 200, "temperature": 0.3},
    }
    received = 0
    try:
        async with session.post(
            f"{OLLAMA_HOST}/api/generate", json=payload
        ) as resp:
            resp.raise_for_status()
            async for raw in resp.content:
                raw = raw.strip()
                if not raw:
                    continue
                data = json.loads(raw)
                if data.get("response"):
                    received += 1
                if received >= 3:
                    break  # simulate cancel-on-new-request
    except Exception as exc:
        print(f"  [cancel test] unexpected error: {exc}")
        return
    print(f"  cancel test: received {received} tokens then aborted — OK")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


async def main(model: str, n_warmup: int, n_runs: int, skip_cancel: bool) -> None:
    print("=" * 60)
    print("  XType — Ollama Latency Benchmark")
    print("=" * 60)
    print(f"  Model   : {model}")
    print(f"  Host    : {OLLAMA_HOST}")
    print(f"  Target  : TTFT < {TTFT_TARGET_MS} ms")
    print(f"  Warmup  : {n_warmup}  Runs: {n_runs}")
    print()

    async with aiohttp.ClientSession(
        timeout=aiohttp.ClientTimeout(connect=5.0, total=60.0)
    ) as session:
        if not await _health_check(session, model):
            sys.exit(1)

        # --- Warmup ---
        print(f"Warmup ({n_warmup} run{'s' if n_warmup != 1 else ''}):")
        for i in range(n_warmup):
            prompt = _PROMPTS[i % len(_PROMPTS)]
            ttft, total, tok, text = await _run_single(session, model, prompt)
            marker = "✓" if ttft < TTFT_TARGET_MS else "✗"
            print(
                f"  [{i+1}] TTFT={ttft:6.0f}ms  total={total:6.0f}ms  "
                f"{tok:2d} tok  {marker}  '{text[:40]}'"
            )

        # --- Cancellation smoke-test ---
        if not skip_cancel:
            print()
            print("Cancellation smoke-test:")
            await _test_cancel(session, model)

        # --- Benchmark runs ---
        print()
        print(
            f"{'#':<4} {'Prompt':<32} {'TTFT':>8} {'Total':>8} "
            f"{'Tok':>4} {'Tok/s':>7}"
        )
        print("-" * 72)

        ttfts: list[float] = []
        totals: list[float] = []
        for i in range(n_runs):
            prompt = _PROMPTS[i % len(_PROMPTS)]
            ttft, total, tok, text = await _run_single(session, model, prompt)
            ttfts.append(ttft)
            totals.append(total)
            tok_per_s = tok / (total / 1000) if total > 0 else 0
            marker = "✓" if ttft < TTFT_TARGET_MS else "✗"
            print(
                f"{i+1:<4} {prompt:<32} {ttft:>7.0f}ms {total:>7.0f}ms "
                f"{tok:>4} {tok_per_s:>6.1f}/s  {marker}"
            )

        # --- Summary ---
        print()
        print("Summary")
        print("-" * 40)
        for label, data in [("TTFT (ms)", ttfts), ("Total (ms)", totals)]:
            p50 = statistics.median(data)
            p90 = _percentile(data, 90)
            p95 = _percentile(data, 95)
            print(
                f"  {label:<12}  "
                f"min={min(data):6.0f}  p50={p50:6.0f}  "
                f"p90={p90:6.0f}  p95={p95:6.0f}  max={max(data):6.0f}"
            )

        passing = sum(1 for t in ttfts if t < TTFT_TARGET_MS)
        pct = 100 * passing // n_runs
        status = "PASS" if pct >= 90 else "FAIL"
        print()
        print(
            f"  Target <{TTFT_TARGET_MS}ms TTFT: {passing}/{n_runs} ({pct}%)  [{status}]"
        )
        if status == "FAIL" and model != "qwen2.5:0.5b":
            print(f"  Tip: try --model qwen2.5:0.5b for lowest latency")
        print()


def _parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="XType Ollama latency benchmark")
    p.add_argument("--model", default=DEFAULT_MODEL, help="Ollama model name")
    p.add_argument("--runs", type=int, default=20, help="Number of timed runs")
    p.add_argument("--warmup", type=int, default=2, help="Warmup runs (excluded)")
    p.add_argument(
        "--no-cancel-test", action="store_true", help="Skip cancellation smoke-test"
    )
    return p.parse_args()


if __name__ == "__main__":
    args = _parse_args()
    asyncio.run(main(args.model, args.warmup, args.runs, args.no_cancel_test))
