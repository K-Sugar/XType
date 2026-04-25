#include "xtype.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

// ── Helpers ───────────────────────────────────────────────────────────────────

static void log_warn(const char *msg) {
    std::fprintf(stderr, "[xtype] WARNING: %s\n", msg);
}

// ── XTypeEngine ───────────────────────────────────────────────────────────────

XTypeEngine::XTypeEngine(fcitx::AddonManager *manager)
    : _instance(manager->instance()),
      _inference(_cfg.inference)
{
    if (!_inference.health_check())
        log_warn("Ollama unreachable or model not available — suggestions disabled");
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void XTypeEngine::activate(const fcitx::InputMethodEntry &,
                           fcitx::InputContextEvent &event) {
    resetState(event.inputContext());
}

void XTypeEngine::deactivate(const fcitx::InputMethodEntry &,
                              fcitx::InputContextEvent &event) {
    // Ghost text is AI suggestion, not user input. Dismiss without committing —
    // committing on focus-out would silently insert text the user never accepted.
    resetState(event.inputContext());
}

void XTypeEngine::reset(const fcitx::InputMethodEntry &,
                        fcitx::InputContextEvent &event) {
    resetState(event.inputContext());
}

// ── Key event ─────────────────────────────────────────────────────────────────

void XTypeEngine::keyEvent(const fcitx::InputMethodEntry &,
                           fcitx::KeyEvent &event) {
    // Key releases are never handled.
    if (event.isRelease()) return;

    auto *ic = event.inputContext();

    // Blocklisted apps pass everything through.
    if (isBlocked(ic->program())) return;

    // Modifier combos (Ctrl / Alt / Super) pass through.
    auto states = event.key().states();
    if (states.testAny(fcitx::KeyStates{fcitx::KeyState::Ctrl,
                                       fcitx::KeyState::Alt,
                                       fcitx::KeyState::Super}))
        return;

    const bool hasSuggestion = _ctx.hasSuggestion();
    const auto sym = event.key().sym();

    // Tab — accept next word; Shift+Tab / ISO_Left_Tab — accept entire suggestion.
    if (sym == FcitxKey_Tab || sym == FcitxKey_ISO_Left_Tab) {
        if (!hasSuggestion) return;
        bool acceptAll = (sym == FcitxKey_ISO_Left_Tab) ||
                         states.test(fcitx::KeyState::Shift);
        std::string committed = acceptAll ? _ctx.acceptAll() : _ctx.acceptNextWord();
        ic->commitString(committed);
        updatePreedit(ic);
        event.filterAndAccept();
        return;
    }

    // Escape — dismiss suggestion.
    if (sym == FcitxKey_Escape) {
        if (!hasSuggestion) return;
        invalidate();
        updatePreedit(ic);
        event.filterAndAccept();
        return;
    }

    // Enter — dismiss suggestion, pass key through.
    if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
        if (hasSuggestion) {
            invalidate();
            updatePreedit(ic);
        }
        return;
    }

    // Backspace — dismiss if suggestion active (consume key); otherwise let app handle it.
    if (sym == FcitxKey_BackSpace) {
        if (hasSuggestion) {
            invalidate();
            updatePreedit(ic);
            event.filterAndAccept();
        } else {
            _ctx.backspace();
            _debounceTimer.reset();
        }
        return;
    }

    // Printable ASCII (space through ~). Pass key through to app, update buffer,
    // arm debounce timer for inference.
    if (sym >= FcitxKey_space && sym <= FcitxKey_asciitilde) {
        invalidate();
        updatePreedit(ic);
        _ctx.appendChar(static_cast<char>(sym));

        _debounceTimer.reset();
        uint64_t fireUs = fcitx::now(CLOCK_MONOTONIC) +
                          static_cast<uint64_t>(_cfg.inference.debounce_ms) * 1000;
        auto icRef = ic->watch();
        auto *icPtr = ic;
        _debounceTimer = _instance->eventLoop().addTimeEvent(
            CLOCK_MONOTONIC, fireUs, /*accuracy=*/0,
            [this, icRef, icPtr](fcitx::EventSourceTime *, uint64_t) mutable {
                // Do NOT reset _debounceTimer here — would destroy this object.
                requestInference(std::move(icRef), icPtr);
                return false;  // one-shot
            });
        return;
    }

    // Everything else passes through.
}

// ── Inference ─────────────────────────────────────────────────────────────────

void XTypeEngine::requestInference(
    fcitx::TrackableObjectReference<fcitx::InputContext> icRef,
    fcitx::InputContext *icPtr)
{
    auto ctx = _ctx.contextText();
    if (static_cast<int>(ctx.size()) < _cfg.inference.min_context_chars)
        return;
    if (static_cast<int>(ctx.size()) > _cfg.inference.context_window)
        ctx = ctx.substr(ctx.size() -
                         static_cast<size_t>(_cfg.inference.context_window));

    ++_gen;
    const uint64_t myGen = _gen;

    _inference.request(
        std::move(ctx),
        [this, myGen, icRef, icPtr](std::string token) {
            _instance->eventDispatcher().scheduleWithContext(
                icRef,
                [this, myGen, tok = std::move(token), icPtr]() {
                    if (myGen != _gen) return;
                    std::string current =
                        _ctx.hasSuggestion() ? *_ctx.suggestion() : "";
                    _ctx.setSuggestion(current + tok);
                    updatePreedit(icPtr);
                });
        },
        [this, myGen, icRef, icPtr]() {
            _instance->eventDispatcher().scheduleWithContext(
                icRef,
                [this, myGen, icPtr]() {
                    if (myGen != _gen) return;
                    if (_ctx.hasSuggestion()) {
                        std::string s = *_ctx.suggestion();
                        while (!s.empty() &&
                               std::isspace(static_cast<unsigned char>(s.back())))
                            s.pop_back();
                        _ctx.setSuggestion(std::move(s));
                    }
                    updatePreedit(icPtr);
                });
        },
        [](std::string) {}  // on_error: silent
    );
}

// ── Preedit ───────────────────────────────────────────────────────────────────

void XTypeEngine::updatePreedit(fcitx::InputContext *ic) {
    auto &panel = ic->inputPanel();
    if (_ctx.hasSuggestion()) {
        fcitx::Text t(*_ctx.suggestion(), fcitx::TextFormatFlag::Underline);
        panel.setClientPreedit(t);
    } else {
        panel.setClientPreedit(fcitx::Text{});
    }
    ic->updatePreedit();
}

void XTypeEngine::clearPreedit(fcitx::InputContext *ic) {
    ic->inputPanel().setClientPreedit(fcitx::Text{});
    ic->updatePreedit();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void XTypeEngine::resetState(fcitx::InputContext *ic) {
    _debounceTimer.reset();
    _inference.cancel();
    ++_gen;
    _ctx.reset();
    clearPreedit(ic);
}

void XTypeEngine::invalidate() {
    ++_gen;
    _ctx.dismiss();
}

bool XTypeEngine::isBlocked(const std::string &program) const {
    for (const auto &app : _cfg.behaviour.blocklist_apps) {
        if (program.find(app) != std::string::npos) return true;
    }
    return false;
}

// ── Addon factory ─────────────────────────────────────────────────────────────

class XTypeAddonFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new XTypeEngine(manager);
    }
};

FCITX_ADDON_FACTORY(XTypeAddonFactory)
