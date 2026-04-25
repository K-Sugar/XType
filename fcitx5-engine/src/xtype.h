#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventloopinterface.h>
#include <fcitx-utils/trackableobject.h>

#include "config.h"
#include "context_buffer.h"
#include "inference_client.h"

class XTypeEngine : public fcitx::InputMethodEngineV2 {
public:
    explicit XTypeEngine(fcitx::AddonManager *manager);
    ~XTypeEngine() override = default;

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
    void invalidate();
    bool isBlocked(const std::string &program) const;

    fcitx::Instance                         *_instance;
    XTypeConfig                              _cfg;
    ContextBuffer                            _ctx;
    InferenceClient                          _inference;
    std::unique_ptr<fcitx::EventSourceTime>  _debounceTimer;
    uint64_t                                 _gen{0};
};
