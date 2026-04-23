"""Tests for engine/config.py — TOML loader and blocklist logic."""

import sys
import pathlib
import tomllib

import pytest

sys.path.insert(0, str(pathlib.Path(__file__).parent.parent))

from engine.config import (
    AppConfig,
    BehaviourSection,
    InferenceSection,
    _DEFAULT_TOML,
    _parse,
    _write_default,
    load_config,
)


# ---------------------------------------------------------------------------
# Default values
# ---------------------------------------------------------------------------


def test_inference_defaults():
    s = InferenceSection()
    assert s.model == "qwen2.5:0.5b"
    assert s.ollama_host == "http://localhost:11434"
    assert s.debounce_ms == 180
    assert s.min_context_chars == 3
    assert s.context_window == 500


def test_behaviour_defaults():
    s = BehaviourSection()
    assert s.tab_accepts_word is True
    assert s.passthrough_terminals is True
    assert isinstance(s.blocklist_apps, list)


def test_appconfig_defaults():
    cfg = AppConfig()
    assert isinstance(cfg.inference, InferenceSection)
    assert isinstance(cfg.behaviour, BehaviourSection)


# ---------------------------------------------------------------------------
# TOML parsing via _parse
# ---------------------------------------------------------------------------


def _toml(text: str) -> dict:
    return tomllib.loads(text)


def test_parse_inference_section():
    cfg = _parse(_toml("""
[inference]
model = "llama3.2:1b"
ollama_host = "http://10.0.0.1:11434"
debounce_ms = 250
min_context_chars = 5
context_window = 300
"""))
    assert cfg.inference.model == "llama3.2:1b"
    assert cfg.inference.ollama_host == "http://10.0.0.1:11434"
    assert cfg.inference.debounce_ms == 250
    assert cfg.inference.min_context_chars == 5
    assert cfg.inference.context_window == 300


def test_parse_behaviour_section():
    cfg = _parse(_toml("""
[behaviour]
tab_accepts_word = false
passthrough_terminals = false
blocklist_apps = ["vim", "emacs"]
"""))
    assert cfg.behaviour.tab_accepts_word is False
    assert cfg.behaviour.passthrough_terminals is False
    assert cfg.behaviour.blocklist_apps == ["vim", "emacs"]


def test_parse_missing_inference_uses_defaults():
    cfg = _parse(_toml("[behaviour]\ntab_accepts_word = false\n"))
    assert cfg.inference.model == "qwen2.5:0.5b"
    assert cfg.behaviour.tab_accepts_word is False


def test_parse_missing_behaviour_uses_defaults():
    cfg = _parse(_toml("[inference]\nmodel = \"phi3:mini\"\n"))
    assert cfg.inference.model == "phi3:mini"
    assert cfg.behaviour.tab_accepts_word is True


def test_parse_partial_inference_fills_missing_with_defaults():
    cfg = _parse(_toml("[inference]\nmodel = \"mistral:7b\"\n"))
    assert cfg.inference.model == "mistral:7b"
    assert cfg.inference.debounce_ms == 180  # default preserved
    assert cfg.inference.context_window == 500


def test_parse_empty_dict_is_pure_defaults():
    cfg = _parse({})
    assert cfg.inference.model == "qwen2.5:0.5b"
    assert cfg.behaviour.passthrough_terminals is True


# ---------------------------------------------------------------------------
# is_blocked
# ---------------------------------------------------------------------------


def test_is_blocked_app_in_blocklist():
    cfg = AppConfig()
    cfg.behaviour.blocklist_apps = ["vim", "emacs"]
    assert cfg.is_blocked("vim") is True


def test_is_blocked_app_not_in_blocklist():
    cfg = AppConfig()
    cfg.behaviour.blocklist_apps = ["vim"]
    assert cfg.is_blocked("kate") is False


def test_is_blocked_empty_string_returns_false():
    cfg = AppConfig()
    assert cfg.is_blocked("") is False


def test_is_blocked_terminal_with_passthrough_true():
    cfg = AppConfig()
    cfg.behaviour.passthrough_terminals = True
    assert cfg.is_blocked("org.kde.konsole") is True


def test_is_blocked_terminal_with_passthrough_false():
    cfg = AppConfig()
    cfg.behaviour.passthrough_terminals = False
    cfg.behaviour.blocklist_apps = []
    assert cfg.is_blocked("org.kde.konsole") is False


def test_is_blocked_custom_app_ignores_terminal_flag():
    cfg = AppConfig()
    cfg.behaviour.passthrough_terminals = True
    cfg.behaviour.blocklist_apps = ["myapp"]
    assert cfg.is_blocked("myapp") is True
    assert cfg.is_blocked("otherapp") is False


# ---------------------------------------------------------------------------
# write_default / load_config
# ---------------------------------------------------------------------------


def test_write_default_creates_valid_toml(tmp_path):
    dest = tmp_path / "xtype" / "config.toml"
    _write_default(dest)
    assert dest.exists()
    with open(dest, "rb") as fh:
        data = tomllib.load(fh)
    assert "inference" in data
    assert "behaviour" in data


def test_write_default_round_trips_to_defaults(tmp_path):
    dest = tmp_path / "config.toml"
    _write_default(dest)
    cfg = load_config(dest)
    assert cfg.inference.model == "qwen2.5:0.5b"
    assert cfg.inference.debounce_ms == 180
    assert cfg.behaviour.passthrough_terminals is True


def test_load_config_missing_file_writes_default_and_returns_defaults(tmp_path):
    p = tmp_path / "sub" / "config.toml"
    cfg = load_config(p)
    assert p.exists(), "default config should have been written"
    assert cfg.inference.model == "qwen2.5:0.5b"


def test_load_config_reads_custom_values(tmp_path):
    p = tmp_path / "config.toml"
    p.write_text('[inference]\nmodel = "phi3:mini"\ndebounce_ms = 300\n', encoding="utf-8")
    cfg = load_config(p)
    assert cfg.inference.model == "phi3:mini"
    assert cfg.inference.debounce_ms == 300
    assert cfg.inference.min_context_chars == 3  # default


def test_default_toml_is_valid_toml():
    data = tomllib.loads(_DEFAULT_TOML)
    assert data["inference"]["model"] == "qwen2.5:0.5b"
    assert data["behaviour"]["passthrough_terminals"] is True
