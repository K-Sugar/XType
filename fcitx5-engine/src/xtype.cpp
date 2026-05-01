#include "xtype.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
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

#include "config_loader.h"
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
    _cfg = config_loader::load();
    _embedClient = std::make_unique<OllamaEmbedClient>(_cfg.inference.ollama_host);
    dbg("config loaded: model=%s debounce=%dms engine_enabled=%d",
        _cfg.inference.model.c_str(),
        _cfg.inference.debounce_ms,
        (int)_cfg.behaviour.engine_enabled);

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
            _embeddingIndexPath  = (cp.parent_path() / "corpus_embeddings.bin").string();
            kickProfileLoad();
            armProfileRefreshTimer();
        }
    } else {
        dbg("[corpus] disabled (opt-in; edit config.h LearningConfig::enabled to enable)");
    }

    _inference.update_config(_cfg.inference);
}

XTypeEngine::~XTypeEngine() {
    if (_profileWorker && _profileWorker->joinable())
        _profileWorker->join();
}

void XTypeEngine::reloadConfig() {
    XTypeConfig newCfg = config_loader::load();
    dbg("[reload] config re-read: model=%s debounce=%dms",
        newCfg.inference.model.c_str(), newCfg.inference.debounce_ms);

    _phraseBlock.load(newCfg.behaviour.blocked_phrases);

    bool learningWas = _cfg.learning.enabled;
    bool learningNow = newCfg.learning.enabled;
    _cfg = newCfg;

    if (learningWas && !learningNow) {
        _corpus.reset();
        dbg("[reload] corpus collector disabled");
    } else if (!learningWas && learningNow && !_cfg.learning.corpus_path.empty()) {
        _corpus = std::make_unique<CorpusCollector>(_cfg.learning);
        if (_corpus->disabled()) { _corpus.reset(); log_warn("corpus disabled after reload"); }
    }

    if (_corpus) {
        auto cp = path_utils::expandTilde(_cfg.learning.corpus_path);
        if (!cp.empty() && cp.string().front() != '~') {
            _corpusPathExpanded  = cp.string();
            _profilePathExpanded = (cp.parent_path() / "style_profile.json").string();
            _embeddingIndexPath  = (cp.parent_path() / "corpus_embeddings.bin").string();
        }
    }

    applyPrompt();
    _inference.update_config(_cfg.inference);
    dbg("[reload] done");
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void XTypeEngine::activate(const fcitx::InputMethodEntry &,
                           fcitx::InputContextEvent &event) {
    auto *ic = event.inputContext();
    const std::string &prog = ic->program();
    dbg("activate prog=%s _lastProg=%s", prog.c_str(), _lastProg.c_str());
    bool appChanged = (prog != _lastProg);
    if (appChanged)
        resetState(ic);       // app changed: full reset including _ctx
    else
        resetInferenceOnly(); // same app cycling (Zen GTK4 pattern): preserve _ctx
    _lastProg = prog;
    if (appChanged)
        applyPrompt();        // re-apply prompt addendum for the new app
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
    bool userTypedThisSession = !_userTypedSinceLastTerminator.empty();
    if (_corpus &&
        static_cast<int>(_userTypedSinceLastTerminator.size()) >=
            _cfg.learning.min_sentence_chars) {
        harvestSentence(ic->program());
    } else {
        _userTypedSinceLastTerminator.clear();
    }
    _ctx.dismiss();
    // Any real typing → clear _ctx so next session starts fresh.
    // Pure focus bounces (no chars typed) preserve _ctx for GTK4 rapid cycling.
    if (userTypedThisSession) _ctx.reset();
    resetInferenceOnly(); // cancel debounce + inference
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

    // Hard-blocked apps (system auth dialogs, password managers) pass through completely —
    // no corpus collection, no inference, no context buffering.
    if (CorpusCollector::isHardBlocked(ic->program())) return;
    // User-configured blocklist.
    if (isBlocked(ic->program())) return;

    // Per-app override: if enabled is explicitly false, pass through.
    if (auto* ov = currentAppOverride(); ov && ov->enabled.has_value() && !*ov->enabled)
        return;

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
        {
            std::vector<float> lastEmbed;
            { std::lock_guard<std::mutex> lk(_embedMutex); lastEmbed = std::move(_lastQueryEmbedding); _lastQueryEmbedding.clear(); }
            if (!lastEmbed.empty()) {
                if (_acceptEmbedRing.size() >= kAcceptRingSize) _acceptEmbedRing.pop_front();
                _acceptEmbedRing.push_back(std::move(lastEmbed));
            }
        }
        ++_metrics.suggestions_accepted;
        _metrics.chars_accepted += committed.size();
        _recent.set_last_accepted();
        writeRecentEvents();
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

    if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
        if (hasSuggestion &&
            _cfg.behaviour.accept_full_key == AcceptKey::Enter) {
            std::string committed = _ctx.acceptAll();
            ic->commitString(committed);
            updatePreedit(ic);
            {
                std::vector<float> lastEmbed;
                { std::lock_guard<std::mutex> lk(_embedMutex); lastEmbed = std::move(_lastQueryEmbedding); _lastQueryEmbedding.clear(); }
                if (!lastEmbed.empty()) {
                    if (_acceptEmbedRing.size() >= kAcceptRingSize) _acceptEmbedRing.pop_front();
                    _acceptEmbedRing.push_back(std::move(lastEmbed));
                }
            }
            ++_metrics.suggestions_accepted;
            _metrics.chars_accepted += committed.size();
            _recent.set_last_accepted();
            writeRecentEvents();
            event.filterAndAccept();
            return;
        }
        // Default: dismiss and pass through.
        if (hasSuggestion) { invalidate(); updatePreedit(ic); }
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

    // Right arrow — accept next word when configured and suggestion active.
    if (sym == FcitxKey_Right &&
        _cfg.behaviour.accept_full_key == AcceptKey::Right &&
        hasSuggestion) {
        std::string committed = _cfg.behaviour.partial_accept
                                ? _ctx.acceptNextWord()
                                : _ctx.acceptAll();
        ic->commitString(committed);
        updatePreedit(ic);
        {
            std::vector<float> lastEmbed;
            { std::lock_guard<std::mutex> lk(_embedMutex); lastEmbed = std::move(_lastQueryEmbedding); _lastQueryEmbedding.clear(); }
            if (!lastEmbed.empty()) {
                if (_acceptEmbedRing.size() >= kAcceptRingSize) _acceptEmbedRing.pop_front();
                _acceptEmbedRing.push_back(std::move(lastEmbed));
            }
        }
        ++_metrics.suggestions_accepted;
        _metrics.chars_accepted += committed.size();
        _recent.set_last_accepted();
        writeRecentEvents();
        event.filterAndAccept();
        return;
    }

    // Printable ASCII (space through ~). Pass key through to app, update buffer,
    // arm debounce timer for inference.
    if (sym >= FcitxKey_space && sym <= FcitxKey_asciitilde) {
        dbg("key prog=%s ctx_len=%zu", ic->program().c_str(), _ctx.contextText().size());
        invalidate();
        updatePreedit(ic);
        const char ch = static_cast<char>(sym);
        _ctx.appendChar(ch);

        // User-typed-only buffer for corpus harvest (excludes AI accept paths).
        // Rolling strategy: on overflow, erase through the next sentence terminator
        // so the remainder starts at a clean sentence boundary.
        if (_userTypedSinceLastTerminator.size() >= kUserBufCap) {
            auto& buf = _userTypedSinceLastTerminator;
            size_t pos = buf.find_first_of(".!?", kUserBufCap / 2);
            if (pos != std::string::npos && pos + 1 < buf.size())
                buf.erase(0, pos + 1);
            else
                buf.clear();
        }
        _userTypedSinceLastTerminator.push_back(ch);
        if (ch == '.' || ch == '!' || ch == '?')
            harvestSentence(ic->program());

        _debounceTimer.reset();
        int debounceMs = _cfg.inference.debounce_ms;
        if (auto* ov = currentAppOverride(); ov && ov->debounce_ms.has_value())
            debounceMs = *ov->debounce_ms;
        uint64_t fireUs = fcitx::now(CLOCK_MONOTONIC) +
                          static_cast<uint64_t>(debounceMs) * 1000;
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

// ── Embedding retrieval (L4) ──────────────────────────────────────────────────

static std::vector<std::string> scoreAndRankExemplars(
    const std::vector<EmbeddingEntry>& index,
    const std::vector<float>& queryVec,
    const std::deque<std::vector<float>>& acceptRing,
    size_t maxCount)
{
    if (index.empty() || queryVec.empty() || maxCount == 0) return {};
    std::vector<std::pair<float, size_t>> scores;
    scores.reserve(index.size());
    for (size_t i = 0; i < index.size(); ++i) {
        const auto& entry = index[i];
        float dot = 0.f;
        size_t dim = std::min(queryVec.size(), entry.embedding.size());
        for (size_t d = 0; d < dim; ++d) dot += queryVec[d] * entry.embedding[d];
        float acceptBoost = 0.f;
        for (const auto& av : acceptRing) {
            float ab = 0.f;
            size_t adim = std::min(av.size(), entry.embedding.size());
            for (size_t d = 0; d < adim; ++d) ab += av[d] * entry.embedding[d];
            acceptBoost = std::max(acceptBoost, ab);
        }
        scores.emplace_back(0.7f * dot + 0.3f * acceptBoost, i);
    }
    size_t n = std::min(maxCount, scores.size());
    std::partial_sort(scores.begin(), scores.begin() + n, scores.end(),
                      [](const auto& a, const auto& b){ return a.first > b.first; });
    std::vector<std::string> results;
    results.reserve(n);
    for (size_t i = 0; i < n; ++i)
        results.push_back(index[scores[i].second].text);
    return results;
}

std::vector<std::string> XTypeEngine::retrieveExemplars(
    const std::string& contextText,
    size_t maxCount,
    const std::deque<std::vector<float>>& acceptRingSnap)
{
    if (maxCount == 0 || !_embedClient) return {};
    auto queryVec = _embedClient->embed(contextText);
    std::shared_ptr<std::vector<EmbeddingEntry>> index;
    {
        std::lock_guard<std::mutex> lk(_embedMutex);
        _lastQueryEmbedding = queryVec;  // may be empty if embed failed
        index = _embeddingIndex;
    }
    if (queryVec.empty() || !index || index->empty()) return {};
    return scoreAndRankExemplars(*index, queryVec, acceptRingSnap, maxCount);
}

// ── Inference ─────────────────────────────────────────────────────────────────

static std::string_view sentenceAligned(std::string_view s) {
    for (size_t i = 0; i + 1 < s.size(); ++i) {
        char c = s[i];
        if ((c == '.' || c == '!' || c == '?') && s[i + 1] == ' ') {
            size_t start = i + 2;
            while (start < s.size() && s[start] == ' ') ++start;
            if (start < s.size()) return s.substr(start);
        }
    }
    return s;
}

void XTypeEngine::requestInference(
    fcitx::TrackableObjectReference<fcitx::InputContext> icRef,
    fcitx::InputContext *icPtr)
{
    if (_profile) _profile->setCurrentApp(icPtr->program());

    auto ctx = _ctx.contextText();
    if (static_cast<int>(ctx.size()) < _cfg.inference.min_context_chars)
        return;
    if (static_cast<int>(ctx.size()) > _cfg.inference.context_window)
        ctx = ctx.substr(ctx.size() -
                         static_cast<size_t>(_cfg.inference.context_window));
    {
        auto aligned = sentenceAligned(ctx);
        ctx = std::string(aligned);
    }

    ++_gen;
    ++_metrics.suggestions_generated;
    const uint64_t myGen = _gen;
    const auto     startUs = fcitx::now(CLOCK_MONOTONIC);

    InferenceConfig reqCfg = _cfg.inference;
    // Per-app override wins; otherwise calibrate dynamically.
    const AppOverride* ov = currentAppOverride();
    if (ov && ov->num_predict.has_value()) {
        reqCfg.num_predict = *ov->num_predict;
    } else if (reqCfg.num_predict == 30) {
        // Only auto-calibrate when the user hasn't changed the default.
        // avgSentenceLen is in chars; divide by 4 for rough token estimate.
        if (_profile && _profile->avgSentenceLen() > 0) {
            int tokens = std::clamp(_profile->avgSentenceLen() / 4, 15, 60);
            reqCfg.num_predict = tokens;
        } else {
            // Depth-based fallback: more context → longer expected completion.
            int ctxLen = static_cast<int>(ctx.size());
            reqCfg.num_predict = ctxLen < 50 ? 15 : ctxLen < 150 ? 25 : 40;
        }
    }
    if (ov && ov->model.has_value() && !ov->model->empty())
        reqCfg.model = *ov->model;

    // Build per-request embedding factory if the index is ready (L4).
    // All state is snapshotted by value on the main thread; factory runs on worker thread.
    std::function<std::string()> promptFactory;
    {
        size_t embedTargetCount = 0;
        std::shared_ptr<std::vector<EmbeddingEntry>> indexSnap;
        {
            std::lock_guard<std::mutex> lk(_embedMutex);
            indexSnap = _embeddingIndex;
        }
        if (_cfg.learning.enabled && _cfg.learning.include_examples_in_prompt
            && _embedClient && indexSnap && !indexSnap->empty()) {
            embedTargetCount = static_cast<size_t>(std::ceil(
                static_cast<double>(StyleProfile::kMaxEmbedExemplars) *
                std::clamp(_cfg.learning.voice_strength, 0, 100) / 100.0));
        }
        if (embedTargetCount > 0) {
            auto acceptRingSnap = _acceptEmbedRing;  // copy on main thread

            std::vector<std::string> staticExemplars;
            if (_profile && !_profile->exemplars().empty()) {
                const auto& all = _profile->exemplars();
                size_t useCount = std::min(embedTargetCount, all.size());
                staticExemplars = std::vector<std::string>(all.begin(), all.begin() + useCount);
            }

            // Build base PromptInputs; profile=nullptr avoids UAF if profile refreshes
            // between factory creation (main thread) and factory execution (worker thread).
            PromptInputs baseIn;
            baseIn.base            = std::string(InferenceClient::base_system_prompt());
            baseIn.profile         = nullptr;
            baseIn.includeExamples = _cfg.learning.include_examples_in_prompt;
            baseIn.userDescription = _cfg.user_prompt.description;
            baseIn.avoidPhrases    = _cfg.user_prompt.avoid_phrases;
            baseIn.budgetChars     = kPromptBudget;
            if (_profile && !_profile->commonOpeners().empty())
                baseIn.commonOpeners = _profile->commonOpeners();
            if (!_cfg.user_prompt.tone.empty() && _cfg.user_prompt.tone != "default") {
                static const std::unordered_map<std::string, const char*> kToneMap = {
                    {"technical",    " Prefer precise technical terminology."},
                    {"casual",       " Use a relaxed, conversational tone."},
                    {"professional", " Use formal, professional language."},
                    {"concise",      " Be brief and direct."},
                };
                auto it = kToneMap.find(_cfg.user_prompt.tone);
                if (it != kToneMap.end()) baseIn.base += it->second;
            }
            if (!_lastProg.empty()) {
                if (auto* appOv = currentAppOverride();
                    appOv && appOv->prompt_addendum.has_value() && !appOv->prompt_addendum->empty())
                    baseIn.base += "\n" + *appOv->prompt_addendum;
            }
            static constexpr size_t kMaxDescLen = 500;
            if (baseIn.userDescription.size() > kMaxDescLen) {
                size_t cut = baseIn.userDescription.rfind(' ', kMaxDescLen);
                baseIn.userDescription.resize(cut != std::string::npos ? cut : kMaxDescLen);
            }

            std::string ctxForEmbed = ctx;  // copy before ctx is moved into request()
            promptFactory = [this,
                             ctxForEmbed     = std::move(ctxForEmbed),
                             embedTargetCount,
                             acceptRingSnap  = std::move(acceptRingSnap),
                             staticExemplars = std::move(staticExemplars),
                             baseIn          = std::move(baseIn)]() mutable -> std::string {
                auto exemplars = retrieveExemplars(ctxForEmbed, embedTargetCount, acceptRingSnap);
                PromptInputs in = std::move(baseIn);
                if (!exemplars.empty())
                    in.exemplarsOverride = std::move(exemplars);
                else if (!staticExemplars.empty())
                    in.exemplarsOverride = staticExemplars;
                else
                    in.includeExamples = false;
                return buildSystemPrompt(in);
            };
        }
    }

    dbg("requestInference ctx_len=%zu", ctx.size());
    auto snapPtr = std::make_shared<std::string>(ctx);  // snapshot; shared ownership
    _inference.request(
        std::move(ctx),
        std::move(reqCfg),
        [this, myGen, icRef, icPtr, snapPtr](std::string token) {
            if (!icRef.isValid()) { dbg("on_token: icRef invalid — dropping"); return; }
            _instance->eventDispatcher().scheduleWithContext(
                icRef,
                [this, myGen, tok = std::move(token), icPtr, snapPtr]() {
                    if (myGen != _gen) return;
                    std::string current =
                        _ctx.hasSuggestion() ? *_ctx.suggestion() : "";
                    std::string accumulated = current + tok;
                    _ctx.setSuggestion(accumulated);

                    // Suppress preedit while accumulated suggestion is still an
                    // echo of the typed context tail (min 4 chars, max 40 chars).
                    constexpr size_t kEchoMin = 4;
                    constexpr size_t kEchoMax = 40;
                    bool isTailEcho = false;
                    const std::string& snap = *snapPtr;
                    if (!accumulated.empty() && !snap.empty()) {
                        size_t checkLen = std::min({accumulated.size(),
                                                   snap.size(), kEchoMax});
                        if (checkLen >= kEchoMin) {
                            isTailEcho =
                                snap.compare(snap.size() - checkLen,
                                             checkLen,
                                             accumulated, 0, checkLen) == 0;
                        }
                    }
                    if (!isTailEcho) {
                        updatePreedit(icPtr);
                    }
                });
        },
        [this, myGen, icRef, icPtr, startUs]() {
            dbg("inference done, myGen=%llu curGen=%llu", (unsigned long long)myGen, (unsigned long long)_gen);
            _instance->eventDispatcher().scheduleWithContext(
                icRef,
                [this, myGen, icPtr, startUs]() {
                    if (myGen != _gen) return;
                    if (_ctx.hasSuggestion()) {
                        std::string s = *_ctx.suggestion();
                        // Strip trailing whitespace.
                        while (!s.empty() &&
                               std::isspace(static_cast<unsigned char>(s.back())))
                            s.pop_back();
                        // Truncate to at most max_sentences sentence-ending marks.
                        if (_cfg.inference.max_sentences > 0 && !s.empty()) {
                            int found = 0;
                            for (size_t i = 0; i < s.size(); ++i) {
                                char c = s[i];
                                if (c == '.' || c == '!' || c == '?') {
                                    ++found;
                                    if (found >= _cfg.inference.max_sentences) {
                                        s.resize(i + 1);
                                        break;
                                    }
                                }
                            }
                        }
                        const std::string ctxText = _ctx.contextText();
                        // Strip tail-of-context echo: model repeats recently typed text.
                        constexpr size_t kMaxCheck = 80;
                        size_t check = std::min({s.size(), ctxText.size(), kMaxCheck});
                        for (size_t len = check; len >= 4; --len) {
                            if (ctxText.compare(ctxText.size() - len, len, s, 0, len) == 0) {
                                s.erase(0, len);
                                break;
                            }
                        }
                        // Strip head-of-context echo: model reproduces the document
                        // from the beginning instead of completing at [CURSOR].
                        // Case-insensitive match on the first 12 chars is enough to
                        // identify this pattern without false-positives on short words.
                        constexpr size_t kHeadCheck = 12;
                        if (!s.empty() && s.size() >= kHeadCheck && ctxText.size() >= kHeadCheck) {
                            auto lower = [](std::string t) {
                                std::transform(t.begin(), t.end(), t.begin(),
                                               [](unsigned char c){ return std::tolower(c); });
                                return t;
                            };
                            if (lower(s.substr(0, kHeadCheck)) == lower(ctxText.substr(0, kHeadCheck)))
                                s.clear();
                        }
                        _ctx.setSuggestion(std::move(s));
                    }
                    updatePreedit(icPtr);
                    const auto endUs = fcitx::now(CLOCK_MONOTONIC);
                    recordLatency(static_cast<int>((endUs - startUs) / 1000));
                    if (_ctx.hasSuggestion()) {
                        RecentEvent ev;
                        const std::string ctxText = _ctx.contextText();
                        ev.typed      = ctxText.substr(
                                           static_cast<size_t>(std::max(0, (int)ctxText.size() - 40)));
                        ev.ghost      = _ctx.suggestion() ? *_ctx.suggestion() : "";
                        ev.app        = icPtr->program();
                        ev.accepted   = false;
                        ev.latency_ms = static_cast<int>((endUs - startUs) / 1000);
                        ev.ts         = std::time(nullptr);
                        _recent.push(ev);
                        writeRecentEvents();
                    }
                });
        },
        [](std::string err) { dbg("inference error: %s", err.c_str()); },
        std::move(promptFactory)
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

    for (const auto &entry : _cfg.behaviour.blocklist_apps) {
        if (entry.empty()) continue;
        // Match only complete components; separators are '.' and '_' (reverse-domain-name style).
        // Prevents "konsole" from matching "konsoleboard" or "kate" matching "kate-beta".
        auto pos = prog_lower.find(entry);
        while (pos != std::string::npos) {
            auto after = pos + entry.size();
            bool startOk = (pos == 0) || prog_lower[pos - 1] == '.' || prog_lower[pos - 1] == '_';
            bool endOk   = (after == prog_lower.size())
                         || prog_lower[after] == '.' || prog_lower[after] == '_';
            if (startOk && endOk) return true;
            pos = prog_lower.find(entry, pos + 1);
        }
    }
    return false;
}

const AppOverride* XTypeEngine::currentAppOverride() const {
    auto it = _cfg.apps.find(_lastProg);
    if (it != _cfg.apps.end()) return &it->second;
    std::string lower = _lastProg;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    it = _cfg.apps.find(lower);
    if (it != _cfg.apps.end()) return &it->second;
    return nullptr;
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
    _corpus->record(std::move(s), program);
}

// ── Observability (E2) ────────────────────────────────────────────────────────

void XTypeEngine::recordLatency(int ms) {
    if (_latencyWindow.size() >= kLatencyWindowSize)
        _latencyWindow.erase(_latencyWindow.begin());
    _latencyWindow.push_back(ms);

    std::vector<int> sorted = _latencyWindow;
    std::sort(sorted.begin(), sorted.end());
    size_t n = sorted.size();
    _metrics.latency_p50_ms.store(sorted[n / 2]);
    _metrics.latency_p95_ms.store(sorted[std::min(n - 1, static_cast<size_t>(n * 0.95))]);

    writeMetrics();
}

void XTypeEngine::writeMetrics() {
    std::time_t now = std::time(nullptr);
    if (now == _metricsLastWrite) return;
    _metricsLastWrite = now;

    auto dir = path_utils::expandTilde("~/.local/share/xtype");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto tmp = (dir / "metrics.json.tmp").string();
    auto dst = (dir / "metrics.json").string();

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "{\"latency_p50_ms\":%d,\"latency_p95_ms\":%d,"
        "\"suggestions_generated\":%" PRIu64 ","
        "\"suggestions_accepted\":%" PRIu64 ","
        "\"chars_accepted\":%" PRIu64 ","
        "\"updated_at\":%" PRId64 "}\n",
        _metrics.latency_p50_ms.load(),
        _metrics.latency_p95_ms.load(),
        static_cast<uint64_t>(_metrics.suggestions_generated.load()),
        static_cast<uint64_t>(_metrics.suggestions_accepted.load()),
        static_cast<uint64_t>(_metrics.chars_accepted.load()),
        static_cast<int64_t>(now));

    if (FILE* f = std::fopen(tmp.c_str(), "w")) {
        std::fputs(buf, f);
        std::fclose(f);
        std::rename(tmp.c_str(), dst.c_str());
    }
}

void XTypeEngine::writeRecentEvents() {
    std::time_t now = std::time(nullptr);
    if (now == _eventsLastWrite) return;
    _eventsLastWrite = now;

    auto dir = path_utils::expandTilde("~/.local/share/xtype");
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    auto tmp = (dir / "recent_events.json.tmp").string();
    auto dst = (dir / "recent_events.json").string();

    auto esc = [](std::string_view s) {
        std::string out;
        for (char c : s) {
            if (c == '"')       out += "\\\"";
            else if (c == '\\') out += "\\\\";
            else if (c == '\n') out += "\\n";
            else                out += c;
        }
        return out;
    };

    auto snap = _recent.snapshot();
    std::string json = "[\n";
    bool first = true;
    for (const auto& ev : snap) {
        if (!first) json += ",\n";
        first = false;
        char entry[1024];
        std::snprintf(entry, sizeof(entry),
            "  {\"typed\":\"%s\",\"ghost\":\"%s\",\"app\":\"%s\","
            "\"accepted\":%s,\"latency_ms\":%d,\"ts\":%" PRId64 "}",
            esc(ev.typed).c_str(),
            esc(ev.ghost).c_str(),
            esc(ev.app).c_str(),
            ev.accepted ? "true" : "false",
            ev.latency_ms,
            static_cast<int64_t>(ev.ts));
        json += entry;
    }
    json += "\n]\n";

    if (FILE* f = std::fopen(tmp.c_str(), "w")) {
        std::fputs(json.c_str(), f);
        std::fclose(f);
        std::rename(tmp.c_str(), dst.c_str());
    }
}

// ── Personalization (Session 17) ──────────────────────────────────────────────

void XTypeEngine::applyPrompt() {
    PromptInputs in;
    in.base            = std::string(InferenceClient::base_system_prompt());
    in.profile         = _profile.get();
    in.includeExamples = _cfg.learning.include_examples_in_prompt;
    in.budgetChars     = kPromptBudget;

    in.userDescription = _cfg.user_prompt.description;
    in.avoidPhrases    = _cfg.user_prompt.avoid_phrases;
    if (_profile && !_profile->commonOpeners().empty())
        in.commonOpeners = _profile->commonOpeners();

    // Cap user description at 500 chars to keep within prompt budget.
    // The budget enforcer in buildSystemPrompt truncates at character level;
    // truncating at a higher level here ensures the description ends on a
    // word boundary.
    static constexpr size_t kMaxDescLen = 500;
    if (in.userDescription.size() > kMaxDescLen) {
        size_t cut = in.userDescription.rfind(' ', kMaxDescLen);
        in.userDescription.resize(cut != std::string::npos ? cut : kMaxDescLen);
        log_warn("user description truncated to fit prompt budget");
    }

    // voice_strength: 0 = no exemplars, 100 = kMaxEmbedExemplars, 1–99 = proportional.
    if (_profile && !_profile->exemplars().empty() && in.includeExamples) {
        const auto& all = _profile->exemplars();
        size_t targetCount = static_cast<size_t>(
            std::ceil(static_cast<double>(StyleProfile::kMaxEmbedExemplars) *
                      std::clamp(_cfg.learning.voice_strength, 0, 100) / 100.0));
        if (targetCount == 0) {
            in.includeExamples = false;
        } else {
            size_t useCount = std::min(targetCount, all.size());
            if (useCount < all.size()) {
                in.exemplarsOverride =
                    std::vector<std::string>(all.begin(), all.begin() + useCount);
            }
            // useCount == all.size(): use default path (exemplarsOverride empty)
        }
    }

    // Tone: append a short style modifier when a non-default tone is set.
    if (!_cfg.user_prompt.tone.empty() && _cfg.user_prompt.tone != "default") {
        static const std::unordered_map<std::string, const char*> kToneMap = {
            {"technical",    " Prefer precise technical terminology."},
            {"casual",       " Use a relaxed, conversational tone."},
            {"professional", " Use formal, professional language."},
            {"concise",      " Be brief and direct."},
        };
        auto it = kToneMap.find(_cfg.user_prompt.tone);
        if (it != kToneMap.end())
            in.base += it->second;
    }

    // Per-app prompt addendum — only applied when an active app is known.
    if (!_lastProg.empty()) {
        if (auto* ov = currentAppOverride();
            ov && ov->prompt_addendum.has_value() && !ov->prompt_addendum->empty()) {
            in.base += "\n" + *ov->prompt_addendum;
        }
    }

    bool truncated = false;
    auto prompt = buildSystemPrompt(in, &truncated);
    if (truncated)
        log_warn("system prompt truncated to fit 2000-char budget");
    size_t exCount = _profile ? _profile->exemplars().size() : (size_t)0;
    dbg("[prompt] set %zu chars exemplars=%zu desc_len=%zu avoid=%zu truncated=%d",
        prompt.size(), exCount,
        in.userDescription.size(),
        in.avoidPhrases.size(),
        truncated ? 1 : 0);
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
    std::string indexPath   = _embeddingIndexPath;
    std::string ollamaHost  = _cfg.inference.ollama_host;
    std::string currentApp  = _lastProg;  // capture for app-weighted exemplar selection

    _profileWorker.emplace([this, corpusPath, profilePath, indexPath, ollamaHost, currentApp]() {
        StyleProfile sp = StyleProfile::deserialize(profilePath);
        bool stale = StyleProfile::isStale(corpusPath, profilePath);
        bool needRebuild = sp.exemplars().empty() || stale;
        std::string reason;
        if (sp.exemplars().empty())               reason = "missing-or-empty";
        else if (stale)                           reason = "stale";
        else                                      reason = "none";

        if (needRebuild) {
            sp = StyleProfile{};
            sp.setCurrentApp(currentApp);
            sp.loadFromCorpus(corpusPath);
            if (!sp.exemplars().empty()) {
                sp.serialize(profilePath);
                _instance->eventDispatcher().schedule(
                    [path = profilePath]{ dbg("[profile] serialize: %s", path.c_str()); });
            }
        }

        // Build embedding index on the background thread (blocking HTTP OK here).
        if (!indexPath.empty() && !ollamaHost.empty()) {
            sp.buildEmbeddingIndex(corpusPath, indexPath, ollamaHost);
            // Hot-swap the index so per-request retrieval picks it up immediately.
            auto newIndex = std::make_shared<std::vector<EmbeddingEntry>>(
                readEmbeddingIndex(indexPath));
            size_t idxCount = newIndex->size();
            {
                std::lock_guard<std::mutex> lk(_embedMutex);
                _embeddingIndex = newIndex;
            }
            _instance->eventDispatcher().schedule(
                [idxCount]{ dbg("[embed-index] loaded %zu entries", idxCount); });
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
