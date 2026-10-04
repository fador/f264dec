/**
 * \file generic-transform.cpp
 * \brief Generic C++ implementations of inverse transform and reconstruction.
 */

#include "strategies/generic/generic-transform.h"
#include "strategies/strategyselector.h"
#include "global.h"
#include "defines.h"
#include <cstring>
#include <algorithm>

void inverse4x4_generic(int **tblock, int **block, int pos_y, int pos_x)
{
  int i, ii;
  int tmp[16];
  int *pTmp = tmp;
  int p0, p1, p2, p3;
  int t0, t1, t2, t3;

  // Horizontal
  for (i = pos_y; i < pos_y + BLOCK_SIZE; i++)
  {
    int *pblock = &tblock[i][pos_x];
    t0 = pblock[0];
    t1 = pblock[1];
    t2 = pblock[2];
    t3 = pblock[3];

    p0 = t0 + t2;
    p1 = t0 - t2;
    p2 = (t1 >> 1) - t3;
    p3 = t1 + (t3 >> 1);

    *(pTmp++) = p0 + p3;
    *(pTmp++) = p1 + p2;
    *(pTmp++) = p1 - p2;
    *(pTmp++) = p0 - p3;
  }

  // Vertical
  for (i = 0; i < BLOCK_SIZE; i++)
  {
    pTmp = tmp + i;
    t0 = *pTmp;
    t1 = *(pTmp += BLOCK_SIZE);
    t2 = *(pTmp += BLOCK_SIZE);
    t3 = *(pTmp += BLOCK_SIZE);

    p0 = t0 + t2;
    p1 = t0 - t2;
    p2 = (t1 >> 1) - t3;
    p3 = t1 + (t3 >> 1);

    ii = i + pos_x;
    block[pos_y    ][ii] = p0 + p3;
    block[pos_y + 1][ii] = p1 + p2;
    block[pos_y + 2][ii] = p1 - p2;
    block[pos_y + 3][ii] = p0 - p3;
  }
}

void sample_reconstruct_generic(imgpel **curImg, imgpel **mpr, int **mb_rres,
                                int mb_x, int opix_x, int width, int height,
                                int max_imgpel_value, int dq_bits)
{
  for (int j = 0; j < height; j++)
  {
    imgpel *imgOrg = &curImg[j][opix_x];
    imgpel *imgPred = &mpr[j][mb_x];
    int *m7 = &mb_rres[j][mb_x];
    for (int i = 0; i < width; i++)
    {
      int val = rshift_rnd_sf(*m7++, dq_bits) + (*imgPred++);
      *imgOrg++ = (imgpel)std::clamp(val, 0, max_imgpel_value);
    }
  }
}

void inverse8x8_generic(int **tblock, int **block, int pos_x)
{
  int i, ii;
  int tmp[64];
  int *pTmp = tmp, *pblock;
  int a0, a1, a2, a3;
  int p0, p1, p2, p3, p4, p5, p6, p7;  
  int b0, b1, b2, b3, b4, b5, b6, b7;

  // Horizontal  
  for (i = 0; i < BLOCK_SIZE_8x8; i++)
  {
    pblock = &tblock[i][pos_x];
    p0 = *(pblock++);
    p1 = *(pblock++);
    p2 = *(pblock++);
    p3 = *(pblock++);
    p4 = *(pblock++);
    p5 = *(pblock++);
    p6 = *(pblock++);
    p7 = *(pblock  );

    a0 = p0 + p4;
    a1 = p0 - p4;
    a2 = p6 - (p2 >> 1);
    a3 = p2 + (p6 >> 1);

    b0 =  a0 + a3;
    b2 =  a1 - a2;
    b4 =  a1 + a2;
    b6 =  a0 - a3;

    a0 = -p3 + p5 - p7 - (p7 >> 1);    
    a1 =  p1 + p7 - p3 - (p3 >> 1);    
    a2 = -p1 + p7 + p5 + (p5 >> 1);    
    a3 =  p3 + p5 + p1 + (p1 >> 1);

    b1 =  a0 + (a3 >> 2);    
    b3 =  a1 + (a2 >> 2);    
    b5 =  a2 - (a1 >> 2);
    b7 =  a3 - (a0 >> 2);                

    *(pTmp++) = b0 + b7;
    *(pTmp++) = b2 - b5;
    *(pTmp++) = b4 + b3;
    *(pTmp++) = b6 + b1;
    *(pTmp++) = b6 - b1;
    *(pTmp++) = b4 - b3;
    *(pTmp++) = b2 + b5;
    *(pTmp++) = b0 - b7;
  }

  // Vertical 
  for (i = 0; i < BLOCK_SIZE_8x8; i++)
  {
    pTmp = tmp + i;
    p0 = *pTmp;
    p1 = *(pTmp += BLOCK_SIZE_8x8);
    p2 = *(pTmp += BLOCK_SIZE_8x8);
    p3 = *(pTmp += BLOCK_SIZE_8x8);
    p4 = *(pTmp += BLOCK_SIZE_8x8);
    p5 = *(pTmp += BLOCK_SIZE_8x8);
    p6 = *(pTmp += BLOCK_SIZE_8x8);
    p7 = *(pTmp += BLOCK_SIZE_8x8);

    a0 =  p0 + p4;
    a1 =  p0 - p4;
    a2 =  p6 - (p2 >> 1);
    a3 =  p2 + (p6 >> 1);

    b0 = a0 + a3;
    b2 = a1 - a2;
    b4 = a1 + a2;
    b6 = a0 - a3;

    a0 = -p3 + p5 - p7 - (p7 >> 1);
    a1 =  p1 + p7 - p3 - (p3 >> 1);
    a2 = -p1 + p7 + p5 + (p5 >> 1);
    a3 =  p3 + p5 + p1 + (p1 >> 1);

    b1 =  a0 + (a3 >> 2);
    b7 =  a3 - (a0 >> 2);
    b3 =  a1 + (a2 >> 2);
    b5 =  a2 - (a1 >> 2);

    ii = i + pos_x;
    block[0][ii] = b0 + b7;
    block[1][ii] = b2 - b5;
    block[2][ii] = b4 + b3;
    block[3][ii] = b6 + b1;
    block[4][ii] = b6 - b1;
    block[5][ii] = b4 - b3;
    block[6][ii] = b2 + b5;
    block[7][ii] = b0 - b7;
  }
}

void recon8x8_generic(int **m7, imgpel **mb_rec, imgpel **mpr, int max_imgpel_value, int ioff)
{
  for (int j = 0; j < 8; j++)
  {
    int    *m_tr  = (*m7++) + ioff;
    imgpel *m_rec = (*mb_rec++) + ioff;
    imgpel *m_prd = (*mpr++) + ioff;

    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec++ = (imgpel) std::clamp((*m_prd++) + rshift_rnd_sf(*m_tr++, DQ_BITS_8), 0, max_imgpel_value);
    *m_rec   = (imgpel) std::clamp((*m_prd  ) + rshift_rnd_sf(*m_tr  , DQ_BITS_8), 0, max_imgpel_value);
  }
}

int f264_strategy_register_transform_generic(void *opaque, uint8_t bitdepth)
{
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "inverse4x4", "generic", 0, (void*)inverse4x4_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "sample_reconstruct", "generic", 0, (void*)sample_reconstruct_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "inverse8x8", "generic", 0, (void*)inverse8x8_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "recon8x8", "generic", 0, (void*)recon8x8_generic) != 0);
  return success ? 1 : 0;
}
