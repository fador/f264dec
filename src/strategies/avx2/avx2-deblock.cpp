#include "strategies/avx2/avx2-deblock.h"
#include "strategies/generic/generic-deblock.h"
#include "strategies/strategyselector.h"

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define F264_ARCH_X86 1
#include <immintrin.h>
#endif

#if defined(F264_ARCH_X86)

static inline void deblock_luma_4pel_sse(
    __m128i &p0, __m128i &q0, __m128i &p1, __m128i &q1,
    __m128i p2, __m128i q2,
    int Alpha, int Beta, int C0, int max_imgpel_value)
{
  __m128i edge_diff = _mm_sub_epi16(q0, p0);
  __m128i abs_edge_diff = _mm_abs_epi16(edge_diff);
  __m128i mask_alpha = _mm_cmplt_epi16(abs_edge_diff, _mm_set1_epi16(Alpha));

  if (_mm_testz_si128(mask_alpha, mask_alpha))
    return;

  __m128i beta_vec = _mm_set1_epi16(Beta);
  __m128i abs_p0_p1 = _mm_abs_epi16(_mm_sub_epi16(p0, p1));
  __m128i abs_q0_q1 = _mm_abs_epi16(_mm_sub_epi16(q0, q1));
  __m128i mask_beta = _mm_and_si128(_mm_cmplt_epi16(abs_p0_p1, beta_vec),
                                    _mm_cmplt_epi16(abs_q0_q1, beta_vec));
  __m128i filter_mask = _mm_and_si128(mask_alpha, mask_beta);

  if (_mm_testz_si128(filter_mask, filter_mask))
    return;

  __m128i abs_p0_p2 = _mm_abs_epi16(_mm_sub_epi16(p0, p2));
  __m128i abs_q0_q2 = _mm_abs_epi16(_mm_sub_epi16(q0, q2));
  __m128i mask_ap = _mm_and_si128(filter_mask, _mm_cmplt_epi16(abs_p0_p2, beta_vec));
  __m128i mask_aq = _mm_and_si128(filter_mask, _mm_cmplt_epi16(abs_q0_q2, beta_vec));

  __m128i ap_val = _mm_srli_epi16(mask_ap, 15);
  __m128i aq_val = _mm_srli_epi16(mask_aq, 15);
  __m128i tc0 = _mm_add_epi16(_mm_set1_epi16(C0), _mm_add_epi16(ap_val, aq_val));

  // (((edge_diff) << 2) + (*SrcPtrP1 - *SrcPtrQ1) + 4) >> 3
  __m128i inner = _mm_add_epi16(_mm_slli_epi16(edge_diff, 2), _mm_sub_epi16(p1, q1));
  inner = _mm_add_epi16(inner, _mm_set1_epi16(4));
  __m128i raw_dif = _mm_srai_epi16(inner, 3);
  __m128i neg_tc0 = _mm_sub_epi16(_mm_setzero_si128(), tc0);
  __m128i dif = _mm_min_epi16(_mm_max_epi16(raw_dif, neg_tc0), tc0);
  dif = _mm_and_si128(dif, filter_mask);

  if (C0 > 0)
  {
    // RL0 = (*imgP + *imgQ + 1) >> 1
    __m128i RL0 = _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(p0, q0), _mm_set1_epi16(1)), 1);
    __m128i c0_vec = _mm_set1_epi16(C0);
    __m128i neg_c0_vec = _mm_sub_epi16(_mm_setzero_si128(), c0_vec);

    // *SrcPtrP1 + iClip3( -C0,  C0, (L2 + RL0 - (*SrcPtrP1<<1)) >> 1 )
    __m128i p1_delta = _mm_srai_epi16(_mm_sub_epi16(_mm_add_epi16(p2, RL0), _mm_slli_epi16(p1, 1)), 1);
    p1_delta = _mm_min_epi16(_mm_max_epi16(p1_delta, neg_c0_vec), c0_vec);
    p1_delta = _mm_and_si128(p1_delta, mask_ap);
    p1 = _mm_add_epi16(p1, p1_delta);

    // *SrcPtrQ1 + iClip3( -C0,  C0, (R2 + RL0 - (*SrcPtrQ1<<1)) >> 1 )
    __m128i q1_delta = _mm_srai_epi16(_mm_sub_epi16(_mm_add_epi16(q2, RL0), _mm_slli_epi16(q1, 1)), 1);
    q1_delta = _mm_min_epi16(_mm_max_epi16(q1_delta, neg_c0_vec), c0_vec);
    q1_delta = _mm_and_si128(q1_delta, mask_aq);
    q1 = _mm_add_epi16(q1, q1_delta);
  }

  p0 = _mm_min_epi16(_mm_max_epi16(_mm_add_epi16(p0, dif), _mm_setzero_si128()), _mm_set1_epi16(max_imgpel_value));
  q0 = _mm_min_epi16(_mm_max_epi16(_mm_sub_epi16(q0, dif), _mm_setzero_si128()), _mm_set1_epi16(max_imgpel_value));
}

void luma_hor_deblock_normal_avx2(imgpel *imgP, imgpel *imgQ, int width, int Alpha, int Beta, int C0, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1)
  {
    __m128i p0 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(const int32_t*)imgP));
    __m128i q0 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(const int32_t*)imgQ));
    __m128i p1 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(const int32_t*)(imgP - width)));
    __m128i q1 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(const int32_t*)(imgQ + width)));
    __m128i p2 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(const int32_t*)(imgP - 2 * width)));
    __m128i q2 = _mm_cvtepu8_epi16(_mm_cvtsi32_si128(*(const int32_t*)(imgQ + 2 * width)));

    deblock_luma_4pel_sse(p0, q0, p1, q1, p2, q2, Alpha, Beta, C0, max_imgpel_value);

    *(int32_t*)imgP = _mm_cvtsi128_si32(_mm_packus_epi16(p0, p0));
    *(int32_t*)imgQ = _mm_cvtsi128_si32(_mm_packus_epi16(q0, q0));
    if (C0 > 0)
    {
      *(int32_t*)(imgP - width) = _mm_cvtsi128_si32(_mm_packus_epi16(p1, p1));
      *(int32_t*)(imgQ + width) = _mm_cvtsi128_si32(_mm_packus_epi16(q1, q1));
    }
    return;
  }
  luma_hor_deblock_normal_generic(imgP, imgQ, width, Alpha, Beta, C0, max_imgpel_value);
}

void luma_ver_deblock_normal_avx2(imgpel **cur_img, int pos_x1, int Alpha, int Beta, int C0, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1)
  {
    int16_t p2_arr[4] = { (int16_t)cur_img[0][pos_x1 - 2], (int16_t)cur_img[1][pos_x1 - 2], (int16_t)cur_img[2][pos_x1 - 2], (int16_t)cur_img[3][pos_x1 - 2] };
    int16_t p1_arr[4] = { (int16_t)cur_img[0][pos_x1 - 1], (int16_t)cur_img[1][pos_x1 - 1], (int16_t)cur_img[2][pos_x1 - 1], (int16_t)cur_img[3][pos_x1 - 1] };
    int16_t p0_arr[4] = { (int16_t)cur_img[0][pos_x1 + 0], (int16_t)cur_img[1][pos_x1 + 0], (int16_t)cur_img[2][pos_x1 + 0], (int16_t)cur_img[3][pos_x1 + 0] };
    int16_t q0_arr[4] = { (int16_t)cur_img[0][pos_x1 + 1], (int16_t)cur_img[1][pos_x1 + 1], (int16_t)cur_img[2][pos_x1 + 1], (int16_t)cur_img[3][pos_x1 + 1] };
    int16_t q1_arr[4] = { (int16_t)cur_img[0][pos_x1 + 2], (int16_t)cur_img[1][pos_x1 + 2], (int16_t)cur_img[2][pos_x1 + 2], (int16_t)cur_img[3][pos_x1 + 2] };
    int16_t q2_arr[4] = { (int16_t)cur_img[0][pos_x1 + 3], (int16_t)cur_img[1][pos_x1 + 3], (int16_t)cur_img[2][pos_x1 + 3], (int16_t)cur_img[3][pos_x1 + 3] };

    __m128i p2 = _mm_loadl_epi64((const __m128i*)p2_arr);
    __m128i p1 = _mm_loadl_epi64((const __m128i*)p1_arr);
    __m128i p0 = _mm_loadl_epi64((const __m128i*)p0_arr);
    __m128i q0 = _mm_loadl_epi64((const __m128i*)q0_arr);
    __m128i q1 = _mm_loadl_epi64((const __m128i*)q1_arr);
    __m128i q2 = _mm_loadl_epi64((const __m128i*)q2_arr);

    deblock_luma_4pel_sse(p0, q0, p1, q1, p2, q2, Alpha, Beta, C0, max_imgpel_value);

    _mm_storel_epi64((__m128i*)p0_arr, p0);
    _mm_storel_epi64((__m128i*)q0_arr, q0);
    if (C0 > 0)
    {
      _mm_storel_epi64((__m128i*)p1_arr, p1);
      _mm_storel_epi64((__m128i*)q1_arr, q1);
      for (int r = 0; r < 4; r++)
      {
        cur_img[r][pos_x1 - 1] = (imgpel)p1_arr[r];
        cur_img[r][pos_x1 + 0] = (imgpel)p0_arr[r];
        cur_img[r][pos_x1 + 1] = (imgpel)q0_arr[r];
        cur_img[r][pos_x1 + 2] = (imgpel)q1_arr[r];
      }
    }
    else
    {
      for (int r = 0; r < 4; r++)
      {
        cur_img[r][pos_x1 + 0] = (imgpel)p0_arr[r];
        cur_img[r][pos_x1 + 1] = (imgpel)q0_arr[r];
      }
    }
    return;
  }
  luma_ver_deblock_normal_generic(cur_img, pos_x1, Alpha, Beta, C0, max_imgpel_value);
}

#endif // F264_ARCH_X86

int f264_strategy_register_deblock_avx2(void *opaque, uint8_t bitdepth)
{
#if defined(F264_ARCH_X86)
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "luma_hor_deblock_normal", "avx2", 20, (void*)luma_hor_deblock_normal_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "luma_ver_deblock_normal", "avx2", 20, (void*)luma_ver_deblock_normal_avx2) != 0);
  return success ? 1 : 0;
#else
  return 1;
#endif
}
