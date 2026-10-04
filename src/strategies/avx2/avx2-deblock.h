#ifndef F264_AVX2_DEBLOCK_H_
#define F264_AVX2_DEBLOCK_H_

#include "global.h"

void luma_hor_deblock_normal_avx2(imgpel *imgP, imgpel *imgQ, int width, int Alpha, int Beta, int C0, int max_imgpel_value);
void luma_ver_deblock_normal_avx2(imgpel **cur_img, int pos_x1, int Alpha, int Beta, int C0, int max_imgpel_value);

int f264_strategy_register_deblock_avx2(void *opaque, uint8_t bitdepth);

#endif // F264_AVX2_DEBLOCK_H_
