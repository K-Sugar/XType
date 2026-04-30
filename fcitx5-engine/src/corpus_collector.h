#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "config.h"

class CorpusCollector {
public:
    explicit CorpusCollector(LearningConfig cfg);
    ~CorpusCollector();

    CorpusCollector(const CorpusCollector&)            = delete;
    CorpusCollector& operator=(const CorpusCollector&) = delete;
    CorpusCollector(CorpusCollector&&)                 = delete;
    CorpusCollector& operator=(CorpusCollector&&)      = delete;

    // Thread-safe, non-blocking. Filters at record-time (length, code-shape,
    // whitespace-only, no-alpha). Drops oldest on queue overflow.
    void record(std::string text);

    // Hard-coded sensitive-app guard. Case-insensitive substring match against
    // a fixed list of password / secret managers. Always blocked, regardless of
    // user config.
    static bool isHardBlocked(std::string_view program);

    // Prune entries older than `days` (based on companion timestamps file).
    // No-op if days == 0. Called from the flush thread.
    static void pruneOldEntries(const std::filesystem::path& corpus_path, int days);

    // True if HOME could not be resolved or path is unusable; record() is a no-op.
    bool disabled() const { return _disabled; }

    // Optional log sink. Invoked from the BACKGROUND flush thread, so the
    // engine-side wiring must marshal back to the main thread before touching
    // any non-thread-safe loggers. Set after construction; thread-safe to set
    // exactly once before the collector starts seeing traffic.
    using LogFn = std::function<void(std::string)>;
    void setLogSink(LogFn fn) { _log = std::move(fn); }

private:
    void run();
    void flushLocked(std::deque<std::string>& drained);
    void rotateIfNeeded();
    void extractAndSeedExemplars();
    bool acceptable(const std::string& text) const;

    // Returns true (reject) if the text contains privacy-sensitive patterns:
    // keywords (password, token, …), email addresses, URLs, phone numbers,
    // or runs of ≥ 4 consecutive digits (PINs, credit-card fragments).
    static bool hasPrivateTerm(const std::string& s);

    LearningConfig          _cfg;
    std::filesystem::path   _path;
    bool                    _disabled{false};

    std::mutex              _mu;
    std::condition_variable _cv;
    std::deque<std::string> _queue;
    std::atomic<bool>       _shutdown{false};
    std::thread             _thread;
    LogFn                   _log;

    static constexpr size_t kQueueCap     = 1000;
    static constexpr size_t kEagerFlushAt = 100;
};
