#pragma once

#include <atomic>
#include <cstdint>
#include <ctime>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/trackableobject.h>

#include "config.h"
#include "context_buffer.h"
#include "corpus_collector.h"
#include "embed_client.h"
#include "engine_metrics.h"
#include "inference_client.h"
#include "phrase_blocklist.h"
#include "recent_events.h"
#include "style_profile.h"

class XTypeEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit XTypeEngine(fcitx::AddonManager *manager);
    ~XTypeEngine() override;

    const EngineMetrics&      metrics()      const noexcept { return _metrics; }
    const RecentEventsRing&   recentEvents() const noexcept { return _recent; }
    const XTypeConfig&        config()       const noexcept { return _cfg; }

    void reloadConfig() override;

    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &event) override;
    void activate(const fcitx::InputMethodEntry &entry,
                  fcitx::InputContextEvent &event) override;
    void deactivate(const fcitx::InputMethodEntry &entry,
                    fcitx::InputContextEvent &event) override;
    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

private:
    void requestInference(fcitx::TrackableObjectReference<fcitx::InputContext> icRef,
                          fcitx::InputContext *icPtr);
    void updatePreedit(fcitx::InputContext *ic);
    void clearPreedit(fcitx::InputContext *ic);
    void resetState(fcitx::InputContext *ic);
    void resetInferenceOnly();
    void invalidate();
    bool isBlocked(const std::string &program) const;
    void harvestSentence(const std::string &program);
    const AppOverride* currentAppOverride() const;  // nullptr if no override

    // Profile lifecycle. _profile is read+written only on the main thread
    // (mutated from event-dispatcher callbacks marshaled from the worker).
    void kickProfileLoad();
    void armProfileRefreshTimer();
    void applyPrompt();

    // Embedding retrieval (L4). Called on InferenceClient's worker thread.
    // acceptRingSnap is a copy taken on the main thread before the factory was created.
    std::vector<std::string> retrieveExemplars(const std::string& contextText,
                                               size_t maxCount,
                                               const std::deque<std::vector<float>>& acceptRingSnap);

    fcitx::Instance                         *_instance;
    XTypeConfig                              _cfg;
    ContextBuffer                            _ctx;

    // L4 embedding members — declared before _inference so they outlive its worker thread.
    std::unique_ptr<OllamaEmbedClient>               _embedClient;
    std::shared_ptr<std::vector<EmbeddingEntry>>     _embeddingIndex;      // guarded by _embedMutex
    static constexpr size_t                          kAcceptRingSize = 20;
    std::deque<std::vector<float>>                   _acceptEmbedRing;     // main-thread only
    std::vector<float>                               _lastQueryEmbedding;  // guarded by _embedMutex
    mutable std::mutex                               _embedMutex;          // guards _embeddingIndex + _lastQueryEmbedding

    InferenceClient                          _inference;
    EngineMetrics                            _metrics;
    RecentEventsRing                         _recent;
    PhraseBlocklist                          _phraseBlock;
    std::unique_ptr<CorpusCollector>         _corpus;
    std::string                              _userTypedSinceLastTerminator;
    std::unique_ptr<fcitx::EventSourceTime>  _debounceTimer;
    uint64_t                                 _gen{0};
    std::string                              _lastProg;

    // Personalization state (Session 17). Populated only when learning enabled.
    std::unique_ptr<StyleProfile>            _profile;          // main-thread only
    std::unique_ptr<fcitx::EventSourceTime>  _profileRefreshTimer;
    std::optional<std::thread>               _profileWorker;    // owning; joined in dtor
    std::atomic<bool>                        _profileLoading{false};
    std::string                              _corpusPathExpanded;
    std::string                              _profilePathExpanded;
    std::string                              _embeddingIndexPath;

    // XTYPE_PROFILE_REFRESH_SEC overrides the profile refresh interval (clamped >= 30).
    int                                      _profileRefreshSec{300};

    // Latency tracking: rolling window of last 32 inference durations (ms).
    static constexpr size_t kLatencyWindowSize = 32;
    std::vector<int>         _latencyWindow;
    std::time_t              _metricsLastWrite{0};
    std::time_t              _eventsLastWrite{0};

    void recordLatency(int ms);
    void writeMetrics();
    void writeRecentEvents();

    static constexpr size_t kUserBufCap        = 2048;
    static constexpr int    kProfileRefreshSec = 300;   // 5 min default
    static constexpr size_t kPromptBudget      = 2000;
};
