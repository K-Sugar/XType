
```markdown
# Project Context: XType Linux — System-Wide AI Text Autocomplete for Linux

## Who I Am
- Name: Kevin Sugar (kevin.sugar@helsing.ai, Helsing)
- OS: CachyOS with KDE Plasma on Wayland
- Goal: Build a Linux clone of https://XType.app/ (macOS app by Daniel Gräfe / Accelerated Thought GmbH)
- Approach: Agentic coding (Claude Code / Cursor)

## What XType (macOS) Does
- System-wide inline text autocomplete powered by a local LLM (Qwen 2.5 1.5B)
- Uses macOS Accessibility API (AXUIElement) to monitor focused text fields across all apps
- Shows ghost text (gray inline suggestions) that you accept word-by-word with Tab
- Runs entirely on-device, no cloud. ~1-2GB RAM. Targets <200ms time-to-first-token
- Built by German indie dev Daniel Gräfe (also made Timing app)

## Why Linux Is Different
- macOS has one unified Accessibility API. Linux has fragmented input across X11/Wayland
- Wayland intentionally blocks global key interception for security
- xdotool doesn't work on Wayland. AT-SPI2 is inconsistent across toolkits on Wayland
- **Solution: Use the Input Method framework** — this is the Wayland-native way to intercept text input and show inline suggestions via preedit text

## Architecture Decision: IBus (Dev) → Fcitx5 (Prod)

### IBus (Development/Prototyping)
- Python-native API, fast iteration
- Works on KDE Wayland via IBus
- Write a custom `IBus.EngineBase` subclass
- Validated by Vocalinux project (voice dictation via IBus on KDE Wayland)

### Fcitx5 (Production)
- **Required for KDE Plasma Wayland** — KWin must launch the input method itself
- C++17 addon (shared library), loaded by Fcitx5
- Native Breeze theme integration on KDE
- Supports `setClientPreedit()` for inline ghost text within the app
- David Edmundson (KDE dev) validated this exact architecture in his input-method-playground project

### Key Insight
The Wayland `text-input` protocol is supported by almost all toolkits (GTK, Qt, Electron, SDL), making the input method approach universal. Preedit text = ghost text = inline suggestion.

## Tech Stack

Component	Technology
Language (IBus)	Python 3.12+
Language (Fcitx5)	C++17
LLM Backend	Ollama REST API (localhost:11434)
Model	Qwen 2.5 1.5B (GGUF Q4_K_M), 0.5B for dev
Input Method (Dev)	IBus custom engine
Input Method (Prod)	Fcitx5 custom addon
Text Injection	Input method commit_text (native)
Ghost Text	Preedit text with gray/underline attributes
Config	TOML (~/.config/XType-linux/config.toml)
Packaging	PKGBUILD (AUR), systemd user service
Build (Fcitx5)	CMake + Ninja

## Project Structure

```
XType-linux/
├── pyproject.toml
├── ibus-engine/                    # Phase A: IBus prototype
│   ├── XType.xml                # IBus component descriptor
│   ├── engine/
│   │   ├── main.py                 # Entry point (IBus.init, factory, main loop)
│   │   ├── engine.py               # XTypeEngine(IBus.Engine) — key handler
│   │   ├── inference.py            # Ollama async client (background thread)
│   │   ├── context_buffer.py       # Typed text state machine
│   │   ├── debouncer.py            # Timer-based debounce (180ms default)
│   │   └── config.py               # TOML config loader
│   └── tests/
├── fcitx5-engine/                  # Phase B: Fcitx5 production
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── XType.h/cpp          # InputMethodEngineV2 implementation
│   │   ├── inference_client.h/cpp  # libcurl HTTP client for Ollama
│   │   ├── context_buffer.h/cpp    # C++ port of context buffer
│   │   └── config.h
│   └── data/
│       ├── XType-addon.conf.in  # Fcitx5 addon descriptor
│       └── XType.conf           # IM registration
├── shared/prompt_templates/
├── packaging/
│   ├── PKGBUILD
│   ├── XType-linux.service      # systemd user service for Ollama
│   └── org.XType.linux.desktop
└── docs/
```

## Core Engine Logic (Both IBus and Fcitx5)

### Key Event Flow
1. **Printable char** → pass through to app, append to context buffer, dismiss old suggestion, debounce 180ms then trigger inference
2. **Tab** (when suggestion active) → accept next word, commit it, update preedit to show remaining
3. **Shift+Tab** (when suggestion active) → accept entire suggestion, commit all, clear preedit
4. **Escape** (when suggestion active) → dismiss suggestion, clear preedit
5. **Backspace** (when suggestion active) → dismiss suggestion; (no suggestion) → remove last char from buffer
6. **Enter** → dismiss suggestion, pass through
7. **Modifier combos** (Ctrl/Alt/Super+key) → always pass through
8. **Key releases** → always ignore
9. **Focus out** → MUST commit any active preedit text or it's lost
10. **Focus in / Reset** → clear preedit

### Context Buffer State Machine
- Maintains a deque of last 500 chars typed (sliding window)
- `set_suggestion(text)` stores LLM output, resets word index
- `accept_next_word()` returns next word + space, advances index, appends to buffer
- `accept_all()` returns full remaining suggestion
- `append_char()` / `backspace()` auto-dismiss any active suggestion
- `context_text` property returns the full buffer as string (LLM prompt context)

### Inference Pipeline
- Async Ollama client on background thread (Python: asyncio + threading; C++: std::thread + libcurl)
- Cancel-on-new-request: each new inference cancels any in-flight request
- Streaming: use stream=true, care about first token latency
- Marshal results back to main thread (GLib.idle_add for IBus, eventDispatcher.schedule for Fcitx5)
- Prompt: system prompt instructs "output ONLY completion text, no explanation, 5-15 words max, stop at sentence boundaries"
- Options: num_predict=30, temperature=0.3, top_p=0.9, stop=[".", "!", "?", "\n"]

### Preedit Rendering
- IBus: `update_preedit_text()` with `IBus.AttrType.UNDERLINE` + `IBus.AttrType.FOREGROUND` (0x888888 gray)
- Fcitx5: `ic->inputPanel().setClientPreedit(text)` with `TextFormatFlag::Underline`
- Client-side preedit renders inline within the application (not in a separate window)

## Config File (~/.config/XType-linux/config.toml)

```toml
[inference]
model = "qwen2.5:1.5b"
ollama_host = "http://localhost:11434"
debounce_ms = 180
min_context_chars = 10
context_window = 500

[behaviour]
tab_accepts_word = true
passthrough_terminals = true
blocklist_apps = ["keepassxc", "1password"]
```plaintext

## Critical Gotchas

1. **Fcitx5 on KDE Wayland**: Must be launched by KWin (Settings → Virtual Keyboard → Fcitx 5). Disable autostart to avoid conflicts.
2. **Focus-out must commit preedit** or text is silently lost.
3. **Chromium/Electron**: Default to text-input-v1, need `--wayland-text-input-version=3` flag for preedit.
4. **Thread safety**: Never call IBus/Fcitx5 APIs from inference thread — always marshal to main thread.
5. **Terminals** (Konsole, Alacritty): Preedit causes visual glitches — detect via window class and blocklist.
6. **Ollama health check**: `GET /api/tags` on startup, warn user if not running.
7. **Electron apps**: Need `ELECTRON_OZONE_PLATFORM_HINT=wayland` env var.

## App Compatibility Matrix

App	Toolkit	Wayland Protocol	Expected
Kate	Qt6	text-input-v2	✅ Full preedit
Firefox	GTK3/Gecko	text-input-v3	✅ via GTK_IM_MODULE=fcitx
Chromium	Blink	text-input-v1	⚠️ Needs flag
Konsole	Qt6	text-input-v2	🚫 Blocklisted
Electron	Chromium	text-input-v1	⚠️ Needs env var
LibreOffice	GTK3	text-input-v3	✅

## Development Timeline

Weeks	Phase
1-2	Phase 0: Environment, Ollama, benchmarks, scaffold
3-4	Phase A.1-A.4: IBus XML, ContextBuffer, Inference, Debouncer
5-6	Phase A.5-A.8: IBus Engine core, testing, polish
7-8	Phase B.1-B.3: Fcitx5 CMake, headers, inference client C++
9-10	Phase B.4-B.5: Fcitx5 engine C++ core (port from Python)
11	Phase B.6: KDE Plasma Wayland activation + toolkit testing
12	Full integration test matrix
13	PKGBUILD, systemd, AUR
14+	Settings UI, per-app profiles

## Agentic Coding Sessions (Sequential)

1. Ollama client + latency benchmark script
2. ContextBuffer + full unit tests
3. Debouncer + cancellation tests
4. IBus Engine skeleton + XML registration
5. Preedit + Tab/Escape/Backspace UX in engine
6. Focus/reset/commit edge cases
7. Config system + blocklist
8. Fcitx5 CMake + addon file scaffold
9. InferenceClient C++ (libcurl)
10. ContextBuffer C++ port
11. XTypeEngine C++ core
12. KDE Plasma Wayland integration testing
13. PKGBUILD + systemd + AUR packaging

## System Prerequisites (CachyOS/Arch)

```bash
sudo pacman -S ibus ibus-typing-booster fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk fcitx5-breeze
sudo pacman -S cmake ninja gcc clang pkgconf
sudo pacman -S ollama python uv
ollama pull qwen2.5:0.5b    # dev
ollama pull qwen2.5:1.5b    # prod
```plaintext

## Status: Ready to begin Session 1 (Ollama client + latency benchmark)

All code samples for both IBus (Python) and Fcitx5 (C++) engines have been drafted in the prior conversation. Ask me to produce any specific file and I can provide the full implementation.
```
