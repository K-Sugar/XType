"""CotypistEngine — IBus engine skeleton for XType."""

import logging

import gi
gi.require_version("IBus", "1.0")
from gi.repository import GLib, IBus

from .context_buffer import ContextBuffer
from .debouncer import Debouncer
from .inference import InferenceClient, InferenceConfig

log = logging.getLogger(__name__)

# IBus key symbols (subset used here)
_KEY_TAB = IBus.KEY_Tab
_KEY_ESC = IBus.KEY_Escape
_KEY_ENTER = IBus.KEY_Return
_KEY_KP_ENTER = IBus.KEY_KP_Enter
_KEY_BACKSPACE = IBus.KEY_BackSpace

_PREEDIT_ATTRS = IBus.AttrList()
_PREEDIT_ATTRS.append(IBus.Attribute.new(IBus.AttrType.UNDERLINE, IBus.AttrUnderline.SINGLE, 0, 0))
_PREEDIT_ATTRS.append(IBus.Attribute.new(IBus.AttrType.FOREGROUND, 0x888888, 0, 0))


def _make_preedit_attrs(length: int) -> IBus.AttrList:
    attrs = IBus.AttrList()
    if length > 0:
        attrs.append(IBus.Attribute.new(IBus.AttrType.UNDERLINE, IBus.AttrUnderline.SINGLE, 0, length))
        attrs.append(IBus.Attribute.new(IBus.AttrType.FOREGROUND, 0x888888, 0, length))
    return attrs


class CotypistEngine(IBus.Engine):
    """XType IBus engine — passes typed text through and shows AI suggestions as preedit."""

    def __init__(self, config: InferenceConfig | None = None) -> None:
        super().__init__()
        self._ctx = ContextBuffer()
        self._debouncer = Debouncer(schedule=GLib.idle_add)
        self._inference = InferenceClient(config or InferenceConfig())
        self._inference.start()

    # ------------------------------------------------------------------
    # IBus lifecycle
    # ------------------------------------------------------------------

    def do_focus_in(self) -> None:
        self._reset_state()

    def do_focus_out(self) -> None:
        # MUST commit any active preedit before clearing — text is lost otherwise
        if self._ctx.has_suggestion:
            self.commit_text(IBus.Text.new_from_string(self._ctx.suggestion or ""))
        self._reset_state()

    def do_reset(self) -> None:
        self._reset_state()

    # ------------------------------------------------------------------
    # Key event handler
    # ------------------------------------------------------------------

    def do_process_key_event(self, keyval: int, keycode: int, state: int) -> bool:
        # Ignore key releases
        if state & IBus.ModifierType.RELEASE_MASK:
            return False

        # Modifier combos (Ctrl/Alt/Super) — pass through
        mask = (
            IBus.ModifierType.CONTROL_MASK
            | IBus.ModifierType.MOD1_MASK
            | IBus.ModifierType.SUPER_MASK
        )
        if state & mask:
            return False

        has_suggestion = self._ctx.has_suggestion

        # Tab — accept next word (Shift+Tab accepts all)
        if keyval == _KEY_TAB:
            if has_suggestion:
                if state & IBus.ModifierType.SHIFT_MASK:
                    committed = self._ctx.accept_all()
                    self.commit_text(IBus.Text.new_from_string(committed))
                else:
                    committed = self._ctx.accept_next_word()
                    self.commit_text(IBus.Text.new_from_string(committed))
                self._update_preedit()
                return True
            return False

        # Escape — dismiss suggestion
        if keyval == _KEY_ESC:
            if has_suggestion:
                self._ctx.dismiss()
                self._update_preedit()
                return True
            return False

        # Enter / KP_Enter — dismiss and pass through
        if keyval in (_KEY_ENTER, _KEY_KP_ENTER):
            if has_suggestion:
                self._ctx.dismiss()
                self._update_preedit()
            return False

        # Backspace — dismiss only when suggestion active; remove char otherwise
        if keyval == _KEY_BACKSPACE:
            if has_suggestion:
                self._ctx.backspace()  # dismiss only per spec
                self._update_preedit()
                return True
            self._ctx.backspace()
            self._debouncer.cancel()
            return False

        # Printable character — pass through, append to buffer, debounce → inference
        ch = IBus.keyval_to_unicode(keyval)
        if ch and ch.isprintable():
            self._ctx.append_char(ch)
            self._debouncer.trigger(self._request_inference)
            return False

        return False

    # ------------------------------------------------------------------
    # Inference
    # ------------------------------------------------------------------

    def _request_inference(self) -> None:
        """Called on GLib main thread by the debouncer."""
        context = self._ctx.context_text
        if not context.strip():
            return
        self._inference.request(
            context=context,
            on_token=self._on_token,
            on_done=self._on_done,
            on_error=self._on_error,
        )

    def _on_token(self, token: str) -> None:
        GLib.idle_add(self._handle_token, token)

    def _on_done(self, full_text: str) -> None:
        GLib.idle_add(self._handle_done, full_text)

    def _on_error(self, exc: Exception) -> None:
        log.warning("inference error: %s", exc)

    def _handle_token(self, token: str) -> bool:
        # Accumulate tokens — the done callback delivers the complete suggestion
        return False

    def _handle_done(self, full_text: str) -> bool:
        if full_text.strip():
            self._ctx.set_suggestion(full_text)
            self._update_preedit()
        return False

    # ------------------------------------------------------------------
    # Preedit helpers
    # ------------------------------------------------------------------

    def _update_preedit(self) -> None:
        if self._ctx.has_suggestion:
            text = self._ctx.suggestion or ""
            ibus_text = IBus.Text.new_from_string(text)
            ibus_text.set_attributes(_make_preedit_attrs(len(text)))
            self.update_preedit_text(ibus_text, len(text), True)
        else:
            self.update_preedit_text(IBus.Text.new_from_string(""), 0, False)

    def _reset_state(self) -> None:
        self._debouncer.cancel()
        self._ctx.dismiss()
        self.update_preedit_text(IBus.Text.new_from_string(""), 0, False)
