#include "strategies/generic/generic-deblock.h"
#include "strategies/strategyselector.h"
#include <algorithm>
#include <cstdlib>

void luma_hor_deblock_normal_generic(imgpel *imgP, imgpel *imgQ, int width, int Alpha, int Beta, int C0, int max_imgpel_value)
{
  int edge_diff;
  int tc0, dif, aq, ap;

  if (C0 == 0)
  {
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
      edge_diff = *imgQ - *imgP;
      if (std::abs(edge_diff) < Alpha)
      {
        imgpel *SrcPtrQ1 = imgQ + width;
        imgpel *SrcPtrP1 = imgP - width;

        if ((std::abs(*imgQ - *SrcPtrQ1) < Beta) && (std::abs(*imgP - *SrcPtrP1) < Beta))
        {
          imgpel R2 = *(SrcPtrQ1 + width);
          imgpel L2 = *(SrcPtrP1 - width);

          aq = (std::abs(*imgQ - R2) < Beta);
          ap = (std::abs(*imgP - L2) < Beta);

          tc0 = (ap + aq);
          dif = std::clamp((((edge_diff) << 2) + (*SrcPtrP1 - *SrcPtrQ1) + 4) >> 3, -tc0, tc0);

          if (dif != 0)
          {
            *imgP = (imgpel)std::clamp(*imgP + dif, 0, max_imgpel_value);
            *imgQ = (imgpel)std::clamp(*imgQ - dif, 0, max_imgpel_value);
          }
        }
      }
      imgP++;
      imgQ++;
    }
  }
  else
  {
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
      edge_diff = *imgQ - *imgP;
      if (std::abs(edge_diff) < Alpha)
      {
        imgpel *SrcPtrQ1 = imgQ + width;
        imgpel *SrcPtrP1 = imgP - width;

        if ((std::abs(*imgQ - *SrcPtrQ1) < Beta) && (std::abs(*imgP - *SrcPtrP1) < Beta))
        {
          int RL0 = (*imgP + *imgQ + 1) >> 1;
          imgpel R2 = *(SrcPtrQ1 + width);
          imgpel L2 = *(SrcPtrP1 - width);

          aq = (std::abs(*imgQ - R2) < Beta);
          ap = (std::abs(*imgP - L2) < Beta);

          tc0 = (C0 + ap + aq);
          dif = std::clamp((((edge_diff) << 2) + (*SrcPtrP1 - *SrcPtrQ1) + 4) >> 3, -tc0, tc0);

          if (ap)
            *SrcPtrP1 = (imgpel)(*SrcPtrP1 + std::clamp((L2 + RL0 - (*SrcPtrP1 << 1)) >> 1, -C0, C0));

          if (dif != 0)
          {
            *imgP = (imgpel)std::clamp(*imgP + dif, 0, max_imgpel_value);
            *imgQ = (imgpel)std::clamp(*imgQ - dif, 0, max_imgpel_value);
          }

          if (aq)
            *SrcPtrQ1 = (imgpel)(*SrcPtrQ1 + std::clamp((R2 + RL0 - (*SrcPtrQ1 << 1)) >> 1, -C0, C0));
        }
      }
      imgP++;
      imgQ++;
    }
  }
}

void luma_ver_deblock_normal_generic(imgpel **cur_img, int pos_x1, int Alpha, int Beta, int C0, int max_imgpel_value)
{
  int edge_diff;
  imgpel *SrcPtrP, *SrcPtrQ;

  if (C0 == 0)
  {
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
      SrcPtrP = *(cur_img++) + pos_x1;
      SrcPtrQ = SrcPtrP + 1;
      edge_diff = *SrcPtrQ - *SrcPtrP;

      if (std::abs(edge_diff) < Alpha)
      {
        imgpel *SrcPtrQ1 = SrcPtrQ + 1;
        imgpel *SrcPtrP1 = SrcPtrP - 1;

        if ((std::abs(*SrcPtrQ - *SrcPtrQ1) < Beta) && (std::abs(*SrcPtrP - *SrcPtrP1) < Beta))
        {
          imgpel R2 = *(SrcPtrQ1 + 1);
          imgpel L2 = *(SrcPtrP1 - 1);

          int aq = (std::abs(*SrcPtrQ - R2) < Beta);
          int ap = (std::abs(*SrcPtrP - L2) < Beta);

          int tc0 = (ap + aq);
          int dif = std::clamp((((edge_diff) << 2) + (*SrcPtrP1 - *SrcPtrQ1) + 4) >> 3, -tc0, tc0);

          if (dif != 0)
          {
            *SrcPtrP = (imgpel)std::clamp(*SrcPtrP + dif, 0, max_imgpel_value);
            *SrcPtrQ = (imgpel)std::clamp(*SrcPtrQ - dif, 0, max_imgpel_value);
          }
        }
      }
    }
  }
  else
  {
    for (int i = 0; i < BLOCK_SIZE; ++i)
    {
      SrcPtrP = *(cur_img++) + pos_x1;
      SrcPtrQ = SrcPtrP + 1;
      edge_diff = *SrcPtrQ - *SrcPtrP;

      if (std::abs(edge_diff) < Alpha)
      {
        imgpel *SrcPtrQ1 = SrcPtrQ + 1;
        imgpel *SrcPtrP1 = SrcPtrP - 1;

        if ((std::abs(*SrcPtrQ - *SrcPtrQ1) < Beta) && (std::abs(*SrcPtrP - *SrcPtrP1) < Beta))
        {
          int RL0 = (*SrcPtrP + *SrcPtrQ + 1) >> 1;
          imgpel R2 = *(SrcPtrQ1 + 1);
          imgpel L2 = *(SrcPtrP1 - 1);

          int aq = (std::abs(*SrcPtrQ - R2) < Beta);
          int ap = (std::abs(*SrcPtrP - L2) < Beta);

          int tc0 = (C0 + ap + aq);
          int dif = std::clamp((((edge_diff) << 2) + (*SrcPtrP1 - *SrcPtrQ1) + 4) >> 3, -tc0, tc0);

          if (ap)
            *SrcPtrP1 = (imgpel)(*SrcPtrP1 + std::clamp((L2 + RL0 - (*SrcPtrP1 << 1)) >> 1, -C0, C0));

          if (dif != 0)
          {
            *SrcPtrP = (imgpel)std::clamp(*SrcPtrP + dif, 0, max_imgpel_value);
            *SrcPtrQ = (imgpel)std::clamp(*SrcPtrQ - dif, 0, max_imgpel_value);
          }

          if (aq)
            *SrcPtrQ1 = (imgpel)(*SrcPtrQ1 + std::clamp((R2 + RL0 - (*SrcPtrQ1 << 1)) >> 1, -C0, C0));
        }
      }
    }
  }
}

int f264_strategy_register_deblock_generic(void *opaque, uint8_t bitdepth)
{
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "luma_hor_deblock_normal", "generic", 0, (void*)luma_hor_deblock_normal_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "luma_ver_deblock_normal", "generic", 0, (void*)luma_ver_deblock_normal_generic) != 0);
  return success ? 1 : 0;
}
