#ifndef F264_STRATEGIES_GENERIC_TRANSFORM_H_
#define F264_STRATEGIES_GENERIC_TRANSFORM_H_

#include "global.h"

void inverse4x4_generic(int **tblock, int **block, int pos_y, int pos_x);
void sample_reconstruct_generic(imgpel **curImg, imgpel **mpr, int **mb_rres,
                                int mb_x, int opix_x, int width, int height,
                                int max_imgpel_value, int dq_bits);
void inverse8x8_generic(int **tblock, int **block, int pos_x);
void recon8x8_generic(int **m7, imgpel **mb_rec, imgpel **mpr, int max_imgpel_value, int ioff);

int f264_strategy_register_transform_generic(void *opaque, uint8_t bitdepth);

#endif // F264_STRATEGIES_GENERIC_TRANSFORM_H_
