/**
 * \file sse2-transform.cpp
 * \brief SSE2 SIMD implementations of inverse transform and reconstruction.
 */

#include "strategies/sse2/sse2-transform.h"
#include "strategies/generic/generic-transform.h"
#include "strategies/strategyselector.h"
#include "global.h"
#include <cstring>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define F264_ARCH_X86 1
#endif

#if defined(F264_ARCH_X86)

static void inverse4x4_sse2(int **tblock, int **block, int pos_y, int pos_x)
{
  __m128i r0 = _mm_loadu_si128((const __m128i*)&tblock[pos_y + 0][pos_x]);
  __m128i r1 = _mm_loadu_si128((const __m128i*)&tblock[pos_y + 1][pos_x]);
  __m128i r2 = _mm_loadu_si128((const __m128i*)&tblock[pos_y + 2][pos_x]);
  __m128i r3 = _mm_loadu_si128((const __m128i*)&tblock[pos_y + 3][pos_x]);

  // Transpose 4x4 32-bit ints
  __m128i t0 = _mm_unpacklo_epi32(r0, r1);
  __m128i t1 = _mm_unpackhi_epi32(r0, r1);
  __m128i t2 = _mm_unpacklo_epi32(r2, r3);
  __m128i t3 = _mm_unpackhi_epi32(r2, r3);

  __m128i c0 = _mm_unpacklo_epi64(t0, t2);
  __m128i c1 = _mm_unpackhi_epi64(t0, t2);
  __m128i c2 = _mm_unpacklo_epi64(t1, t3);
  __m128i c3 = _mm_unpackhi_epi64(t1, t3);

  __m128i p0 = _mm_add_epi32(c0, c2);
  __m128i p1 = _mm_sub_epi32(c0, c2);
  __m128i c1_sra1 = _mm_srai_epi32(c1, 1);
  __m128i c3_sra1 = _mm_srai_epi32(c3, 1);
  __m128i p2 = _mm_sub_epi32(c1_sra1, c3);
  __m128i p3 = _mm_add_epi32(c1, c3_sra1);

  __m128i h0 = _mm_add_epi32(p0, p3);
  __m128i h1 = _mm_add_epi32(p1, p2);
  __m128i h2 = _mm_sub_epi32(p1, p2);
  __m128i h3 = _mm_sub_epi32(p0, p3);

  // Transpose back to do vertical transform
  t0 = _mm_unpacklo_epi32(h0, h1);
  t1 = _mm_unpackhi_epi32(h0, h1);
  t2 = _mm_unpacklo_epi32(h2, h3);
  t3 = _mm_unpackhi_epi32(h2, h3);

  c0 = _mm_unpacklo_epi64(t0, t2);
  c1 = _mm_unpackhi_epi64(t0, t2);
  c2 = _mm_unpacklo_epi64(t1, t3);
  c3 = _mm_unpackhi_epi64(t1, t3);

  p0 = _mm_add_epi32(c0, c2);
  p1 = _mm_sub_epi32(c0, c2);
  c1_sra1 = _mm_srai_epi32(c1, 1);
  c3_sra1 = _mm_srai_epi32(c3, 1);
  p2 = _mm_sub_epi32(c1_sra1, c3);
  p3 = _mm_add_epi32(c1, c3_sra1);

  __m128i v0 = _mm_add_epi32(p0, p3);
  __m128i v1 = _mm_add_epi32(p1, p2);
  __m128i v2 = _mm_sub_epi32(p1, p2);
  __m128i v3 = _mm_sub_epi32(p0, p3);

  _mm_storeu_si128((__m128i*)&block[pos_y + 0][pos_x], v0);
  _mm_storeu_si128((__m128i*)&block[pos_y + 1][pos_x], v1);
  _mm_storeu_si128((__m128i*)&block[pos_y + 2][pos_x], v2);
  _mm_storeu_si128((__m128i*)&block[pos_y + 3][pos_x], v3);
}

static void sample_reconstruct_sse2(imgpel **curImg, imgpel **mpr, int **mb_rres,
                                    int mb_x, int opix_x, int width, int height,
                                    int max_imgpel_value, int dq_bits)
{
  if (dq_bits == 6 && (width % 4 == 0))
  {
    const __m128i offset = _mm_set1_epi32(1 << (6 - 1)); // 32
    const __m128i zero = _mm_setzero_si128();
    const __m128i max_val = _mm_set1_epi32(max_imgpel_value);

    for (int j = 0; j < height; j++)
    {
      imgpel *imgOrg = &curImg[j][opix_x];
      imgpel *imgPred = &mpr[j][mb_x];
      int *m7 = &mb_rres[j][mb_x];

      for (int i = 0; i < width; i += 4)
      {
        __m128i res = _mm_loadu_si128((const __m128i*)&m7[i]);
        res = _mm_add_epi32(res, offset);
        res = _mm_srai_epi32(res, 6);

        __m128i pred4 = _mm_loadl_epi64((const __m128i*)&imgPred[i]);
        __m128i pred_vec = _mm_unpacklo_epi16(pred4, zero);

        __m128i sum = _mm_add_epi32(res, pred_vec);
        sum = _mm_min_epi32(_mm_max_epi32(sum, zero), max_val);

        __m128i packed16 = _mm_packus_epi32(sum, sum);
        _mm_storel_epi64((__m128i*)&imgOrg[i], packed16);
      }
    }
  }
  else
  {
    sample_reconstruct_generic(curImg, mpr, mb_rres, mb_x, opix_x, width, height, max_imgpel_value, dq_bits);
  }
}

#endif // F264_ARCH_X86

int f264_strategy_register_transform_sse2(void *opaque, uint8_t bitdepth)
{
#if defined(F264_ARCH_X86)
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "inverse4x4", "sse2", 10, (void*)inverse4x4_sse2) != 0);
  success &= (f264_strategyselector_register(opaque, "sample_reconstruct", "sse2", 10, (void*)sample_reconstruct_sse2) != 0);
  return success ? 1 : 0;
#else
  return 1;
#endif
}
