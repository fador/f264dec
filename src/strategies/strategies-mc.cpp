/**
 * \file strategies-mc.cpp
 * \brief Strategy registration and dispatch for subpel motion compensation.
 */

#include "strategies/strategies-mc.h"
#include "strategies/strategyselector.h"
#include "strategies/generic/generic-mc.h"
#include "strategies/avx2/avx2-mc.h"

// Function pointers
f264_luma_2d_func f264_get_luma_20 = nullptr;
f264_luma_2d_func f264_get_luma_10 = nullptr;
f264_luma_2d_func f264_get_luma_30 = nullptr;

f264_luma_shift_func f264_get_luma_02 = nullptr;
f264_luma_shift_func f264_get_luma_01 = nullptr;
f264_luma_shift_func f264_get_luma_03 = nullptr;

f264_luma_shift_func f264_get_luma_11 = nullptr;
f264_luma_shift_func f264_get_luma_13 = nullptr;
f264_luma_shift_func f264_get_luma_31 = nullptr;
f264_luma_shift_func f264_get_luma_33 = nullptr;

f264_luma_22_func f264_get_luma_22 = nullptr;
f264_luma_22_func f264_get_luma_21 = nullptr;
f264_luma_22_func f264_get_luma_23 = nullptr;

f264_luma_shift_tmp_func f264_get_luma_12 = nullptr;
f264_luma_shift_tmp_func f264_get_luma_32 = nullptr;

f264_bi_pred_func f264_bi_prediction = nullptr;
f264_weighted_bi_pred_func f264_weighted_bi_prediction = nullptr;
f264_chroma_0X_func f264_get_chroma_0X = nullptr;
f264_chroma_X0_func f264_get_chroma_X0 = nullptr;
f264_chroma_XY_func f264_get_chroma_XY = nullptr;

int f264_strategy_register_mc(void *opaque, uint8_t bitdepth)
{
  bool success = true;

  // Generic C++ implementations
  success &= (f264_strategy_register_mc_generic(opaque, bitdepth) != 0);

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  if (f264_g_hardware_flags.intel_flags.avx2) {
    success &= (f264_strategy_register_mc_avx2(opaque, bitdepth) != 0);
  }
#endif

  return success ? 1 : 0;
}
