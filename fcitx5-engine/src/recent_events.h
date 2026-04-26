#pragma once

#include <algorithm>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

struct RecentEvent {
    std::string typed;
    std::string ghost;
    std::string app;
    bool        accepted{false};
    int         latency_ms{0};
    std::time_t ts{0};
};

class RecentEventsRing {
public:
    void push(RecentEvent e) {
        std::lock_guard<std::mutex> lk(_mu);
        if (_buf.size() < kCapacity)
            _buf.push_back(std::move(e));
        else
            _buf[_head] = std::move(e);
        _head = (_head + 1) % kCapacity;
        if (_count < kCapacity) ++_count;
    }

    // Returns events newest-first.
    std::vector<RecentEvent> snapshot() const {
        std::lock_guard<std::mutex> lk(_mu);
        std::vector<RecentEvent> out;
        out.reserve(_count);
        size_t start = _count < kCapacity ? 0 : _head;
        for (size_t i = 0; i < _count; ++i)
            out.push_back(_buf[(start + i) % kCapacity]);
        std::reverse(out.begin(), out.end());
        return out;
    }

private:
    static constexpr size_t kCapacity = 32;
    mutable std::mutex       _mu;
    std::vector<RecentEvent> _buf;
    size_t                   _head{0};
    size_t                   _count{0};
};
