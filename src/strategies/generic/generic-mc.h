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

int f264_strategy_register_mc_generic(void *opaque, uint8_t bitdepth);

#endif // F264_STRATEGIES_GENERIC_MC_H_
