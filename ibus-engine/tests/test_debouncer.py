"""Tests use delay_ms=40 to keep the suite fast while still exercising real timing."""

import sys
import pathlib
import threading
import time

import pytest

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

from engine.debouncer import Debouncer

DELAY = 40        # ms — short enough for fast tests
BEFORE = 0.010    # seconds — well inside the quiet window
AFTER = 0.120     # seconds — well past the delay, safe margin


def direct_schedule(f: callable) -> None:
    """Schedule shim that calls f() immediately on the timer thread."""
    f()


# ---------------------------------------------------------------------------
# Basic fire
# ---------------------------------------------------------------------------


def test_fires_after_delay():
    fired = threading.Event()
    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(fired.set)
    assert not fired.is_set(), "fired before delay elapsed"
    assert fired.wait(timeout=AFTER), "did not fire within timeout"


def test_does_not_fire_before_delay():
    fired = threading.Event()
    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(fired.set)
    time.sleep(BEFORE)
    assert not fired.is_set()


# ---------------------------------------------------------------------------
# Rapid-fire: only the last call fires
# ---------------------------------------------------------------------------


def test_rapid_fire_fires_exactly_once():
    count = 0
    done = threading.Event()

    def cb():
        nonlocal count
        count += 1
        done.set()

    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    for _ in range(10):
        d.trigger(cb)
        time.sleep(BEFORE)  # 10ms between calls < 40ms delay

    assert done.wait(timeout=AFTER * 3)
    time.sleep(AFTER)  # wait extra to catch any spurious late fires
    assert count == 1


def test_rapid_fire_callback_is_last_registered():
    results: list[int] = []
    done = threading.Event()

    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(lambda: results.append(1))
    d.trigger(lambda: results.append(2))
    d.trigger(lambda: (results.append(3), done.set()))

    assert done.wait(timeout=AFTER * 3)
    time.sleep(AFTER)
    assert results == [3]


# ---------------------------------------------------------------------------
# Cancel
# ---------------------------------------------------------------------------


def test_cancel_prevents_fire():
    fired = threading.Event()
    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(fired.set)
    d.cancel()
    time.sleep(AFTER)
    assert not fired.is_set()


def test_cancel_before_trigger_is_noop():
    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.cancel()  # must not raise


def test_cancel_allows_subsequent_trigger():
    fired = threading.Event()
    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(lambda: None)
    d.cancel()
    d.trigger(fired.set)  # new trigger after cancel should work
    assert fired.wait(timeout=AFTER * 3)


def test_double_cancel_is_safe():
    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(lambda: None)
    d.cancel()
    d.cancel()  # second cancel must not raise


# ---------------------------------------------------------------------------
# Re-trigger resets the timer
# ---------------------------------------------------------------------------


def test_retrigger_resets_delay():
    fired_at: list[float] = []
    done = threading.Event()

    def cb():
        fired_at.append(time.perf_counter())
        done.set()

    d = Debouncer(delay_ms=DELAY, schedule=direct_schedule)
    d.trigger(cb)
    time.sleep(BEFORE)   # < delay — must not fire yet
    d.trigger(cb)        # reset
    time.sleep(BEFORE)   # < delay
    d.trigger(cb)        # reset again
    t_last = time.perf_counter()

    assert done.wait(timeout=AFTER * 3)
    # must fire >= DELAY ms after the last trigger
    elapsed_since_last_ms = (fired_at[0] - t_last) * 1000
    assert elapsed_since_last_ms >= DELAY - 5, (
        f"fired {elapsed_since_last_ms:.0f}ms after last trigger, expected >={DELAY}ms"
    )
    assert len(fired_at) == 1, "fired more than once"


# ---------------------------------------------------------------------------
# schedule parameter
# ---------------------------------------------------------------------------


def test_no_schedule_calls_directly():
    """When schedule=None, callback runs directly on the timer thread."""
    fired = threading.Event()
    d = Debouncer(delay_ms=DELAY)  # no schedule
    d.trigger(fired.set)
    assert fired.wait(timeout=AFTER * 3)


def test_custom_schedule_is_invoked():
    scheduled_calls: list[callable] = []
    fired = threading.Event()

    def my_schedule(f):
        scheduled_calls.append(f)
        f()
        fired.set()

    d = Debouncer(delay_ms=DELAY, schedule=my_schedule)
    d.trigger(lambda: None)

    assert fired.wait(timeout=AFTER * 3)
    assert len(scheduled_calls) == 1


# ---------------------------------------------------------------------------
# Default delay
# ---------------------------------------------------------------------------


def test_default_delay_is_180ms():
    d = Debouncer()
    assert d._delay == pytest.approx(0.180)
