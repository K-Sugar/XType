#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

#include "config.h"

class InferenceClient {
public:
    using TokenCb = std::function<void(std::string)>;
    using DoneCb  = std::function<void()>;
    using ErrCb   = std::function<void(std::string)>;

    explicit InferenceClient(InferenceConfig cfg = {});
    ~InferenceClient();

    InferenceClient(const InferenceClient&)            = delete;
    InferenceClient& operator=(const InferenceClient&) = delete;

    // Thread-safe. Each new call cancels any in-flight request.
    void request(std::string context, TokenCb on_token, DoneCb on_done, ErrCb on_error);
    void cancel();
    bool health_check();  // blocking GET /api/tags

    // Replace the system prompt sent on subsequent requests. Thread-safe.
    // No-op if equal to the current value.
    void set_system_prompt(std::string s);

    // Update inference config (model, temperature, etc.) for subsequent requests. Thread-safe.
    void update_config(const InferenceConfig& cfg);

    // The canonical base instruction this client uses by default.
    static std::string_view base_system_prompt();

private:
    struct Req {
        std::string context;
        uint64_t    gen{};
        TokenCb     on_token;
        DoneCb      on_done;
        ErrCb       on_error;
    };

    void run();
    void execute(Req& req);

    InferenceConfig          _cfg;
    std::thread              _thread;
    std::mutex               _mutex;          // also guards _system_prompt
    std::condition_variable  _cv;
    std::optional<Req>       _pending;
    std::string              _system_prompt;  // guarded by _mutex
    std::atomic<uint64_t>    _gen{0};
    std::atomic<bool>        _shutdown{false};
};
