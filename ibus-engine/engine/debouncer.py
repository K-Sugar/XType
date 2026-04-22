"""Trailing-edge debouncer: fires callback after quiet period.

Production use — wire into engine.py like this:

    from gi.repository import GLib
    debouncer = Debouncer(schedule=GLib.idle_add)
    debouncer.trigger(lambda: self._on_debounce_fire())

The GLib.idle_add schedule ensures the callback runs on the GLib main thread
even though the timer fires on a background thread.
"""

import threading
from collections.abc import Callable

_DEFAULT_DELAY_MS = 180


class Debouncer:
    """Delays a callback until the caller has been quiet for `delay_ms` ms.

    Each call to trigger() cancels any pending invocation and starts a fresh
    timer.  A generation counter prevents a just-expired timer from firing
    after a concurrent trigger() or cancel() — eliminating the race window
    where threading.Timer.cancel() arrives too late.
    """

    def __init__(
        self,
        delay_ms: int = _DEFAULT_DELAY_MS,
        schedule: Callable[[Callable[[], None]], None] | None = None,
    ) -> None:
        """
        Args:
            delay_ms: Quiet period before callback fires (default 180 ms).
            schedule: Function used to marshal the callback to the target
                      thread.  Pass GLib.idle_add for IBus engine use.
                      Defaults to direct call (used in tests).
        """
        self._delay = delay_ms / 1000.0
        self._schedule = schedule
        self._timer: threading.Timer | None = None
        self._lock = threading.Lock()
        self._generation = 0

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def trigger(self, callback: Callable[[], None]) -> None:
        """Reset the timer; callback fires after delay if not re-triggered."""
        with self._lock:
            if self._timer is not None:
                self._timer.cancel()
            self._generation += 1
            gen = self._generation
            self._timer = threading.Timer(
                self._delay, self._fire, args=(callback, gen)
            )
            self._timer.daemon = True
            self._timer.start()

    def cancel(self) -> None:
        """Cancel any pending invocation; no-op if nothing is pending."""
        with self._lock:
            if self._timer is not None:
                self._timer.cancel()
                self._timer = None
            self._generation += 1  # invalidate any in-flight _fire call

    # ------------------------------------------------------------------
    # Internal
    # ------------------------------------------------------------------

    def _fire(self, callback: Callable[[], None], gen: int) -> None:
        with self._lock:
            if gen != self._generation:
                return  # superseded by a later trigger() or cancel()
            self._timer = None
        if self._schedule is not None:
            self._schedule(callback)
        else:
            callback()
