#ifndef F264_STRATEGIES_DEBLOCK_H_
#define F264_STRATEGIES_DEBLOCK_H_

/**
 * \file strategies-deblock.h
 * \brief Strategy interface for loop filtering and deblocking.
 */

#include "global.h"

typedef void (*f264_deblock_luma_hor_func)(imgpel *imgP, imgpel *imgQ, int width, int Alpha, int Beta, int C0, int max_imgpel_value);
typedef void (*f264_deblock_luma_ver_func)(imgpel **cur_img, int pos_x1, int Alpha, int Beta, int C0, int max_imgpel_value);

extern f264_deblock_luma_hor_func f264_luma_hor_deblock_normal;
extern f264_deblock_luma_ver_func f264_luma_ver_deblock_normal;

int f264_strategy_register_deblock(void *opaque, uint8_t bitdepth);

#define STRATEGIES_DEBLOCK_EXPORTS \
  {"luma_hor_deblock_normal", (void**)&f264_luma_hor_deblock_normal}, \
  {"luma_ver_deblock_normal", (void**)&f264_luma_ver_deblock_normal},

#endif // F264_STRATEGIES_DEBLOCK_H_
