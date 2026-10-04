/**
 * \file generic-mc.cpp
 * \brief Generic C++ implementations of subpel motion compensation.
 */

#include "strategies/generic/generic-mc.h"
#include "strategies/strategyselector.h"
#include "global.h"
#include <cstring>
#include <algorithm>

void get_luma_20_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  for (int j = 0; j < block_size_y; j++)
  {
    imgpel *p0 = &cur_imgY[j][x_pos - 2];
    imgpel *p1 = p0 + 1;
    imgpel *p2 = p1 + 1;
    imgpel *p3 = p2 + 1;
    imgpel *p4 = p3 + 1;
    imgpel *p5 = p4 + 1;
    imgpel *orig_line = block[j];

    for (int i = 0; i < block_size_x; i++)
    {        
      int result = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
      *orig_line++ = (imgpel) std::clamp((result + 16) >> 5, 0, max_imgpel_value);
    }
  }
}

void get_luma_10_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  for (int j = 0; j < block_size_y; j++)
  {
    imgpel *cur_line = &(cur_imgY[j][x_pos]);
    imgpel *p0 = &cur_imgY[j][x_pos - 2];
    imgpel *p1 = p0 + 1;
    imgpel *p2 = p1 + 1;
    imgpel *p3 = p2 + 1;
    imgpel *p4 = p3 + 1;
    imgpel *p5 = p4 + 1;
    imgpel *orig_line = block[j];            

    for (int i = 0; i < block_size_x; i++)
    {        
      int result = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
      int val = std::clamp((result + 16) >> 5, 0, max_imgpel_value);
      *orig_line++ = (imgpel) ((val + *(cur_line++) + 1) >> 1);
    }
  }
}

void get_luma_30_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  for (int j = 0; j < block_size_y; j++)
  {
    imgpel *cur_line = &(cur_imgY[j][x_pos + 1]);
    imgpel *p0 = &cur_imgY[j][x_pos - 2];
    imgpel *p1 = p0 + 1;
    imgpel *p2 = p1 + 1;
    imgpel *p3 = p2 + 1;
    imgpel *p4 = p3 + 1;
    imgpel *p5 = p4 + 1;
    imgpel *orig_line = block[j];            

    for (int i = 0; i < block_size_x; i++)
    {        
      int result = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
      int val = std::clamp((result + 16) >> 5, 0, max_imgpel_value);
      *orig_line++ = (imgpel) ((val + *(cur_line++) + 1) >> 1);
    }
  }
}

void get_luma_02_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  imgpel *p0 = &(cur_imgY[-2][x_pos]);
  for (int j = 0; j < block_size_y; j++)
  {                  
    imgpel *p1 = p0 + shift_x;          
    imgpel *p2 = p1 + shift_x;
    imgpel *p3 = p2 + shift_x;
    imgpel *p4 = p3 + shift_x;
    imgpel *p5 = p4 + shift_x;
    imgpel *orig_line = block[j];

    for (int i = 0; i < block_size_x; i++)
    {
      int result = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
      *orig_line++ = (imgpel) std::clamp((result + 16) >> 5, 0, max_imgpel_value);
    }
    p0 = p1 - block_size_x;
  }
}

void get_luma_01_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  int jj = 0;
  imgpel *p0 = &(cur_imgY[-2][x_pos]);
  for (int j = 0; j < block_size_y; j++)
  {                  
    imgpel *p1 = p0 + shift_x;          
    imgpel *p2 = p1 + shift_x;
    imgpel *p3 = p2 + shift_x;
    imgpel *p4 = p3 + shift_x;
    imgpel *p5 = p4 + shift_x;
    imgpel *orig_line = block[j];
    imgpel *cur_line = &(cur_imgY[jj++][x_pos]);

    for (int i = 0; i < block_size_x; i++)
    {
      int result = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
      int val = std::clamp((result + 16) >> 5, 0, max_imgpel_value);
      *orig_line++ = (imgpel) ((val + *(cur_line++) + 1) >> 1);
    }
    p0 = p1 - block_size_x;
  }
}

void get_luma_03_generic(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  int jj = 1;
  imgpel *p0 = &(cur_imgY[-2][x_pos]);
  for (int j = 0; j < block_size_y; j++)
  {                  
    imgpel *p1 = p0 + shift_x;          
    imgpel *p2 = p1 + shift_x;
    imgpel *p3 = p2 + shift_x;
    imgpel *p4 = p3 + shift_x;
    imgpel *p5 = p4 + shift_x;
    imgpel *orig_line = block[j];
    imgpel *cur_line = &(cur_imgY[jj++][x_pos]);

    for (int i = 0; i < block_size_x; i++)
    {
      int result = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
      int val = std::clamp((result + 16) >> 5, 0, max_imgpel_value);
      *orig_line++ = (imgpel) ((val + *(cur_line++) + 1) >> 1);
    }
    p0 = p1 - block_size_x;
  }
}

void get_luma_22_generic(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  int jj = -2;
  for (int j = 0; j < block_size_y + 5; j++)
  {
    imgpel *p0 = &cur_imgY[jj++][x_pos - 2];
    imgpel *p1 = p0 + 1;
    imgpel *p2 = p1 + 1;
    imgpel *p3 = p2 + 1;
    imgpel *p4 = p3 + 1;
    imgpel *p5 = p4 + 1;          
    int *tmp_line = tmp_res[j];

    for (int i = 0; i < block_size_x; i++)
    {        
      *(tmp_line++) = (*(p0++) + *(p5++)) - 5 * (*(p1++) + *(p4++)) + 20 * (*(p2++) + *(p3++));
    }
  }

  for (int j = 0; j < block_size_y; j++)
  {
    int *x0 = tmp_res[j    ];
    int *x1 = tmp_res[j + 1];
    int *x2 = tmp_res[j + 2];
    int *x3 = tmp_res[j + 3];
    int *x4 = tmp_res[j + 4];
    int *x5 = tmp_res[j + 5];
    imgpel *orig_line = block[j];

    for (int i = 0; i < block_size_x; i++)
    {
      int result = (*x0++ + *x5++) - 5 * (*x1++ + *x4++) + 20 * (*x2++ + *x3++);
      *(orig_line++) = (imgpel) std::clamp((result + 512) >> 10, 0, max_imgpel_value);
    }
  }
}

int f264_strategy_register_mc_generic(void *opaque, uint8_t bitdepth)
{
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "get_luma_20", "generic", 0, (void*)get_luma_20_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_10", "generic", 0, (void*)get_luma_10_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_30", "generic", 0, (void*)get_luma_30_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_02", "generic", 0, (void*)get_luma_02_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_01", "generic", 0, (void*)get_luma_01_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_03", "generic", 0, (void*)get_luma_03_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_22", "generic", 0, (void*)get_luma_22_generic) != 0);
  return success ? 1 : 0;
}
