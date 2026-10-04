#ifndef F264_GENERIC_DEBLOCK_H_
#define F264_GENERIC_DEBLOCK_H_

#include "global.h"

void luma_hor_deblock_normal_generic(imgpel *imgP, imgpel *imgQ, int width, int Alpha, int Beta, int C0, int max_imgpel_value);
void luma_ver_deblock_normal_generic(imgpel **cur_img, int pos_x1, int Alpha, int Beta, int C0, int max_imgpel_value);

int f264_strategy_register_deblock_generic(void *opaque, uint8_t bitdepth);

#endif // F264_GENERIC_DEBLOCK_H_
