/**
 * \file strategies-transform.cpp
 * \brief Strategy registration and dispatch for transform and reconstruction.
 */

#include "strategies/strategies-transform.h"
#include "strategies/strategyselector.h"
#include "strategies/generic/generic-transform.h"
#include "strategies/sse2/sse2-transform.h"
#include "strategies/avx2/avx2-transform.h"

// Function pointer storage
f264_inverse4x4_func f264_inverse4x4 = nullptr;
f264_sample_recon_func f264_sample_reconstruct = nullptr;
f264_inverse8x8_func f264_inverse8x8 = nullptr;
f264_recon8x8_func f264_recon8x8 = nullptr;

int f264_strategy_register_transform(void *opaque, uint8_t bitdepth)
{
  bool success = true;

  // Generic C++ implementations
  success &= (f264_strategy_register_transform_generic(opaque, bitdepth) != 0);

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  if (f264_g_hardware_flags.intel_flags.sse2) {
    success &= (f264_strategy_register_transform_sse2(opaque, bitdepth) != 0);
  }
  if (f264_g_hardware_flags.intel_flags.avx2) {
    success &= (f264_strategy_register_transform_avx2(opaque, bitdepth) != 0);
  }
#endif

  return success ? 1 : 0;
}
