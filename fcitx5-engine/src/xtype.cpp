#include "xtype.h"

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <utility>

#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

#include "path_utils.h"
#include "prompt_builder.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

static FILE *dbg_file() {
    static FILE *f = []() -> FILE* {
        auto dir = path_utils::expandTilde("~/.local/share/xtype");
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return std::fopen((dir / "fcitx5.log").c_str(), "a");
    }();
    return f;
}
static void dbg(const char *fmt, ...) {
    FILE *f = dbg_file();
    if (!f) return;
    std::time_t t = std::time(nullptr);
    char ts[20];
    std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&t));
    std::fprintf(f, "[%s] ", ts);
    va_list ap; va_start(ap, fmt); std::vfprintf(f, fmt, ap); va_end(ap);
    std::fputc('\n', f); std::fflush(f);
}
static void log_warn(const char *msg) {
    std::fprintf(stderr, "[xtype] WARNING: %s\n", msg);
    dbg("WARN: %s", msg);
}

// ── XTypeEngine ───────────────────────────────────────────────────────────────

XTypeEngine::XTypeEngine(fcitx::AddonManager *manager)
    : _instance(manager->instance()),
      _inference(_cfg.inference)
{
    // Observability env reads (once, on the main thread).
    {
        const char* v = std::getenv("XTYPE_DEBUG_VERBOSE");
        if (v && *v && v[0] != '0') _debugVerbose = true;
        // Sentinel file fallback: ~/.local/share/xtype/.debug_verbose
        if (!_debugVerbose) {
            auto sentinel = path_utils::expandTilde("~/.local/share/xtype/.debug_verbose");
            std::error_code ec;
            if (std::filesystem::exists(sentinel, ec)) _debugVerbose = true;
        }
    }
    {
        const char* r = std::getenv("XTYPE_PROFILE_REFRESH_SEC");
        if (r && *r) {
            int n = std::atoi(r);
            if (n >= 30) _profileRefreshSec = n;
        } else {
            _profileRefreshSec = kProfileRefreshSec;
        }
    }

    _phraseBlock.load(_cfg.behaviour.blocked_phrases);
    dbg("XTypeEngine loaded, model=%s", _cfg.inference.model.c_str());
    if (_debugVerbose)
        dbg("[debug] verbose mode ON — sentence content will be written to debug.log");
    if (_profileRefreshSec != kProfileRefreshSec)
        dbg("[profile] refresh interval override: %d sec", _profileRefreshSec);

    if (!_inference.health_check())
        log_warn("Ollama unreachable or model not available — suggestions disabled");
    else
        dbg("health_check OK");

    if (_cfg.learning.enabled && !_cfg.learning.corpus_path.empty()) {
        _corpus = std::make_unique<CorpusCollector>(_cfg.learning);
        if (_corpus->disabled()) {
            log_warn("corpus collector disabled: HOME unresolved or path invalid");
            _corpus.reset();
        } else {
            dbg("[corpus] enabled path=%s", _cfg.learning.corpus_path.c_str());
            // Sink runs on the collector's flush thread; marshal to main before dbg().
            _corpus->setLogSink([this](std::string msg) {
                _instance->eventDispatcher().schedule(
                    [m = std::move(msg)]() mutable {
                        dbg("[corpus] %s", m.c_str());
                    });
            });
        }

        // Resolve absolute corpus + profile paths once; both worker and isStale need them.
        auto cp = path_utils::expandTilde(_cfg.learning.corpus_path);
        if (!cp.empty() && cp.string().front() != '~') {
            _corpusPathExpanded  = cp.string();
            _profilePathExpanded = (cp.parent_path() / "style_profile.json").string();
            kickProfileLoad();
            armProfileRefreshTimer();
        }
    } else {
        dbg("[corpus] disabled (opt-in; edit config.h LearningConfig::enabled to enable)");
    }
}

XTypeEngine::~XTypeEngine() {
    if (_profileWorker && _profileWorker->joinable())
        _profileWorker->join();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void XTypeEngine::activate(const fcitx::InputMethodEntry &,
                           fcitx::InputContextEvent &event) {
    auto *ic = event.inputContext();
    const std::string &prog = ic->program();
    dbg("activate prog=%s _lastProg=%s", prog.c_str(), _lastProg.c_str());
    if (prog != _lastProg)
        resetState(ic);       // app changed: full reset including _ctx
    else
        resetInferenceOnly(); // same app cycling (Zen GTK4 pattern): preserve _ctx
    _lastProg = prog;
}

void XTypeEngine::deactivate(const fcitx::InputMethodEntry &,
                              fcitx::InputContextEvent &event) {
    auto *ic = event.inputContext();
    dbg("deactivate prog=%s hasSuggestion=%d", ic->program().c_str(), (int)_ctx.hasSuggestion());
    // commitString("") is an atomic text-input-v3 transaction: it sends commit_string("") +
    // set_preedit_string("", 0, 0) + commit() in one shot. Unlike updatePreedit() alone, this
    // guarantees the client receives a commit() event, which is required for changes to take
    // effect. Called unconditionally so Chromium always commits empty rather than ghost text.
    ic->commitString("");
    clearPreedit(ic);
    if (_corpus &&
        static_cast<int>(_userTypedSinceLastTerminator.size()) >=
            _cfg.learning.min_sentence_chars) {
        harvestSentence(ic->program());
    } else {
        _userTypedSinceLastTerminator.clear();
    }
    _ctx.dismiss();
    resetInferenceOnly(); // cancel debounce + inference; _ctx typed text preserved for same-app cycling
}

void XTypeEngine::reset(const fcitx::InputMethodEntry &,
                        fcitx::InputContextEvent &event) {
    auto *ic = event.inputContext();
    dbg("reset prog=%s", ic->program().c_str());
    // The IM protocol "reset" means discard preedit state — not wipe the user's typed context.
    // GTK4 apps (Zen/Firefox) send reset() after every keystroke. Do NOT cancel _debounceTimer
    // here: that would prevent the 180ms debounce from ever firing, so inference would never run.
    _ctx.dismiss();
    clearPreedit(ic);
    _inference.cancel();
    ++_gen;
}

// ── Key event ─────────────────────────────────────────────────────────────────

void XTypeEngine::keyEvent(const fcitx::InputMethodEntry &,
                           fcitx::KeyEvent &event) {
    // Key releases are never handled.
    if (event.isRelease()) return;

    auto *ic = event.inputContext();

    // Blocklisted apps pass everything through.
    if (isBlocked(ic->program())) return;

    // Phrase blocklist stub (always false until future session implements matching).
    if (_phraseBlock.matches(_ctx.contextText())) return;

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
        ++_metrics.suggestions_accepted;
        _metrics.chars_accepted += committed.size();
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
        if (_corpus && !_userTypedSinceLastTerminator.empty())
            harvestSentence(ic->program());
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
            if (!_userTypedSinceLastTerminator.empty())
                _userTypedSinceLastTerminator.pop_back();
            _debounceTimer.reset();
        }
        return;
    }

    // Printable ASCII (space through ~). Pass key through to app, update buffer,
    // arm debounce timer for inference.
    if (sym >= FcitxKey_space && sym <= FcitxKey_asciitilde) {
        dbg("key '%c' prog=%s ctx_len=%zu", static_cast<char>(sym),
            ic->program().c_str(), _ctx.contextText().size());
        invalidate();
        updatePreedit(ic);
        const char ch = static_cast<char>(sym);
        _ctx.appendChar(ch);

        // User-typed-only buffer for corpus harvest (excludes AI accept paths).
        if (_userTypedSinceLastTerminator.size() >= kUserBufCap)
            _userTypedSinceLastTerminator.erase(0, kUserBufCap / 2);
        _userTypedSinceLastTerminator.push_back(ch);
        if (ch == '.' || ch == '!' || ch == '?')
            harvestSentence(ic->program());

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
    ++_metrics.suggestions_generated;
    const uint64_t myGen = _gen;

    dbg("requestInference ctx='%.40s...'", ctx.c_str());
    _inference.request(
        std::move(ctx),
        [this, myGen, icRef, icPtr](std::string token) {
            dbg("token received: '%s' icValid=%d", token.c_str(), (int)icRef.isValid());
            if (!icRef.isValid()) { dbg("on_token: icRef invalid — dropping"); return; }
            _instance->eventDispatcher().scheduleWithContext(
                icRef,
                [this, myGen, tok = std::move(token), icPtr]() {
                    if (myGen != _gen) return;
                    std::string current =
                        _ctx.hasSuggestion() ? *_ctx.suggestion() : "";
                    _ctx.setSuggestion(current + tok);
                    dbg("preedit updated: '%s'", _ctx.suggestion()->c_str());
                    updatePreedit(icPtr);
                });
        },
        [this, myGen, icRef, icPtr]() {
            dbg("inference done, myGen=%llu curGen=%llu", (unsigned long long)myGen, (unsigned long long)_gen);
            _instance->eventDispatcher().scheduleWithContext(
                icRef,
                [this, myGen, icPtr]() {
                    if (myGen != _gen) return;
                    if (_ctx.hasSuggestion()) {
                        std::string s = *_ctx.suggestion();
                        // Strip trailing whitespace.
                        while (!s.empty() &&
                               std::isspace(static_cast<unsigned char>(s.back())))
                            s.pop_back();
                        const std::string ctx = _ctx.contextText();
                        // Strip tail-of-context echo: model repeats recently typed text.
                        constexpr size_t kMaxCheck = 80;
                        size_t check = std::min({s.size(), ctx.size(), kMaxCheck});
                        for (size_t len = check; len >= 4; --len) {
                            if (ctx.compare(ctx.size() - len, len, s, 0, len) == 0) {
                                s.erase(0, len);
                                break;
                            }
                        }
                        // Strip head-of-context echo: model reproduces the document
                        // from the beginning instead of completing at [CURSOR].
                        // Case-insensitive match on the first 12 chars is enough to
                        // identify this pattern without false-positives on short words.
                        constexpr size_t kHeadCheck = 12;
                        if (!s.empty() && s.size() >= kHeadCheck && ctx.size() >= kHeadCheck) {
                            auto lower = [](std::string t) {
                                std::transform(t.begin(), t.end(), t.begin(),
                                               [](unsigned char c){ return std::tolower(c); });
                                return t;
                            };
                            if (lower(s.substr(0, kHeadCheck)) == lower(ctx.substr(0, kHeadCheck)))
                                s.clear();
                        }
                        _ctx.setSuggestion(std::move(s));
                    }
                    updatePreedit(icPtr);
                });
        },
        [](std::string err) { dbg("inference error: %s", err.c_str()); }
    );
}

// ── Preedit ───────────────────────────────────────────────────────────────────

void XTypeEngine::updatePreedit(fcitx::InputContext *ic) {
    auto &panel = ic->inputPanel();
    if (_ctx.hasSuggestion()) {
        fcitx::Text t(*_ctx.suggestion(),
                      fcitx::TextFormatFlags{fcitx::TextFormatFlag::Underline,
                                             fcitx::TextFormatFlag::DontCommit});
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
    _userTypedSinceLastTerminator.clear();
    clearPreedit(ic);
}

void XTypeEngine::resetInferenceOnly() {
    _debounceTimer.reset();
    _inference.cancel();
    ++_gen;
}

void XTypeEngine::invalidate() {
    ++_gen;
    _ctx.dismiss();
}

bool XTypeEngine::isBlocked(const std::string &program) const {
    std::string prog_lower = program;
    std::transform(prog_lower.begin(), prog_lower.end(), prog_lower.begin(), ::tolower);
    for (const auto &app : _cfg.behaviour.blocklist_apps)
        if (prog_lower.find(app) != std::string::npos) return true;
    return false;
}

void XTypeEngine::harvestSentence(const std::string &program) {
    if (!_corpus) { _userTypedSinceLastTerminator.clear(); return; }
    if (CorpusCollector::isHardBlocked(program)) {
        dbg("[harvest] drop hard-blocked app=%s", program.c_str());
        _userTypedSinceLastTerminator.clear();
        return;
    }
    if (isBlocked(program)) {
        dbg("[harvest] drop blocked app=%s", program.c_str());
        _userTypedSinceLastTerminator.clear();
        return;
    }
    std::string s;
    s.swap(_userTypedSinceLastTerminator);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.erase(0, 1);
    if (static_cast<int>(s.size()) < _cfg.learning.min_sentence_chars) {
        dbg("[harvest] drop too-short len=%zu", s.size());
        return;
    }
    dbg("[harvest] ok len=%zu", s.size());
    if (_debugVerbose) {
        // Cap snippet to first 60 chars so very long sentences don't blow up the log.
        std::string snip = s.size() > 60 ? s.substr(0, 60) + "..." : s;
        dbg("[harvest] verbose content='%s'", snip.c_str());
    }
    _corpus->record(std::move(s));
}

// ── Personalization (Session 17) ──────────────────────────────────────────────

void XTypeEngine::applyPrompt() {
    PromptInputs in;
    in.base            = std::string(InferenceClient::base_system_prompt());
    in.profile         = _profile.get();
    in.includeExamples = _cfg.learning.include_examples_in_prompt;
    in.budgetChars     = kPromptBudget;
    bool truncated = false;
    auto prompt = buildSystemPrompt(in, &truncated);
    if (truncated)
        log_warn("system prompt truncated to fit 2000-char budget");
    size_t exCount = _profile ? _profile->exemplars().size() : (size_t)0;
    dbg("[prompt] set %zu chars exemplars=%zu truncated=%d",
        prompt.size(), exCount, truncated ? 1 : 0);
    _inference.set_system_prompt(std::move(prompt));
}

void XTypeEngine::kickProfileLoad() {
    if (_profileLoading.exchange(true)) {
        // Marshal to main thread for thread-safe dbg.
        _instance->eventDispatcher().schedule(
            []{ dbg("[profile] refresh: skipped (still loading)"); });
        return;
    }

    if (_profileWorker && _profileWorker->joinable())
        _profileWorker->join();

    std::string corpusPath  = _corpusPathExpanded;
    std::string profilePath = _profilePathExpanded;

    _profileWorker.emplace([this, corpusPath, profilePath]() {
        StyleProfile sp = StyleProfile::deserialize(profilePath);
        bool stale = StyleProfile::isStale(corpusPath, profilePath);
        bool needRebuild = sp.exemplars().empty() || stale;
        std::string reason;
        if (sp.exemplars().empty())               reason = "missing-or-empty";
        else if (stale)                           reason = "stale";
        else                                      reason = "none";

        if (needRebuild) {
            sp = StyleProfile{};
            sp.loadFromCorpus(corpusPath);
            if (!sp.exemplars().empty()) {
                sp.serialize(profilePath);
                _instance->eventDispatcher().schedule(
                    [path = profilePath]{ dbg("[profile] serialize: %s", path.c_str()); });
            }
        }

        size_t exCount = sp.exemplars().size();
        int    avg     = sp.avgSentenceLen();
        int    cnt     = sp.sentenceCount();
        _instance->eventDispatcher().schedule(
            [reason, needRebuild, exCount, avg, cnt]{
                dbg("[profile] stale=%s reason=%s",
                    needRebuild ? "true" : "false", reason.c_str());
                dbg("[profile] load: exemplars=%zu avg=%d count=%d",
                    exCount, avg, cnt);
            });

        _instance->eventDispatcher().schedule(
            [this, sp = std::move(sp)]() mutable {
                _profile = std::make_unique<StyleProfile>(std::move(sp));
                applyPrompt();
                _profileLoading.store(false);
            });
    });
}

void XTypeEngine::armProfileRefreshTimer() {
    uint64_t fireUs = fcitx::now(CLOCK_MONOTONIC) +
                      static_cast<uint64_t>(_profileRefreshSec) * 1000000ull;
    _profileRefreshTimer = _instance->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC, fireUs, /*accuracy=*/0,
        [this](fcitx::EventSourceTime *, uint64_t) {
            dbg("[profile] refresh: timer fired");
            kickProfileLoad();
            armProfileRefreshTimer();
            return false;  // one-shot; we re-arm above
        });
}

// ── Addon factory ─────────────────────────────────────────────────────────────

class XTypeAddonFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new XTypeEngine(manager);
    }
};

FCITX_ADDON_FACTORY(XTypeAddonFactory)
