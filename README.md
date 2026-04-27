<div align="center">

# XType

**System-wide locally running AI text autocomplete for Linux.**

Inline ghost-text completions powered by a local LLM, delivered through the
Wayland-native input method framework. No cloud, no telemetry, just a clean and simple
Fcitx5 engine that works in every text field on your desktop.

[![Status](https://img.shields.io/badge/status-v1.0.0%20%E2%80%94%20production-blue)]()
[![Platform](https://img.shields.io/badge/platform-Linux%20%E2%80%A2%20Wayland%20%E2%80%A2%20KDE-1793D1)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)]()
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

It is implemented as a native Fcitx5 input method engine so it works across
GTK, Qt, Electron and SDL applications without ever touching the keyboard,
the X server, or the network.

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

---

## 3. Features

- **Inline ghost text** rendered directly in the application's own text field —
  no popup window, no overlay, no z-order race with your tiling WM.
- **Local-only inference** via [Ollama](https://ollama.com), defaulting to
  **Qwen 2.5 1.5B** quantised to Q4_K_M.
- **Streaming** with cancel-on-new-keystroke, so a stale request never paints
  over your fresh typing.
- **Word-by-word acceptance** (`Tab`) or full-suggestion acceptance
  (`Shift+Tab`). Accept key is configurable: Tab, Enter, or →.
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
- **`xtype-settings` Qt6/QML app** — full settings interface for all engine
  options, accessible from KDE application menus; writes
  `~/.config/xtype/config.toml` and shows a reload banner when changes need
  to be applied.
- **Personalisation** — opt-in writing corpus (`~/.local/share/xtype/corpus.txt`),
  style profile extraction, voice-match strength slider (0–100), and a
  forget-after-days rolling window to keep the profile fresh automatically.
- **User voice profile** — free-text description (≤ 500 chars), tone selector
  (Default / Casual / Professional / Technical / Concise), and a per-phrase
  avoid list — all injected into the system prompt.
- **Per-app overrides** — independently configure suggestion length, mode,
  debounce, and a prompt addendum per application.
- **Live observability** — engine writes `metrics.json` and `recent_events.json`
  after every inference; the sidebar shows acceptance rate, latency p50/p95,
  and a recent-events log without a page reload.
- **Privacy by design** — no telemetry, no cloud, no analytics, no auto-update
  ping. Password managers are permanently hard-blocked from corpus collection;
  AI suggestions never enter the corpus.

---

## 4. Installation

> XType is currently developed on **CachyOS / Arch Linux with KDE Plasma on
> Wayland**. Other distros and desktops should work but are not yet validated.

### 4.1 System prerequisites (Arch / CachyOS)

```bash
sudo pacman -S \
  fcitx5 fcitx5-configtool fcitx5-qt fcitx5-gtk fcitx5-breeze \
  cmake ninja gcc pkgconf curl \
  ollama
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

### 4.4 Build & install the settings app

```bash
cmake -B settings/qt6-app/build -G Ninja -S settings/qt6-app
ninja -C settings/qt6-app/build
sudo ninja -C settings/qt6-app/build install   # installs xtype-settings + .desktop
```

Or run it directly without installing:

```bash
./settings/qt6-app/build/xtype-settings
```

### 4.5 Wire input method environment variables

`~/.config/environment.d/input-method.conf`:

```ini
QT_IM_MODULE=fcitx
GTK_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
```

### 4.6 Activate Fcitx5 inside KWin

KDE Plasma on Wayland requires that Fcitx5 be **launched by KWin** — not as a
standalone autostart daemon. Open:

> **System Settings → Input & Output → Keyboard → Virtual Keyboard → Fcitx5**

Then disable any IBus autostart entry to avoid conflicts, log out, and log back
in. Verify with `fcitx5-configtool` that **XType** appears in your enabled
input methods.

---

## 5. Configuration

**Recommended:** use the `xtype-settings` GUI app (see `settings/qt6-app/README.md`).
It writes `~/.config/xtype/config.toml` automatically and shows a reload banner
when changes need to be applied.

**CLI fallback:** edit `~/.config/xtype/config.toml` directly (created on first launch
with sane defaults). All keys are optional.

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
engine_enabled      = true
accept_full_key     = "tab"    # "tab" | "enter" | "right"
partial_accept      = true
esc_dismisses       = true
blocklist_apps      = ["konsole", "alacritty", "keepassxc", "1password"]

[learning]
enabled             = false    # opt-in
corpus_path         = "~/.local/share/xtype/corpus.txt"
voice_strength      = 50       # 0–100
forget_after_days   = 0        # 0 = disabled

[user_prompt]
description         = ""       # ≤ 500 chars; injected into system prompt
tone                = ""       # "casual" | "professional" | "technical" | "concise"
avoid_phrases       = []

[apps.firefox]                 # per-app override example
num_predict         = 20
mode                = "Default"
```

| Key                          | Effect                                                                              |
|------------------------------|-------------------------------------------------------------------------------------|
| `model`                      | Any model installed in Ollama. `qwen2.5:0.5b` is faster, `1.5b` is sharper.        |
| `debounce_ms`                | Idle time before a request is fired. Lower = snappier, higher = less load.         |
| `accept_full_key`            | Which key accepts the full suggestion: `"tab"`, `"enter"`, or `"right"`.           |
| `partial_accept`             | When `true`, the accept key commits one word at a time; `Shift+key` commits all.   |
| `min_context_chars`          | Minimum buffered characters before the engine asks for a suggestion.               |
| `context_window`             | Sliding-window size (chars) handed to the model as prefill.                        |
| `blocklist_apps`             | Case-insensitive component match against `ic->program()`.                          |
| `learning.voice_strength`    | 0 = no corpus examples in prompt; 100 = all exemplars included; proportional.     |
| `learning.forget_after_days` | Prune corpus entries older than N days. 0 disables pruning.                        |
| `user_prompt.description`    | Free-text description of yourself injected into the system prompt (≤ 500 chars).   |

Logs are written to `~/.local/share/xtype/fcitx5.log` (engine debug) and
`~/.local/share/xtype/inference.log` (inference requests). Both are viewable in
the LogTail panel in `xtype-settings`.

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

### 6.2 Build-time

- **CMake** ≥ 3.19
- **Ninja**
- **GCC** ≥ 13 or **Clang** ≥ 16 (C++20)
- **pkgconf**
- **Catch2 v3** *(only when `-DBUILD_TESTS=ON`)*

### 6.3 Development

- **Catch2 v3** — C++ unit tests (engine + settings app)

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
│                            Fcitx5 daemon                           │
└───┬──────────────────────────────────────────────────────────▲─────┘
    │                                                          │
    │  C++ addon API                                           │
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
validated this exact architecture on KDE Wayland.

### 7.3 Key event state machine

| Event                                | Action                                                        |
|--------------------------------------|---------------------------------------------------------------|
| Printable char                       | Pass through, append to buffer, dismiss old, debounce inference |
| `Tab` (suggestion active)            | Commit next word; update preedit with remainder               |
| `Shift+Tab` (suggestion active)      | Commit entire suggestion; clear preedit                       |
| `Esc` (suggestion active)            | Dismiss; clear preedit                                        |
| `Backspace` (suggestion active)      | Dismiss only                                                  |
| `Backspace` (no suggestion)          | Pop last char from buffer; pass through                       |
| `Enter` (suggestion active)          | Accept full suggestion if `accept_full_key = "enter"`; otherwise dismiss and pass through |
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

- Background `std::thread` with an `std::atomic` generation counter for cancellation.
- Streaming NDJSON parser; tokens delivered to a callback as they arrive.
- Marshalled back to the main thread via Fcitx5's `eventDispatcher().schedule()` — never call IM APIs off the main thread.
- Default Ollama options: `num_predict=30`, `temperature=0.3`, `top_p=0.9`,
  `stop=[".", "!", "?", "\n"]`.
- **Anti-loop strategy:** the context is sent as an `assistant`-role message
  on `/api/chat`, with a small system prompt instructing the model to *continue*
  rather than *respond*. A literal `[CURSOR]` marker was tried and abandoned —
  prefill achieves the same effect without a magic token.

### 7.5 Context buffer

A `std::deque<char>` (500-char sliding window) tracks the last `context_window`
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

The implementation is covered by Catch2 unit tests.

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
├── Summary.md                 High-level project context & rationale
│
├── fcitx5-engine/             Production Fcitx5 addon (C++20)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── xtype.{h,cpp}                InputMethodEngineV2 — key handler, lifecycle
│   │   ├── inference_client.{h,cpp}     libcurl streaming client, system prompt mgmt
│   │   ├── context_buffer.{h,cpp}       500-char rolling context window
│   │   ├── config.h                     Config structs (InferenceConfig, etc.)
│   │   ├── config_loader.{h,cpp}        TOML → config struct parser
│   │   ├── corpus_collector.{h,cpp}     Opt-in background corpus writer
│   │   ├── style_profile.{h,cpp}        Exemplar extraction + JSON round-trip
│   │   ├── prompt_builder.{h,cpp}       System prompt assembly with budget cap
│   │   ├── phrase_blocklist.h           Case-insensitive phrase filter
│   │   ├── engine_metrics.h             Atomic latency / acceptance counters
│   │   ├── recent_events.h              Ring buffer of last N inference events
│   │   └── path_utils.{h,cpp}           ~ expansion helper
│   ├── data/
│   │   ├── xtype.conf.in                Fcitx5 addon descriptor
│   │   └── xtype-im.conf                Input method registration
│   └── tests/                           Catch2 unit tests
│
├── settings/qt6-app/          Qt6/QML settings interface (xtype-settings)
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── config_store.{h,cpp}         Single TOML source of truth for QML
│   │   ├── engine_probe.{h,cpp}         Polls metrics.json / recent_events.json
│   │   ├── corpus_stats.{h,cpp}         CorpusStats QML context property
│   │   ├── style_profile_model.{h,cpp}  Exposes exemplars to QML
│   │   ├── log_tail.{h,cpp}             Tails fcitx5.log / inference.log
│   │   ├── reloader.{h,cpp}             Triggers fcitx5-remote -r
│   │   └── ollama_client.{h,cpp}        Fetches available model list
│   └── qml/
│       ├── pages/             PageGeneral, PagePersonalisation, PageBlockList,
│       │                      PagePerApp, PageModel, PageAbout
│       └── primitives/        Shared UI components (XSlider, XToggle, etc.)
│
├── shared/prompt_templates/   Prompts shared between engines
├── scripts/benchmark_ollama.py  TTFT benchmark harness (p50/p95/p99)
├── packaging/                 PKGBUILD, systemd service, .desktop file
└── docs/                      Integration test matrices, per-app mode docs
```

---

## 9. Application Compatibility

Status from the integration test matrix on KDE Plasma Wayland.

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
- [x] **Fcitx5 engine** — CMake C++20 addon, libcurl streaming client, engine core, KDE Plasma Wayland integration matrix
- [x] Browser compatibility hardening — Zen GTK4 per-keystroke cycle, Chromium text-input-v3 commit ordering
- [x] Anti-loop inference — `qwen2.5:1.5b` upgrade with assistant-prefill chat format
- [x] **Personalisation engine** — opt-in corpus collector, style profile extraction (representative exemplars, privacy filtering), dynamic system-prompt assembly capped at 2000 chars, 5-minute background refresh
- [x] **User voice profile** — `description`, `tone`, `avoid_phrases`, voice-match strength slider, forget-after-days rolling window
- [x] **Per-app overrides** — `enabled`, `model`, `debounce_ms`, `num_predict`, `mode`, `prompt_addendum` per application
- [x] **Qt6/QML settings app** (`xtype-settings`) — all six pages wired to the engine via `config.toml`; sidebar live metrics; reload banner
- [x] **Engine ↔ UI observability** — `metrics.json`, `recent_events.json`, `inference.log`, LogTail panel

### Planned

- [ ] **Model presets** — hardware-tier presets (Low / Balanced / High / Enthusiast) with RAM-based auto-selection and Ollama health-check fallback
- [ ] **PKGBUILD + AUR** — packaging, systemd user service, AUR submission
- [ ] **Learning model improvements** — acceptance-history ranking, per-field context analysis
- [ ] **Screen reader context** (exploratory) — stronger context from accessibility APIs for better suggestions

The full session-by-session plan — file layouts, interfaces, and per-session gotchas — lives in [`PLAN.md`](./PLAN.md).

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
