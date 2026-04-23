"""XType configuration — TOML loader with per-field defaults."""

import logging
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

log = logging.getLogger(__name__)

CONFIG_PATH = Path.home() / ".config" / "xtype" / "config.toml"

# Terminal window classes that are auto-blocked when passthrough_terminals=true
_TERMINAL_CLASSES: frozenset[str] = frozenset([
    "org.kde.konsole",
    "Alacritty", "alacritty",
    "kitty",
    "com.mitchellh.ghostty",
    "xterm", "urxvt", "foot", "wezterm",
    "tilix", "gnome-terminal-server",
])

_DEFAULT_TOML = """\
[inference]
model = "qwen2.5:0.5b"
ollama_host = "http://localhost:11434"
debounce_ms = 180
min_context_chars = 3
context_window = 500

[behaviour]
tab_accepts_word = true
passthrough_terminals = true
blocklist_apps = ["org.kde.konsole", "Alacritty", "kitty"]
"""


@dataclass
class InferenceSection:
    model: str = "qwen2.5:0.5b"
    ollama_host: str = "http://localhost:11434"
    debounce_ms: int = 180
    min_context_chars: int = 3
    context_window: int = 500


@dataclass
class BehaviourSection:
    tab_accepts_word: bool = True
    passthrough_terminals: bool = True
    blocklist_apps: list[str] = field(default_factory=lambda: [
        "org.kde.konsole", "Alacritty", "kitty",
    ])


@dataclass
class AppConfig:
    inference: InferenceSection = field(default_factory=InferenceSection)
    behaviour: BehaviourSection = field(default_factory=BehaviourSection)

    def is_blocked(self, app_id: str) -> bool:
        """Return True if the engine should pass all keys through for this app."""
        if not app_id:
            return False
        if self.behaviour.passthrough_terminals and app_id in _TERMINAL_CLASSES:
            return True
        return app_id in self.behaviour.blocklist_apps


def load_config(path: Path | None = None) -> AppConfig:
    """Load config from *path* (default CONFIG_PATH), writing defaults if absent."""
    p = path or CONFIG_PATH
    if not p.exists():
        _write_default(p)
        return AppConfig()
    try:
        with open(p, "rb") as fh:
            data = tomllib.load(fh)
    except Exception as exc:
        log.warning("Failed to read config %s: %s — using defaults", p, exc)
        return AppConfig()
    return _parse(data)


def _parse(data: dict) -> AppConfig:
    cfg = AppConfig()
    if inf := data.get("inference"):
        cfg.inference = InferenceSection(
            model=inf.get("model", cfg.inference.model),
            ollama_host=inf.get("ollama_host", cfg.inference.ollama_host),
            debounce_ms=int(inf.get("debounce_ms", cfg.inference.debounce_ms)),
            min_context_chars=int(inf.get("min_context_chars", cfg.inference.min_context_chars)),
            context_window=int(inf.get("context_window", cfg.inference.context_window)),
        )
    if beh := data.get("behaviour"):
        cfg.behaviour = BehaviourSection(
            tab_accepts_word=bool(beh.get("tab_accepts_word", cfg.behaviour.tab_accepts_word)),
            passthrough_terminals=bool(
                beh.get("passthrough_terminals", cfg.behaviour.passthrough_terminals)
            ),
            blocklist_apps=list(beh.get("blocklist_apps", cfg.behaviour.blocklist_apps)),
        )
    return cfg


def _write_default(path: Path) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(_DEFAULT_TOML, encoding="utf-8")
        log.info("Wrote default config to %s", path)
    except Exception as exc:
        log.warning("Could not write default config to %s: %s", path, exc)
