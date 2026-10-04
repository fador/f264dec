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
typedef void (*f264_chroma_0X_func)(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w01, int total_scale);
typedef void (*f264_chroma_X0_func)(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w10, int total_scale);
typedef void (*f264_chroma_XY_func)(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w01, int w10, int w11, int total_scale);

typedef void (*f264_luma_shift_tmp_func)(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value);

extern f264_luma_2d_func f264_get_luma_20;
extern f264_luma_2d_func f264_get_luma_10;
extern f264_luma_2d_func f264_get_luma_30;

extern f264_luma_shift_func f264_get_luma_02;
extern f264_luma_shift_func f264_get_luma_01;
extern f264_luma_shift_func f264_get_luma_03;

extern f264_luma_shift_func f264_get_luma_11;
extern f264_luma_shift_func f264_get_luma_13;
extern f264_luma_shift_func f264_get_luma_31;
extern f264_luma_shift_func f264_get_luma_33;

extern f264_luma_22_func f264_get_luma_22;
extern f264_luma_22_func f264_get_luma_21;
extern f264_luma_22_func f264_get_luma_23;

extern f264_luma_shift_tmp_func f264_get_luma_12;
extern f264_luma_shift_tmp_func f264_get_luma_32;

typedef void (*f264_weighted_bi_pred_func)(imgpel *mb_pred, imgpel *block_l0, imgpel *block_l1, int block_size_y, int block_size_x, int wp_scale_l0, int wp_scale_l1, int wp_offset, int weight_denom, int color_clip);

extern f264_bi_pred_func f264_bi_prediction;
extern f264_weighted_bi_pred_func f264_weighted_bi_prediction;
extern f264_chroma_0X_func f264_get_chroma_0X;
extern f264_chroma_X0_func f264_get_chroma_X0;
extern f264_chroma_XY_func f264_get_chroma_XY;

int f264_strategy_register_mc(void *opaque, uint8_t bitdepth);

#define STRATEGIES_MC_EXPORTS \
  {"get_luma_20", (void**)&f264_get_luma_20}, \
  {"get_luma_10", (void**)&f264_get_luma_10}, \
  {"get_luma_30", (void**)&f264_get_luma_30}, \
  {"get_luma_02", (void**)&f264_get_luma_02}, \
  {"get_luma_01", (void**)&f264_get_luma_01}, \
  {"get_luma_03", (void**)&f264_get_luma_03}, \
  {"get_luma_11", (void**)&f264_get_luma_11}, \
  {"get_luma_13", (void**)&f264_get_luma_13}, \
  {"get_luma_31", (void**)&f264_get_luma_31}, \
  {"get_luma_33", (void**)&f264_get_luma_33}, \
  {"get_luma_22", (void**)&f264_get_luma_22}, \
  {"get_luma_21", (void**)&f264_get_luma_21}, \
  {"get_luma_23", (void**)&f264_get_luma_23}, \
  {"get_luma_12", (void**)&f264_get_luma_12}, \
  {"get_luma_32", (void**)&f264_get_luma_32}, \
  {"bi_prediction", (void**)&f264_bi_prediction}, \
  {"weighted_bi_prediction", (void**)&f264_weighted_bi_prediction}, \
  {"get_chroma_0X", (void**)&f264_get_chroma_0X}, \
  {"get_chroma_X0", (void**)&f264_get_chroma_X0}, \
  {"get_chroma_XY", (void**)&f264_get_chroma_XY},

#endif // F264_STRATEGIES_MC_H_
