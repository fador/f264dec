#ifndef F264_STRATEGIES_MC_H_
#define F264_STRATEGIES_MC_H_

/**
 * \file strategies-mc.h
 * \brief Strategy interface for motion compensation and subpel interpolation.
 */

#include "global.h"

typedef void (*f264_luma_2d_func)(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value);
typedef void (*f264_luma_shift_func)(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value);
typedef void (*f264_luma_22_func)(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value);
typedef void (*f264_bi_pred_func)(imgpel **mb_pred, imgpel **block_l0, imgpel **block_l1, int block_size_y, int block_size_x, int ioff);

extern f264_luma_2d_func f264_get_luma_20;
extern f264_luma_2d_func f264_get_luma_10;
extern f264_luma_2d_func f264_get_luma_30;

extern f264_luma_shift_func f264_get_luma_02;
extern f264_luma_shift_func f264_get_luma_01;
extern f264_luma_shift_func f264_get_luma_03;

extern f264_luma_22_func f264_get_luma_22;
extern f264_bi_pred_func f264_bi_prediction;

int f264_strategy_register_mc(void *opaque, uint8_t bitdepth);

#define STRATEGIES_MC_EXPORTS \
  {"get_luma_20", (void**)&f264_get_luma_20}, \
  {"get_luma_10", (void**)&f264_get_luma_10}, \
  {"get_luma_30", (void**)&f264_get_luma_30}, \
  {"get_luma_02", (void**)&f264_get_luma_02}, \
  {"get_luma_01", (void**)&f264_get_luma_01}, \
  {"get_luma_03", (void**)&f264_get_luma_03}, \
  {"get_luma_22", (void**)&f264_get_luma_22}, \
  {"bi_prediction", (void**)&f264_bi_prediction},

#endif // F264_STRATEGIES_MC_H_
