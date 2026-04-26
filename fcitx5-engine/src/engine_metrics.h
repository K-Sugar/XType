#pragma once

#include <atomic>
#include <cstdint>

struct EngineMetrics {
    std::atomic<int>      latency_p50_ms{0};
    std::atomic<int>      latency_p95_ms{0};
    std::atomic<int>      cpu_pct{0};
    std::atomic<long>     ram_mb{0};

    std::atomic<uint64_t> suggestions_generated{0};
    std::atomic<uint64_t> suggestions_accepted{0};
    std::atomic<uint64_t> chars_accepted{0};
};
