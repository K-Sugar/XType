"""Context buffer: sliding window of typed text + active suggestion state."""

from collections import deque

_MAX_CHARS = 500


class ContextBuffer:
    """Maintains the last 500 typed characters and tracks an active suggestion.

    Mutation rules
    --------------
    - append_char / backspace always dismiss any active suggestion first.
    - backspace with an active suggestion only dismisses — it does NOT remove
      a character from the buffer (matches the key-event spec).
    - accept_next_word / accept_all append their text into the buffer.
    """

    def __init__(self) -> None:
        self._buffer: deque[str] = deque(maxlen=_MAX_CHARS)
        self._suggestion: str | None = None

    # ------------------------------------------------------------------
    # Properties
    # ------------------------------------------------------------------

    @property
    def context_text(self) -> str:
        return "".join(self._buffer)

    @property
    def suggestion(self) -> str | None:
        return self._suggestion

    @property
    def has_suggestion(self) -> bool:
        return self._suggestion is not None

    # ------------------------------------------------------------------
    # Buffer mutations
    # ------------------------------------------------------------------

    def append_char(self, ch: str) -> None:
        """Append a typed character, dismissing any active suggestion."""
        self.dismiss()
        self._buffer.append(ch)

    def backspace(self) -> None:
        """Dismiss suggestion if active; otherwise remove the last character."""
        if self._suggestion is not None:
            self.dismiss()
        elif self._buffer:
            self._buffer.pop()

    # ------------------------------------------------------------------
    # Suggestion lifecycle
    # ------------------------------------------------------------------

    def set_suggestion(self, text: str) -> None:
        """Store a new suggestion, replacing any previous one."""
        self._suggestion = text if text else None

    def accept_next_word(self) -> str:
        """Consume and return the next word (+ trailing space) from the suggestion.

        The consumed text is appended to the buffer.  Returns "" when there is
        no active suggestion.  Clears the suggestion once all words are consumed.
        """
        if not self._suggestion:
            return ""
        idx = self._suggestion.find(" ")
        if idx == -1:
            word = self._suggestion
            self._suggestion = None
        else:
            word = self._suggestion[: idx + 1]
            remaining = self._suggestion[idx + 1 :]
            self._suggestion = remaining if remaining else None
        for ch in word:
            self._buffer.append(ch)
        return word

    def accept_all(self) -> str:
        """Consume and return the entire remaining suggestion.

        The text is appended to the buffer and the suggestion is cleared.
        Returns "" when there is no active suggestion.
        """
        if not self._suggestion:
            return ""
        text = self._suggestion
        self._suggestion = None
        for ch in text:
            self._buffer.append(ch)
        return text

    def dismiss(self) -> None:
        """Clear the active suggestion without modifying the buffer."""
        self._suggestion = None

    def reset(self) -> None:
        """Clear both the typed-text buffer and any active suggestion."""
        self._buffer.clear()
        self._suggestion = None
