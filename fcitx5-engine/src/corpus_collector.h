#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
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

    // True if HOME could not be resolved or path is unusable; record() is a no-op.
    bool disabled() const { return _disabled; }

private:
    void run();
    void flushLocked(std::deque<std::string>& drained);
    void rotateIfNeeded();
    bool acceptable(const std::string& text) const;

    LearningConfig          _cfg;
    std::filesystem::path   _path;
    bool                    _disabled{false};

    std::mutex              _mu;
    std::condition_variable _cv;
    std::deque<std::string> _queue;
    std::atomic<bool>       _shutdown{false};
    std::thread             _thread;

    static constexpr size_t kQueueCap     = 1000;
    static constexpr size_t kEagerFlushAt = 100;
};
