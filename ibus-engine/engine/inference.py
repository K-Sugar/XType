"""Async Ollama streaming client running on a dedicated background thread.

Callbacks fire on the background thread — callers must marshal to the main
GLib thread via GLib.idle_add before touching any IBus/UI state.
"""

import asyncio
import json
import logging
import threading
from collections.abc import Callable
from dataclasses import dataclass, field

import aiohttp

logger = logging.getLogger(__name__)

_SYSTEM_PROMPT = (
    "Output ONLY the completion text, no explanation. "
    "5-15 words max. Stop at sentence boundaries."
)


@dataclass
class InferenceConfig:
    model: str = "qwen2.5:0.5b"
    ollama_host: str = "http://localhost:11434"
    num_predict: int = 30
    temperature: float = 0.3
    top_p: float = 0.9
    stop_tokens: list[str] = field(default_factory=lambda: [".", "!", "?", "\n"])


class InferenceClient:
    """Thread-safe Ollama client.

    Call start() once, then request() from any thread.  A new request
    immediately cancels any in-flight one.  Call stop() to shut down.
    """

    def __init__(self, config: InferenceConfig | None = None) -> None:
        self._cfg = config or InferenceConfig()
        self._loop: asyncio.AbstractEventLoop | None = None
        self._thread: threading.Thread | None = None
        self._current_task: asyncio.Task | None = None
        self._session: aiohttp.ClientSession | None = None
        self._ready = threading.Event()

    # ------------------------------------------------------------------
    # Public API (main-thread safe)
    # ------------------------------------------------------------------

    def start(self) -> None:
        """Start the background event loop thread."""
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(
            target=self._run_loop, daemon=True, name="inference"
        )
        self._thread.start()
        self._ready.wait(timeout=5.0)

    def stop(self) -> None:
        """Gracefully shut down the background thread."""
        if self._loop and self._loop.is_running():
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread:
            self._thread.join(timeout=3.0)

    def request(
        self,
        context: str,
        on_token: Callable[[str], None],
        on_done: Callable[[], None],
        on_error: Callable[[Exception], None],
    ) -> None:
        """Schedule an inference, cancelling any in-flight request first.

        Args:
            context:  The text the user has typed so far (LLM prompt).
            on_token: Called for each streamed token as it arrives.
            on_done:  Called once when the stream completes normally.
            on_error: Called if the request fails (not called on cancel).
        """
        if self._loop is None:
            raise RuntimeError("call start() first")
        self._loop.call_soon_threadsafe(
            self._schedule, context, on_token, on_done, on_error
        )

    def cancel(self) -> None:
        """Cancel any in-flight inference request without starting a new one."""
        if self._loop is None:
            return
        self._loop.call_soon_threadsafe(self._cancel_current)

    def health_check(self) -> bool:
        """Blocking check: is Ollama reachable and is the model available?"""
        import urllib.request

        try:
            with urllib.request.urlopen(
                f"{self._cfg.ollama_host}/api/tags", timeout=2
            ) as resp:
                data = json.loads(resp.read())
                names = [m["name"] for m in data.get("models", [])]
                if self._cfg.model not in names:
                    logger.warning(
                        "Model %s not found. Available: %s", self._cfg.model, names
                    )
                    return False
                return True
        except Exception:
            logger.warning("Ollama not reachable at %s", self._cfg.ollama_host)
            return False

    # ------------------------------------------------------------------
    # Background thread internals
    # ------------------------------------------------------------------

    def _run_loop(self) -> None:
        asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self._init_session())
        self._ready.set()
        self._loop.run_forever()
        self._loop.run_until_complete(self._close_session())

    async def _init_session(self) -> None:
        self._session = aiohttp.ClientSession(
            timeout=aiohttp.ClientTimeout(connect=2.0, total=30.0)
        )

    async def _close_session(self) -> None:
        if self._session:
            await self._session.close()

    def _cancel_current(self) -> None:
        if self._current_task and not self._current_task.done():
            self._current_task.cancel()
        self._current_task = None

    def _schedule(
        self,
        context: str,
        on_token: Callable[[str], None],
        on_done: Callable[[], None],
        on_error: Callable[[Exception], None],
    ) -> None:
        if self._current_task and not self._current_task.done():
            self._current_task.cancel()
        self._current_task = self._loop.create_task(
            self._infer(context, on_token, on_done, on_error)
        )

    async def _infer(
        self,
        context: str,
        on_token: Callable[[str], None],
        on_done: Callable[[], None],
        on_error: Callable[[Exception], None],
    ) -> None:
        payload = {
            "model": self._cfg.model,
            "prompt": context,
            "system": _SYSTEM_PROMPT,
            "stream": True,
            "options": {
                "num_predict": self._cfg.num_predict,
                "temperature": self._cfg.temperature,
                "top_p": self._cfg.top_p,
                "stop": self._cfg.stop_tokens,
            },
        }
        try:
            url = f"{self._cfg.ollama_host}/api/generate"
            async with self._session.post(url, json=payload) as resp:
                resp.raise_for_status()
                async for raw in resp.content:
                    raw = raw.strip()
                    if not raw:
                        continue
                    data = json.loads(raw)
                    token = data.get("response", "")
                    if token:
                        on_token(token)
                    if data.get("done"):
                        break
            on_done()
        except asyncio.CancelledError:
            pass  # normal path when a newer request arrives
        except Exception as exc:
            logger.exception("Inference failed")
            on_error(exc)
