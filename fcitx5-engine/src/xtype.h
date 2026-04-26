#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/trackableobject.h>

#include "config.h"
#include "context_buffer.h"
#include "corpus_collector.h"
#include "inference_client.h"
#include "style_profile.h"

class XTypeEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit XTypeEngine(fcitx::AddonManager *manager);
    ~XTypeEngine() override;

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

    // Profile lifecycle. _profile is read+written only on the main thread
    // (mutated from event-dispatcher callbacks marshaled from the worker).
    void kickProfileLoad();
    void armProfileRefreshTimer();
    void applyPrompt();

    fcitx::Instance                         *_instance;
    XTypeConfig                              _cfg;
    ContextBuffer                            _ctx;
    InferenceClient                          _inference;
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

    static constexpr size_t kUserBufCap        = 2048;
    static constexpr int    kProfileRefreshSec = 300;   // 5 min
    static constexpr size_t kPromptBudget      = 2000;
};
