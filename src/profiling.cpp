#include "profiling.h"
#include <cstdio>

F264ProfileStats g_profile_stats;

extern "C" {

void f264_profile_reset(void)
{
    g_profile_stats.reset();
}

void f264_profile_report(void)
{
    double mc_ms = g_profile_stats.mc_ns.load() / 1e6;
    double tr_ms = g_profile_stats.transform_ns.load() / 1e6;
    double db_ms = g_profile_stats.deblock_ns.load() / 1e6;
    double cb_ms = g_profile_stats.cabac_ns.load() / 1e6;
    double total_ms = mc_ms + tr_ms + db_ms + cb_ms;

    if (total_ms < 0.001) return;

    std::fprintf(stderr, "\n=======================================================\n");
    std::fprintf(stderr, "           f264dec Performance Hotspot Profile         \n");
    std::fprintf(stderr, "=======================================================\n");
    std::fprintf(stderr, "  Component             Time (ms)     Percentage       \n");
    std::fprintf(stderr, "-------------------------------------------------------\n");
    std::fprintf(stderr, "  Motion Compensation   %8.2f ms       %5.1f %%\n", mc_ms, 100.0 * mc_ms / total_ms);
    std::fprintf(stderr, "  Deblocking Filter     %8.2f ms       %5.1f %%\n", db_ms, 100.0 * db_ms / total_ms);
    std::fprintf(stderr, "  Inverse Transforms    %8.2f ms       %5.1f %%\n", tr_ms, 100.0 * tr_ms / total_ms);
    std::fprintf(stderr, "  CABAC/Entropy Decode  %8.2f ms       %5.1f %%\n", cb_ms, 100.0 * cb_ms / total_ms);
    std::fprintf(stderr, "-------------------------------------------------------\n");
    std::fprintf(stderr, "  Profiled Core Total   %8.2f ms       100.0 %%\n", total_ms);
    std::fprintf(stderr, "=======================================================\n\n");
}

}
