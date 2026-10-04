#ifndef F264_PROFILING_H_
#define F264_PROFILING_H_

#include <stdint.h>

#ifdef __cplusplus
#include <chrono>
#include <atomic>

struct F264ProfileStats {
    std::atomic<uint64_t> mc_ns{0};
    std::atomic<uint64_t> transform_ns{0};
    std::atomic<uint64_t> deblock_ns{0};
    std::atomic<uint64_t> cabac_ns{0};
    std::atomic<uint64_t> total_ns{0};

    void reset() {
        mc_ns = 0;
        transform_ns = 0;
        deblock_ns = 0;
        cabac_ns = 0;
        total_ns = 0;
    }
};

extern F264ProfileStats g_profile_stats;

struct ScopedTimer {
    std::atomic<uint64_t> &target;
    std::chrono::high_resolution_clock::time_point start;
    ScopedTimer(std::atomic<uint64_t> &t) : target(t), start(std::chrono::high_resolution_clock::now()) {}
    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        target.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), std::memory_order_relaxed);
    }
};
#endif

#ifdef __cplusplus
extern "C" {
#endif
void f264_profile_report(void);
void f264_profile_reset(void);
#ifdef __cplusplus
}
#endif

#endif // F264_PROFILING_H_
