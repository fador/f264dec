#ifndef F264_STRATEGIES_GENERIC_MC_H_
#define F264_STRATEGIES_GENERIC_MC_H_

#include "global.h"

void get_luma_20_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value);
void get_luma_10_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value);
void get_luma_30_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value);
void get_luma_02_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value);
void get_luma_01_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value);
void get_luma_03_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value);
void get_luma_22_generic(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value);
void bi_prediction_generic(imgpel **mb_pred, imgpel **block_l0, imgpel **block_l1, int block_size_y, int block_size_x, int ioff);
void get_chroma_0X_generic(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w01, int total_scale);
void get_chroma_X0_generic(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w10, int total_scale);
void get_chroma_XY_generic(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w01, int w10, int w11, int total_scale);

int f264_strategy_register_mc_generic(void *opaque, uint8_t bitdepth);

#endif // F264_STRATEGIES_GENERIC_MC_H_
