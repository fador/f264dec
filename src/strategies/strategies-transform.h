#ifndef F264_STRATEGIES_TRANSFORM_H_
#define F264_STRATEGIES_TRANSFORM_H_

/**
 * \file strategies-transform.h
 * \brief Strategy interface for inverse transform and sample reconstruction.
 */

#include "global.h"

typedef void (*f264_inverse4x4_func)(int **tblock, int **block, int pos_y, int pos_x);
typedef void (*f264_sample_recon_func)(imgpel **curImg, imgpel **mpr, int **mb_rres, int mb_x, int opix_x, int width, int height, int max_imgpel_value, int dq_bits);

// Function pointers dispatched at runtime by strategyselector
extern f264_inverse4x4_func f264_inverse4x4;
extern f264_sample_recon_func f264_sample_reconstruct;

int f264_strategy_register_transform(void *opaque, uint8_t bitdepth);

#define STRATEGIES_TRANSFORM_EXPORTS \
  {"inverse4x4", (void**)&f264_inverse4x4}, \
  {"sample_reconstruct", (void**)&f264_sample_reconstruct},

#endif // F264_STRATEGIES_TRANSFORM_H_
