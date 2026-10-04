#include "strategies/strategies-deblock.h"
#include "strategies/generic/generic-deblock.h"
#include "strategies/avx2/avx2-deblock.h"

f264_deblock_luma_hor_func f264_luma_hor_deblock_normal = nullptr;
f264_deblock_luma_ver_func f264_luma_ver_deblock_normal = nullptr;

int f264_strategy_register_deblock(void *opaque, uint8_t bitdepth)
{
  bool success = true;
  success &= (f264_strategy_register_deblock_generic(opaque, bitdepth) != 0);
  success &= (f264_strategy_register_deblock_avx2(opaque, bitdepth) != 0);
  return success ? 1 : 0;
}
