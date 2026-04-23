import pytest
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

from engine.context_buffer import ContextBuffer


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def buf(*chars: str) -> ContextBuffer:
    cb = ContextBuffer()
    for ch in chars:
        cb.append_char(ch)
    return cb


# ---------------------------------------------------------------------------
# context_text
# ---------------------------------------------------------------------------


def test_context_text_empty():
    assert ContextBuffer().context_text == ""


def test_context_text_after_typing():
    cb = buf("h", "e", "l", "l", "o")
    assert cb.context_text == "hello"


def test_context_text_after_accept_next_word():
    cb = buf("t", "y", "p", "e", " ")
    cb.set_suggestion("fast")
    cb.accept_next_word()
    assert cb.context_text == "type fast"


def test_context_text_after_accept_all():
    cb = buf("t", "y", "p", "e", " ")
    cb.set_suggestion("fast now")
    cb.accept_all()
    assert cb.context_text == "type fast now"


# ---------------------------------------------------------------------------
# append_char
# ---------------------------------------------------------------------------


def test_append_char_grows_buffer():
    cb = ContextBuffer()
    cb.append_char("a")
    cb.append_char("b")
    assert cb.context_text == "ab"


def test_append_char_dismisses_active_suggestion():
    cb = ContextBuffer()
    cb.set_suggestion("hello world")
    cb.append_char("x")
    assert not cb.has_suggestion
    assert cb.context_text == "x"


# ---------------------------------------------------------------------------
# backspace
# ---------------------------------------------------------------------------


def test_backspace_removes_last_char():
    cb = buf("a", "b", "c")
    cb.backspace()
    assert cb.context_text == "ab"


def test_backspace_on_empty_buffer_no_crash():
    cb = ContextBuffer()
    cb.backspace()  # must not raise
    assert cb.context_text == ""


def test_backspace_with_suggestion_dismisses_only():
    cb = buf("h", "i")
    cb.set_suggestion("there")
    cb.backspace()
    assert not cb.has_suggestion
    assert cb.context_text == "hi"  # char NOT removed


def test_backspace_without_suggestion_removes_char():
    cb = buf("h", "i")
    cb.backspace()
    assert cb.context_text == "h"


# ---------------------------------------------------------------------------
# set_suggestion / dismiss / has_suggestion
# ---------------------------------------------------------------------------


def test_set_suggestion_stores_text():
    cb = ContextBuffer()
    cb.set_suggestion("hello world")
    assert cb.suggestion == "hello world"
    assert cb.has_suggestion


def test_set_suggestion_empty_string_clears():
    cb = ContextBuffer()
    cb.set_suggestion("something")
    cb.set_suggestion("")
    assert not cb.has_suggestion
    assert cb.suggestion is None


def test_set_suggestion_replaces_previous():
    cb = ContextBuffer()
    cb.set_suggestion("first")
    cb.set_suggestion("second")
    assert cb.suggestion == "second"


def test_dismiss_clears_suggestion():
    cb = ContextBuffer()
    cb.set_suggestion("hello")
    cb.dismiss()
    assert not cb.has_suggestion


def test_dismiss_on_no_suggestion_no_crash():
    cb = ContextBuffer()
    cb.dismiss()  # must not raise
    assert not cb.has_suggestion


def test_dismiss_does_not_modify_buffer():
    cb = buf("a", "b")
    cb.set_suggestion("cd")
    cb.dismiss()
    assert cb.context_text == "ab"


# ---------------------------------------------------------------------------
# accept_next_word
# ---------------------------------------------------------------------------


def test_accept_next_word_single_word_no_space():
    cb = ContextBuffer()
    cb.set_suggestion("hello")
    word = cb.accept_next_word()
    assert word == "hello"
    assert not cb.has_suggestion


def test_accept_next_word_returns_word_with_trailing_space():
    cb = ContextBuffer()
    cb.set_suggestion("quick brown fox")
    word = cb.accept_next_word()
    assert word == "quick "


def test_accept_next_word_advances_through_suggestion():
    cb = ContextBuffer()
    cb.set_suggestion("one two three")
    assert cb.accept_next_word() == "one "
    assert cb.accept_next_word() == "two "
    assert cb.accept_next_word() == "three"
    assert not cb.has_suggestion


def test_accept_next_word_appends_to_buffer():
    cb = buf("s", "t", "a", "r", "t", " ")
    cb.set_suggestion("here now")
    cb.accept_next_word()
    assert cb.context_text == "start here "


def test_accept_next_word_exhausts_suggestion_on_last_word():
    cb = ContextBuffer()
    cb.set_suggestion("last")
    cb.accept_next_word()
    assert cb.suggestion is None
    assert not cb.has_suggestion


def test_accept_next_word_no_suggestion_returns_empty():
    cb = ContextBuffer()
    assert cb.accept_next_word() == ""


def test_accept_next_word_full_sequence_appended_to_buffer():
    cb = ContextBuffer()
    cb.set_suggestion("the lazy dog")
    cb.accept_next_word()
    cb.accept_next_word()
    cb.accept_next_word()
    assert cb.context_text == "the lazy dog"


# ---------------------------------------------------------------------------
# accept_all
# ---------------------------------------------------------------------------


def test_accept_all_returns_full_remaining_suggestion():
    cb = ContextBuffer()
    cb.set_suggestion("quick brown fox")
    text = cb.accept_all()
    assert text == "quick brown fox"


def test_accept_all_clears_suggestion():
    cb = ContextBuffer()
    cb.set_suggestion("something")
    cb.accept_all()
    assert not cb.has_suggestion


def test_accept_all_appends_to_buffer():
    cb = buf("a", " ")
    cb.set_suggestion("big idea")
    cb.accept_all()
    assert cb.context_text == "a big idea"


def test_accept_all_no_suggestion_returns_empty():
    cb = ContextBuffer()
    assert cb.accept_all() == ""


def test_accept_all_after_partial_accept():
    cb = ContextBuffer()
    cb.set_suggestion("one two three")
    cb.accept_next_word()           # consumes "one "
    rest = cb.accept_all()
    assert rest == "two three"
    assert cb.context_text == "one two three"
    assert not cb.has_suggestion


# ---------------------------------------------------------------------------
# Buffer overflow (maxlen=500)
# ---------------------------------------------------------------------------


def test_buffer_respects_maxlen():
    cb = ContextBuffer()
    for _ in range(600):
        cb.append_char("x")
    assert len(cb.context_text) == 500


def test_buffer_drops_oldest_chars():
    cb = ContextBuffer()
    for i in range(500):
        cb.append_char("a")
    cb.append_char("z")
    assert cb.context_text[-1] == "z"
    assert len(cb.context_text) == 500
    assert cb.context_text[0] == "a"


def test_accept_all_respects_maxlen():
    cb = ContextBuffer()
    for _ in range(490):
        cb.append_char("a")
    cb.set_suggestion("x" * 20)
    cb.accept_all()
    assert len(cb.context_text) == 500


# ---------------------------------------------------------------------------
# reset
# ---------------------------------------------------------------------------


def test_reset_clears_buffer():
    cb = buf("a", "b", "c")
    cb.reset()
    assert cb.context_text == ""


def test_reset_clears_suggestion():
    cb = ContextBuffer()
    cb.set_suggestion("hello world")
    cb.reset()
    assert not cb.has_suggestion
    assert cb.suggestion is None


def test_reset_clears_both():
    cb = buf("t", "y", "p", "e", " ")
    cb.set_suggestion("this")
    cb.reset()
    assert cb.context_text == ""
    assert not cb.has_suggestion


def test_reset_on_empty_buffer_no_crash():
    cb = ContextBuffer()
    cb.reset()  # must not raise
    assert cb.context_text == ""
    assert not cb.has_suggestion


def test_reset_allows_fresh_use():
    cb = buf("h", "e", "l", "l", "o")
    cb.set_suggestion("world")
    cb.reset()
    cb.append_char("x")
    assert cb.context_text == "x"
    assert not cb.has_suggestion
