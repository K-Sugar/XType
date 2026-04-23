"""CotypistEngine — IBus engine for XType."""

import logging

import gi
gi.require_version("IBus", "1.0")
from gi.repository import GLib, IBus

from .config import AppConfig, load_config
from .context_buffer import ContextBuffer
from .debouncer import Debouncer
from .inference import InferenceClient, InferenceConfig

log = logging.getLogger(__name__)

_KEY_TAB = IBus.KEY_Tab
_KEY_ESC = IBus.KEY_Escape
_KEY_ENTER = IBus.KEY_Return
_KEY_KP_ENTER = IBus.KEY_KP_Enter
_KEY_BACKSPACE = IBus.KEY_BackSpace

_MODIFIER_MASK = (
    IBus.ModifierType.CONTROL_MASK
    | IBus.ModifierType.MOD1_MASK
    | IBus.ModifierType.SUPER_MASK
)


def _preedit_text(text: str) -> IBus.Text:
    n = len(text)
    t = IBus.Text.new_from_string(text)
    if n > 0:
        attrs = IBus.AttrList()
        attrs.append(IBus.Attribute.new(IBus.AttrType.UNDERLINE, IBus.AttrUnderline.SINGLE, 0, n))
        attrs.append(IBus.Attribute.new(IBus.AttrType.FOREGROUND, 0x888888, 0, n))
        t.set_attributes(attrs)
    return t


class CotypistEngine(IBus.Engine):
    """XType IBus engine — passes typed text through and shows AI suggestions as preedit."""

    def __init__(self, config: AppConfig | None = None) -> None:
        super().__init__()
        cfg = config or load_config()
        self._cfg = cfg

        self._ctx = ContextBuffer()
        self._debouncer = Debouncer(delay_ms=cfg.inference.debounce_ms, schedule=GLib.idle_add)
        self._inference = InferenceClient(InferenceConfig(
            model=cfg.inference.model,
            ollama_host=cfg.inference.ollama_host,
        ))
        self._inference.start()

        # Generation counter: bumped on every invalidation to silence stale callbacks.
        self._gen = 0
        # Client app identifier populated by do_focus_in_id for blocklist checks.
        self._app_id: str = ""

        if not self._inference.health_check():
            log.warning("Ollama unreachable or model '%s' not available", cfg.inference.model)

    # ------------------------------------------------------------------
    # IBus lifecycle
    # ------------------------------------------------------------------

    def do_focus_in_id(self, object_path: str, client: str) -> None:
        self._app_id = client or ""
        self._reset_state()

    def do_focus_in(self) -> None:
        # do_focus_in_id is preferred; this fires as a fallback on older IBus
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

        # Blocklisted app — pass everything through immediately
        if self._cfg.is_blocked(self._app_id):
            return False

        # Modifier combos (Ctrl/Alt/Super) — always pass through
        if state & _MODIFIER_MASK:
            return False

        has_suggestion = self._ctx.has_suggestion

        # Tab — accept next word; Shift+Tab — accept entire suggestion
        if keyval == _KEY_TAB:
            if has_suggestion:
                if state & IBus.ModifierType.SHIFT_MASK:
                    self.commit_text(IBus.Text.new_from_string(self._ctx.accept_all()))
                else:
                    self.commit_text(IBus.Text.new_from_string(self._ctx.accept_next_word()))
                self._update_preedit()
                return True
            return False

        # Escape — dismiss suggestion, clear preedit
        if keyval == _KEY_ESC:
            if has_suggestion:
                self._invalidate()
                self._update_preedit()
                return True
            return False

        # Enter — dismiss suggestion, pass key through to application
        if keyval in (_KEY_ENTER, _KEY_KP_ENTER):
            if has_suggestion:
                self._invalidate()
                self._update_preedit()
            return False

        # Backspace — dismiss only when suggestion active; remove char otherwise
        if keyval == _KEY_BACKSPACE:
            if has_suggestion:
                self._invalidate()
                self._update_preedit()
                return True
            self._invalidate()
            self._ctx.backspace()
            self._debouncer.cancel()
            return False

        # Printable character — pass through to app, append to buffer, debounce → inference
        ch = IBus.keyval_to_unicode(keyval)
        if ch and ch.isprintable():
            self._invalidate()
            self._ctx.append_char(ch)
            self._debouncer.trigger(self._request_inference)
            return False

        return False

    # ------------------------------------------------------------------
    # Inference
    # ------------------------------------------------------------------

    def _request_inference(self) -> None:
        """Kick off an inference request. Called on the GLib main thread by the debouncer."""
        context = self._ctx.context_text
        if len(context) < self._cfg.inference.min_context_chars:
            return
        # Truncate context to the configured window (send most-recent chars)
        if len(context) > self._cfg.inference.context_window:
            context = context[-self._cfg.inference.context_window:]
        self._gen += 1
        gen = self._gen
        self._inference.request(
            context=context,
            on_token=lambda tok: GLib.idle_add(self._handle_token, tok, gen),
            on_done=lambda: GLib.idle_add(self._handle_done, gen),
            on_error=self._on_error,
        )

    def _handle_token(self, token: str, gen: int) -> bool:
        """Append one streamed token to the live suggestion. Runs on GLib main thread."""
        if gen != self._gen:
            return False
        self._ctx.set_suggestion((self._ctx.suggestion or "") + token)
        self._update_preedit()
        return False

    def _handle_done(self, gen: int) -> bool:
        """Stream complete — strip trailing whitespace and finalise. Runs on GLib main thread."""
        if gen != self._gen:
            return False
        if self._ctx.has_suggestion:
            self._ctx.set_suggestion((self._ctx.suggestion or "").strip())
            self._update_preedit()
        return False

    def _on_error(self, exc: Exception) -> None:
        log.warning("inference error: %s", exc)

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _invalidate(self) -> None:
        """Bump the generation counter and dismiss any active suggestion."""
        self._gen += 1
        self._ctx.dismiss()

    def _update_preedit(self) -> None:
        if self._ctx.has_suggestion:
            text = self._ctx.suggestion or ""
            self.update_preedit_text(_preedit_text(text), len(text), True)
        else:
            self.update_preedit_text(IBus.Text.new_from_string(""), 0, False)

    def _reset_state(self) -> None:
        self._gen += 1
        self._debouncer.cancel()
        self._inference.cancel()
        self._ctx.reset()
        self.update_preedit_text(IBus.Text.new_from_string(""), 0, False)
