<div align="center">

# XType

**System-wide locally running AI text autocomplete for Linux.**

Inline ghost-text completions powered by a local LLM, delivered through the
Wayland-native input method framework. No cloud, no telemetry, just a clean and simple
Fcitx5 engine that works in every text field on your desktop.

[![Status](https://img.shields.io/badge/status-phase%20B%20%E2%80%94%20fcitx5%20production-blue)]()
[![Platform](https://img.shields.io/badge/platform-Linux%20%E2%80%A2%20Wayland%20%E2%80%A2%20KDE-1793D1)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)]()
[![Python](https://img.shields.io/badge/Python-3.12%2B-3776AB?logo=python&logoColor=white)]()
[![Model](https://img.shields.io/badge/model-Qwen2.5%201.5B-7C3AED)]()

</div>

---

## Table of Contents

1. [Summary](#1-summary)
2. [Introduction](#2-introduction)
3. [Features](#3-features)
4. [Installation](#4-installation)
5. [Configuration](#5-configuration)
6. [Dependencies](#6-dependencies)
7. [Systems & Technical Details](#7-systems--technical-details)
8. [Project Structure](#8-project-structure)
9. [Application Compatibility](#9-application-compatibility)
10. [Roadmap](#10-roadmap)
11. [Acknowledgements](#11-acknowledgements)
12. [License](#12-license)

---

## 1. Summary

**XType** brings Cotypist-style inline AI autocomplete to Linux. This project is
vibe coded by one developer, and basically a personal project. Dont expect perfect
support or production quality :D** 

As you type, a small local language model continues your sentence in greyed-out
ghost text directly inside whatever app you're using — your editor, your browser,
your office suite. Press `Tab` to accept the next word, `Shift+Tab` to accept the
whole suggestion, `Esc` to dismiss it. Everything happens on-device, on your CPU
or GPU, with the model held in roughly **1–2 GB of RAM** and a target
**time-to-first-token under 200 ms**.

It is implemented as a native input method engine — first as a Python IBus
prototype, then re-engineered in C++ as a production Fcitx5 addon — so it works
across GTK, Qt, Electron and SDL applications without ever touching the
keyboard, the X server, or the network.

---

## 2. Introduction

On macOS, [Cotypist](https://cotypist.app/) by **Daniel Gräfe** (Accelerated
Thought GmbH) demonstrated how delightful system-wide AI autocomplete can feel
when it's tightly integrated with the OS. 

Input on Linux is fragmented across X11 and Wayland, multiple toolkits (GTK,
Qt, Electron, SDL), and shifting protocols. Wayland deliberately blocks global
key interception as a security boundary; `xdotool` does not work; AT-SPI2 is
inconsistent across toolkits.

The clean answer is the one Wayland itself gives you: **become the input
method**. Every modern toolkit on Wayland speaks the `text-input` protocol, and
input methods are first-class citizens with full rights to render *preedit
text* (ghost text) inline in the focused field. That is the opening XType walks
through.

The project is built in two phases:

| Phase | Stack         | Purpose                                                |
|------:|---------------|--------------------------------------------------------|
| **A** | Python + IBus | Rapid prototyping of the engine logic and UX           |
| **B** | C++20 + Fcitx5 | Production-quality addon, native Breeze integration   |

The two engines share one design and one set of behaviours; the C++ port is a
near 1:1 translation of the Python reference.

---

## 3. Features

- **Inline ghost text** rendered directly in the application's own text field —
  no popup window, no overlay, no z-order race with your tiling WM.
- **Local-only inference** via [Ollama](https://ollama.com), defaulting to
  **Qwen 2.5 1.5B** quantised to Q4_K_M.
- **Streaming** with cancel-on-new-keystroke, so a stale request never paints
  over your fresh typing.
- **Word-by-word acceptance** (`Tab`) or full-suggestion acceptance
  (`Shift+Tab`).
- **Sub-200 ms time-to-first-token** target, measured with the included
  benchmark script.
- **Per-app blocklist** — terminals (Konsole, Alacritty), password managers
  (KeePassXC, 1Password), and any app you nominate are skipped automatically.
- **Browser-aware**: dedicated handling for the GTK4/Firefox per-keystroke
  activate/deactivate cycle (so Zen Browser actually accumulates context) and
  for Chromium's text-input-v3 commit ordering (so ghost text doesn't leak
  into the field on focus loss).
- **Debounced** at 220 ms by default so Ollama isn't spammed mid-burst.
- **Anti-loop chat format** — uses `assistant`-prefill messaging on Ollama's
  `/api/chat` endpoint so the model continues your text instead of replying
  to it.
- **Privacy by design** — no telemetry, no cloud, no analytics, no auto-update
  ping.

---

## 4. Installation

> XType is currently developed on **CachyOS / Arch Linux with KDE Plasma on
> Wayland**. Other distros and desktops should work but are not yet validated.

### 4.1 System prerequisites (Arch / CachyOS)

```bash
sudo pacman -S \
  fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk fcitx5-breeze \
  cmake ninja gcc pkgconf curl \
  ollama python uv
```

For the IBus development prototype, additionally:

```bash
sudo pacman -S ibus
```

### 4.2 Pull the language model

```bash
ollama pull qwen2.5:1.5b      # production
ollama pull qwen2.5:0.5b      # smaller dev model (optional)
```

Make sure the Ollama daemon is running:

```bash
systemctl --user enable --now ollama
```

### 4.3 Build & install the Fcitx5 engine (production)

```bash
git clone https://github.com/<your-user>/XType.git
cd XType/fcitx5-engine

cmake -B build -G Ninja -DFCITX_INSTALL_USE_FCITX_SYS_PATHS=ON
ninja -C build
sudo ninja -C build install
```

This installs:

- `/usr/lib/fcitx5/libxtype-fcitx5.so`
- `/usr/share/fcitx5/addon/xtype.conf`
- `/usr/share/fcitx5/inputmethod/xtype.conf`

### 4.4 Wire input method environment variables

`~/.config/environment.d/input-method.conf`:

```ini
QT_IM_MODULE=fcitx
GTK_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
```

### 4.5 Activate Fcitx5 inside KWin

KDE Plasma on Wayland requires that Fcitx5 be **launched by KWin** — not as a
standalone autostart daemon. Open:

> **System Settings → Input & Output → Keyboard → Virtual Keyboard → Fcitx5**

Then disable any IBus autostart entry to avoid conflicts, log out, and log back
in. Verify with `fcitx5-configtool` that **XType** appears in your enabled
input methods.

### 4.6 Run the IBus prototype (development only)

```bash
cd XType/ibus-engine
uv venv
source .venv/bin/activate.fish      # fish; use activate for bash/zsh
uv pip install -e ".[dev]"
python -m pytest tests/

# Launch a session-scoped IBus daemon with the engine
ibus-daemon --replace --xim &
python -m engine.main
```

---

## 5. Configuration

XType reads `~/.config/xtype/config.toml` (created on first launch with sane
defaults). All keys are optional.

```toml
[inference]
model              = "qwen2.5:1.5b"
ollama_host        = "http://localhost:11434"
debounce_ms        = 220
min_context_chars  = 10
context_window     = 150
num_predict        = 30
temperature        = 0.3
top_p              = 0.9
stop_tokens        = [".", "!", "?", "\n"]

[behaviour]
tab_accepts_word      = true
passthrough_terminals = true
blocklist_apps        = ["konsole", "alacritty", "keepassxc", "1password"]
```

| Key                          | Effect                                                                        |
|------------------------------|-------------------------------------------------------------------------------|
| `model`                      | Any model installed in Ollama. `qwen2.5:0.5b` is faster, `1.5b` is sharper.   |
| `debounce_ms`                | Idle time before a request is fired. Lower = snappier, higher = less load.    |
| `min_context_chars`          | Minimum buffered characters before the engine asks for a suggestion.          |
| `context_window`             | Sliding-window size (chars) handed to the model as prefill.                   |
| `blocklist_apps`             | Case-insensitive substring match against `ic->program()`.                     |
| `passthrough_terminals`      | Convenience flag — when `true`, common terminals are always skipped.          |

Logs are written to `~/.local/share/xtype/engine.log` (IBus) and
`fcitx5-engine/debug.log` (Fcitx5 dev build) for post-mortem analysis.

---

## 6. Dependencies

### 6.1 Runtime

| Dependency  | Version    | Why                                                  |
|-------------|------------|------------------------------------------------------|
| Linux       | any modern | Tested on kernel 6.19 (CachyOS)                      |
| Wayland     | —          | Required for KDE Plasma; X11 works via XWayland       |
| Fcitx5      | ≥ 5.0      | Production input method                              |
| Ollama      | ≥ 0.1.30   | Local LLM runtime                                    |
| libcurl     | ≥ 7.80     | HTTP streaming to Ollama                             |
| Qwen 2.5    | 1.5B Q4_K_M | Default model (~1 GB on disk, ~1.5 GB resident)     |

For the IBus prototype:

| Dependency  | Version | Why                                            |
|-------------|---------|------------------------------------------------|
| IBus        | ≥ 1.5   | Development input method                       |
| Python      | ≥ 3.12  | `tomllib` is stdlib, asyncio improvements      |
| `aiohttp`   | ≥ 3.9   | Async streaming HTTP to Ollama                 |
| PyGObject   | system  | IBus bindings via GObject introspection        |

### 6.2 Build-time

- **CMake** ≥ 3.19
- **Ninja**
- **GCC** ≥ 13 or **Clang** ≥ 16 (C++20)
- **pkgconf**
- **Catch2 v3** *(only when `-DBUILD_TESTS=ON`)*

### 6.3 Development

- `pytest` ≥ 8.0, `pytest-asyncio` ≥ 0.23 — Python test suite
- `uv` — virtual environment & dependency manager

---

## 7. Systems & Technical Details

### 7.1 Architecture at a glance

```
┌────────────────────────────────────────────────────────────────────┐
│                       Application (Kate, Firefox, …)               │
│                                                                    │
│   ▲ keystrokes                                ▼ preedit / commit   │
└───┬──────────────────────────────────────────────────────────▲─────┘
    │                                                          │
    │  Wayland text-input-v2 / v3                              │
    │                                                          │
┌───▼──────────────────────────────────────────────────────────┴─────┐
│                         Fcitx5 / IBus daemon                       │
└───┬──────────────────────────────────────────────────────────▲─────┘
    │                                                          │
    │  C++ addon API  /  Python IBus.EngineBase                │
    │                                                          │
┌───▼──────────────────────────────────────────────────────────┴─────┐
│                         XTypeEngine                                │
│                                                                    │
│  ┌─────────────┐   ┌──────────────┐   ┌────────────────────────┐   │
│  │ KeyHandler  │──▶│  Debouncer   │──▶│  ContextBuffer (deque) │   │
│  └─────────────┘   └──────┬───────┘   └────────────┬───────────┘   │
│                           │                        │               │
│                           ▼                        ▼               │
│                  ┌─────────────────┐      ┌─────────────────┐      │
│                  │ InferenceClient │◀─────│ Prompt builder  │      │
│                  │ (libcurl, std::thread) │ (chat / prefill)│     │
│                  └────────┬────────┘      └─────────────────┘      │
└───────────────────────────┼────────────────────────────────────────┘
                            │  HTTP POST /api/chat (streaming)
                            ▼
                  ┌─────────────────────┐
                  │   Ollama (local)    │
                  │   Qwen 2.5 1.5B     │
                  └─────────────────────┘
```

### 7.2 Why the input method approach

Wayland's `text-input` protocol is ubiquitous: GTK, Qt, Electron, and SDL all
implement it. Input methods are explicitly granted the right to display
**preedit text** — visually distinct, not-yet-committed text that lives inside
the focused widget. That is exactly the surface ghost-text autocomplete needs,
and it ships universally.

The alternative paths were considered and rejected:

- **AT-SPI2 + key injection** — inconsistent on Wayland, no inline rendering.
- **`xdotool` / Uinput** — broken on Wayland, requires root or input groups,
  cannot render anything.
- **Per-app extensions** — does not scale to "every app".

David Edmundson's
[`input-method-playground`](https://invent.kde.org/plasma/input-method-playground)
validated this exact architecture on KDE Wayland, and the Vocalinux project
validated the IBus side.

### 7.3 Key event state machine

| Event                                | Action                                                        |
|--------------------------------------|---------------------------------------------------------------|
| Printable char                       | Pass through, append to buffer, dismiss old, debounce inference |
| `Tab` (suggestion active)            | Commit next word; update preedit with remainder               |
| `Shift+Tab` (suggestion active)      | Commit entire suggestion; clear preedit                       |
| `Esc` (suggestion active)            | Dismiss; clear preedit                                        |
| `Backspace` (suggestion active)      | Dismiss only                                                  |
| `Backspace` (no suggestion)          | Pop last char from buffer; pass through                       |
| `Enter`                              | Dismiss; pass through                                         |
| `Ctrl/Alt/Super` + key               | Always pass through                                           |
| Key release                          | Always ignored                                                |
| Focus out                            | **Discard** preedit (`commitString("")` + `clearPreedit`)     |
| Focus in / reset                     | Clear preedit, reset context                                  |

Critically, **focus-out must not commit ghost text.** The suggestion is the
model's, not the user's; committing it would leak unaccepted text into the
document on every Alt-Tab. The engine sends an empty commit followed by an
empty preedit before Fcitx5 finishes its deactivation sequence — necessary
because Chromium's text-input-v3 implementation otherwise commits the preedit
before the engine's deactivation callback can run.

### 7.4 Inference pipeline

- Background `std::thread` (C++) / asyncio task (Python), with an
  `std::atomic<bool>` cancel token / generation counter.
- Streaming NDJSON parser; tokens delivered to a callback as they arrive.
- Marshalled back to the main thread via Fcitx5's `eventDispatcher().schedule()`
  or GLib's `idle_add()` — never call IM APIs off the main thread.
- Default Ollama options: `num_predict=30`, `temperature=0.3`, `top_p=0.9`,
  `stop=[".", "!", "?", "\n"]`.
- **Anti-loop strategy:** the context is sent as an `assistant`-role message
  on `/api/chat`, with a small system prompt instructing the model to *continue*
  rather than *respond*. A literal `[CURSOR]` marker was tried and abandoned —
  prefill achieves the same effect without a magic token.

### 7.5 Context buffer

A `std::deque<char>` (1 KB sliding window) tracks the last `context_window`
characters typed. It exposes a small state machine:

```
appendChar()          ─┐
backspace()           ─┼─▶ auto-dismiss any active suggestion
setSuggestion(text)   ─┘
acceptNextWord()      ─▶ returns next word + space, advances index, appends to buffer
acceptAll()           ─▶ returns full remaining suggestion
dismiss()             ─▶ clears suggestion only
contextText()         ─▶ entire buffer as std::string
```

The Python and C++ implementations share an identical API and identical unit
tests (`pytest` and Catch2 respectively).

### 7.6 Performance notes

- Q4_K_M Qwen 2.5 1.5B sits around 1.5 GB resident on CPU and produces a first
  token in **70–180 ms** on a modern desktop with Ollama's default backends.
- The 0.5B variant is roughly 2× faster but noticeably less coherent — kept as
  a development convenience.
- `debounce_ms` defaults to **220 ms** (intentionally a touch above typical
  burst-typing intervals) to avoid issuing requests that will be cancelled
  mid-flight.

---

## 8. Project Structure

```
XType/
├── CLAUDE.md                  Engineering rules and session workflow
├── PLAN.md                    Session-by-session implementation plan
├── Summary.md                 High-level project context & rationale
├── pyproject.toml             Python (IBus prototype) packaging
│
├── ibus-engine/               Phase A — Python + IBus prototype
│   ├── xtype.xml              IBus component descriptor
│   ├── engine/
│   │   ├── main.py            IBus.init / factory / main loop
│   │   ├── engine.py          XTypeEngine(IBus.EngineBase) — key handler
│   │   ├── inference.py       Async Ollama client (aiohttp)
│   │   ├── context_buffer.py  Typed-text state machine
│   │   ├── debouncer.py       180 ms timer-based debounce
│   │   └── config.py          TOML config loader
│   └── tests/                 pytest suite
│
├── fcitx5-engine/             Phase B — C++20 + Fcitx5 production
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── xtype.{h,cpp}              InputMethodEngineV2 implementation
│   │   ├── inference_client.{h,cpp}   libcurl HTTP streaming client
│   │   ├── context_buffer.{h,cpp}     C++ port of the buffer state machine
│   │   └── config.h
│   ├── data/
│   │   ├── xtype.conf.in              Fcitx5 addon descriptor
│   │   └── xtype-im.conf              Input method registration
│   └── tests/                         Catch2 unit tests
│
├── shared/prompt_templates/   Prompts shared between engines
├── scripts/benchmark_ollama.py  TTFT benchmark harness (p50/p95/p99)
├── packaging/                 PKGBUILD, systemd service, .desktop file
└── docs/                      Integration test matrices
```

---

## 9. Application Compatibility

Status from the Phase B integration test matrix on KDE Plasma Wayland.

| Application       | Toolkit       | Wayland Protocol  | Status                                  |
|-------------------|---------------|-------------------|-----------------------------------------|
| Kate              | Qt 6          | text-input-v2     | Full preedit & word-by-word accept      |
| Firefox / Zen     | GTK 3 / 4     | text-input-v3     | Working (per-keystroke cycle handled)   |
| Chromium          | Blink         | text-input-v1/v3  | Needs `--wayland-text-input-version=3`  |
| LibreOffice       | GTK 3         | text-input-v3     | Working                                 |
| Electron apps     | Chromium      | text-input-v1     | Needs `ELECTRON_OZONE_PLATFORM_HINT=wayland` |
| Konsole           | Qt 6          | text-input-v2     | Blocklisted (terminal — preedit glitches) |
| Alacritty         | OpenGL        | text-input-v3     | Blocklisted                             |
| KeePassXC         | Qt 6          | text-input-v2     | Blocklisted (sensitive input)           |

---

## 10. Roadmap

### Shipped

- [x] **Phase 0** — Async Ollama client, ContextBuffer, Debouncer, TTFT benchmark harness
- [x] **Phase A** — IBus prototype (Python): engine skeleton, preedit UX, focus handling, TOML config, end-to-end validation on KDE Wayland
- [x] **Phase B** — Fcitx5 production (C++20): CMake addon, libcurl streaming client, engine core, KDE Plasma Wayland integration matrix
- [x] Browser compatibility hardening — Zen GTK4 per-keystroke cycle, Chromium text-input-v3 commit ordering
- [x] Anti-loop inference — `qwen2.5:1.5b` upgrade with assistant-prefill chat format

### Planned

- [ ] **Phase D — Personalization engine** (Sessions 15–17)
  - Opt-in writing-corpus collector with a hard-coded password-manager blocklist and AI-suggestion exclusion
  - Style profile extraction (representative exemplars, common openers) with privacy filtering for emails, secrets, and ID-shaped digit runs
  - Dynamic system-prompt assembly with style exemplars and a 5-minute corpus-mtime refresh, capped at 2000 chars to preserve TTFT
- [ ] **Phase E — Model & prompt customization** (Sessions 18–19)
  - Hardware-tier model presets (Low / Balanced / High / Enthusiast) with RAM-based auto-selection and Ollama health-check fallback
  - Personal prompt config — `description`, `tone`, `avoid_phrases`
- [ ] **Phase F — Per-app settings** (Session 20)
  - Per-app overrides for `enabled`, `model`, `debounce_ms`, `prompt_addendum`
  - Bounded LRU `InferenceClient` pool (cap 3) for per-app model routing
- [ ] **Phase G — Packaging & polish** (Sessions 21–22)
  - PKGBUILD + systemd user service + AUR submission
  - KCModule settings panel — General, Personalization, Per-app, Blocklist, Status

The full session-by-session plan — file layouts, interfaces, and per-session gotchas — lives in [`PLAN.md`](./PLAN.md). Speculative ideas (LoRA fine-tuning loop, per-field context detection, acceptance-history ranking) are tracked under "Future work" at the bottom of that file.

---

## 11. Acknowledgements

> ### A heartfelt shoutout to **Daniel Gräfe**
>
> XType would not exist without **Cotypist** — Daniel Gräfe's elegant macOS app
> that demonstrated, with great taste and engineering polish, that on-device
> AI autocomplete can feel like a native part of an operating system rather
> than a chat sidebar bolted onto your editor.
>
> Daniel is also the developer behind the long-running [Timing](https://timingapp.com)
> productivity app and runs **Accelerated Thought GmbH**. Cotypist's UX, its
> commitment to running entirely on-device, and its accuracy to the users writing style 
> inspired this project exists in the first place.
>
> If you are on macOS, please support his work directly: **<https://cotypist.app>**.
>
> XType is an independent re-implementation for Linux. It is not affiliated
> with, endorsed by, or derived from Cotypist's source code — only inspired by
> the experience it pioneered.

Additional thanks to:

- **David Edmundson** (KDE) — whose
  [input-method-playground](https://invent.kde.org/plasma/input-method-playground)
  was the canonical reference for getting an input method talking to KWin on
  Wayland.
- The **Fcitx5** and **IBus** maintainers — for two excellent, well-documented
  input method frameworks.
- The **Ollama** team — for making local LLM serving a one-line install.
- The **Qwen 2.5** team at Alibaba — for releasing a small model that is
  genuinely good at sentence continuation.
- The **Vocalinux** project — prior art validating custom IBus engines on KDE
  Wayland.

---

## 12. License

MIT — see [`LICENSE`](./LICENSE) for the full text.

> XType is built and maintained by **Kevin Sugar** on CachyOS / KDE Plasma.
> Issues, ideas, and pull requests are welcome.
