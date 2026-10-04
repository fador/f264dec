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
    double mc_luma_ms = g_profile_stats.mc_luma_ns.load() / 1e6;
    double mc_chroma_ms = g_profile_stats.mc_chroma_ns.load() / 1e6;
    double mc_pred_ms = g_profile_stats.mc_pred_ns.load() / 1e6;
    double tr_ms = g_profile_stats.transform_ns.load() / 1e6;
    double db_ms = g_profile_stats.deblock_ns.load() / 1e6;
    double db_str_ms = g_profile_stats.db_strength_ns.load() / 1e6;
    double db_flt_ms = g_profile_stats.db_filter_ns.load() / 1e6;
    double cb_ms = g_profile_stats.cabac_ns.load() / 1e6;
    double mb_ms = g_profile_stats.decode_mb_ns.load() / 1e6;
    double pad_ms = g_profile_stats.pad_ns.load() / 1e6;
    double total_ms = cb_ms + mb_ms + db_ms + pad_ms;

    if (total_ms < 0.001) return;

    std::fprintf(stderr, "\n=======================================================\n");
    std::fprintf(stderr, "           f264dec Performance Hotspot Profile         \n");
    std::fprintf(stderr, "=======================================================\n");
    std::fprintf(stderr, "  Component             Time (ms)     Percentage       \n");
    std::fprintf(stderr, "-------------------------------------------------------\n");
    std::fprintf(stderr, "  CABAC/Entropy Decode  %8.2f ms       %5.1f %%\n", cb_ms, 100.0 * cb_ms / total_ms);
    std::fprintf(stderr, "  MB Reconstruction     %8.2f ms       %5.1f %%\n", mb_ms, 100.0 * mb_ms / total_ms);
    std::fprintf(stderr, "    - Motion Comp (sub) %8.2f ms       %5.1f %%\n", mc_ms, 100.0 * mc_ms / total_ms);
    std::fprintf(stderr, "      * MC Luma         %8.2f ms       %5.1f %%\n", mc_luma_ms, 100.0 * mc_luma_ms / total_ms);
    std::fprintf(stderr, "      * MC Chroma       %8.2f ms       %5.1f %%\n", mc_chroma_ms, 100.0 * mc_chroma_ms / total_ms);
    std::fprintf(stderr, "      * MC Copy/Pred    %8.2f ms       %5.1f %%\n", mc_pred_ms, 100.0 * mc_pred_ms / total_ms);
    std::fprintf(stderr, "    - Transforms (sub)  %8.2f ms       %5.1f %%\n", tr_ms, 100.0 * tr_ms / total_ms);
    std::fprintf(stderr, "  Deblocking Filter     %8.2f ms       %5.1f %%\n", db_ms, 100.0 * db_ms / total_ms);
    std::fprintf(stderr, "    - Strength Calc     %8.2f ms       %5.1f %%\n", db_str_ms, 100.0 * db_str_ms / total_ms);
    std::fprintf(stderr, "    - Edge Filtering    %8.2f ms       %5.1f %%\n", db_flt_ms, 100.0 * db_flt_ms / total_ms);
    std::fprintf(stderr, "  Picture Boundary Pad  %8.2f ms       %5.1f %%\n", pad_ms, 100.0 * pad_ms / total_ms);
    std::fprintf(stderr, "-------------------------------------------------------\n");
    std::fprintf(stderr, "  Profiled Core Total   %8.2f ms       100.0 %%\n", total_ms);
    std::fprintf(stderr, "=======================================================\n\n");
}

}
