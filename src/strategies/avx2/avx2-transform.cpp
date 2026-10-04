/**
 * \file avx2-transform.cpp
 * \brief AVX2 SIMD implementations of inverse transform and reconstruction.
 */

#include "strategies/avx2/avx2-transform.h"
#include "strategies/generic/generic-transform.h"
#include "strategies/strategyselector.h"
#include "global.h"
#include "defines.h"
#include <cstring>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC target("avx2")
#endif
#include <immintrin.h>
#define F264_ARCH_X86 1
#endif

#if defined(F264_ARCH_X86)

static inline void transpose8x8_epi32(__m256i &r0, __m256i &r1, __m256i &r2, __m256i &r3,
                                      __m256i &r4, __m256i &r5, __m256i &r6, __m256i &r7)
{
  __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
  __m256i t1 = _mm256_unpackhi_epi32(r0, r1);
  __m256i t2 = _mm256_unpacklo_epi32(r2, r3);
  __m256i t3 = _mm256_unpackhi_epi32(r2, r3);
  __m256i t4 = _mm256_unpacklo_epi32(r4, r5);
  __m256i t5 = _mm256_unpackhi_epi32(r4, r5);
  __m256i t6 = _mm256_unpacklo_epi32(r6, r7);
  __m256i t7 = _mm256_unpackhi_epi32(r6, r7);

  __m256i u0 = _mm256_unpacklo_epi64(t0, t2);
  __m256i u1 = _mm256_unpackhi_epi64(t0, t2);
  __m256i u2 = _mm256_unpacklo_epi64(t1, t3);
  __m256i u3 = _mm256_unpackhi_epi64(t1, t3);
  __m256i u4 = _mm256_unpacklo_epi64(t4, t6);
  __m256i u5 = _mm256_unpackhi_epi64(t4, t6);
  __m256i u6 = _mm256_unpacklo_epi64(t5, t7);
  __m256i u7 = _mm256_unpackhi_epi64(t5, t7);

  r0 = _mm256_permute2f128_si256(u0, u4, 0x20);
  r1 = _mm256_permute2f128_si256(u1, u5, 0x20);
  r2 = _mm256_permute2f128_si256(u2, u6, 0x20);
  r3 = _mm256_permute2f128_si256(u3, u7, 0x20);
  r4 = _mm256_permute2f128_si256(u0, u4, 0x31);
  r5 = _mm256_permute2f128_si256(u1, u5, 0x31);
  r6 = _mm256_permute2f128_si256(u2, u6, 0x31);
  r7 = _mm256_permute2f128_si256(u3, u7, 0x31);
}

static inline void transform8_1d_epi32(
    __m256i p0, __m256i p1, __m256i p2, __m256i p3,
    __m256i p4, __m256i p5, __m256i p6, __m256i p7,
    __m256i &out0, __m256i &out1, __m256i &out2, __m256i &out3,
    __m256i &out4, __m256i &out5, __m256i &out6, __m256i &out7)
{
  __m256i a0 = _mm256_add_epi32(p0, p4);
  __m256i a1 = _mm256_sub_epi32(p0, p4);
  __m256i a2 = _mm256_sub_epi32(p6, _mm256_srai_epi32(p2, 1));
  __m256i a3 = _mm256_add_epi32(p2, _mm256_srai_epi32(p6, 1));

  __m256i b0 = _mm256_add_epi32(a0, a3);
  __m256i b2 = _mm256_sub_epi32(a1, a2);
  __m256i b4 = _mm256_add_epi32(a1, a2);
  __m256i b6 = _mm256_sub_epi32(a0, a3);

  __m256i p7_sra1 = _mm256_srai_epi32(p7, 1);
  __m256i p3_sra1 = _mm256_srai_epi32(p3, 1);
  __m256i p5_sra1 = _mm256_srai_epi32(p5, 1);
  __m256i p1_sra1 = _mm256_srai_epi32(p1, 1);

  __m256i t_a0 = _mm256_sub_epi32(_mm256_sub_epi32(p5, p3), _mm256_add_epi32(p7, p7_sra1));
  __m256i t_a1 = _mm256_sub_epi32(_mm256_add_epi32(p1, p7), _mm256_add_epi32(p3, p3_sra1));
  __m256i t_a2 = _mm256_add_epi32(_mm256_sub_epi32(p7, p1), _mm256_add_epi32(p5, p5_sra1));
  __m256i t_a3 = _mm256_add_epi32(_mm256_add_epi32(p3, p5), _mm256_add_epi32(p1, p1_sra1));

  __m256i b1 = _mm256_add_epi32(t_a0, _mm256_srai_epi32(t_a3, 2));
  __m256i b3 = _mm256_add_epi32(t_a1, _mm256_srai_epi32(t_a2, 2));
  __m256i b5 = _mm256_sub_epi32(t_a2, _mm256_srai_epi32(t_a1, 2));
  __m256i b7 = _mm256_sub_epi32(t_a3, _mm256_srai_epi32(t_a0, 2));

  out0 = _mm256_add_epi32(b0, b7);
  out1 = _mm256_sub_epi32(b2, b5);
  out2 = _mm256_add_epi32(b4, b3);
  out3 = _mm256_add_epi32(b6, b1);
  out4 = _mm256_sub_epi32(b6, b1);
  out5 = _mm256_sub_epi32(b4, b3);
  out6 = _mm256_add_epi32(b2, b5);
  out7 = _mm256_sub_epi32(b0, b7);
}

static void inverse8x8_avx2(int **tblock, int **block, int pos_x)
{
  __m256i r0 = _mm256_loadu_si256((const __m256i*)&tblock[0][pos_x]);
  __m256i r1 = _mm256_loadu_si256((const __m256i*)&tblock[1][pos_x]);
  __m256i r2 = _mm256_loadu_si256((const __m256i*)&tblock[2][pos_x]);
  __m256i r3 = _mm256_loadu_si256((const __m256i*)&tblock[3][pos_x]);
  __m256i r4 = _mm256_loadu_si256((const __m256i*)&tblock[4][pos_x]);
  __m256i r5 = _mm256_loadu_si256((const __m256i*)&tblock[5][pos_x]);
  __m256i r6 = _mm256_loadu_si256((const __m256i*)&tblock[6][pos_x]);
  __m256i r7 = _mm256_loadu_si256((const __m256i*)&tblock[7][pos_x]);

  transpose8x8_epi32(r0, r1, r2, r3, r4, r5, r6, r7);

  __m256i h0, h1, h2, h3, h4, h5, h6, h7;
  transform8_1d_epi32(r0, r1, r2, r3, r4, r5, r6, r7, h0, h1, h2, h3, h4, h5, h6, h7);

  transpose8x8_epi32(h0, h1, h2, h3, h4, h5, h6, h7);

  __m256i v0, v1, v2, v3, v4, v5, v6, v7;
  transform8_1d_epi32(h0, h1, h2, h3, h4, h5, h6, h7, v0, v1, v2, v3, v4, v5, v6, v7);

  _mm256_storeu_si256((__m256i*)&block[0][pos_x], v0);
  _mm256_storeu_si256((__m256i*)&block[1][pos_x], v1);
  _mm256_storeu_si256((__m256i*)&block[2][pos_x], v2);
  _mm256_storeu_si256((__m256i*)&block[3][pos_x], v3);
  _mm256_storeu_si256((__m256i*)&block[4][pos_x], v4);
  _mm256_storeu_si256((__m256i*)&block[5][pos_x], v5);
  _mm256_storeu_si256((__m256i*)&block[6][pos_x], v6);
  _mm256_storeu_si256((__m256i*)&block[7][pos_x], v7);
}

static void recon8x8_avx2(int **m7, imgpel **mb_rec, imgpel **mpr, int max_imgpel_value, int ioff)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    const __m256i v32 = _mm256_set1_epi32(32);
    for (int j = 0; j < 8; j++)
    {
      const int *m_tr = (*m7++) + ioff;
      imgpel *m_rec = (*mb_rec++) + ioff;
      const imgpel *m_prd = (*mpr++) + ioff;

      __m256i tr = _mm256_loadu_si256((const __m256i*)m_tr);
      tr = _mm256_srai_epi32(_mm256_add_epi32(tr, v32), 6);

      __m128i prd8 = _mm_loadl_epi64((const __m128i*)m_prd);
      __m256i prd32 = _mm256_cvtepu8_epi32(prd8);

      __m256i sum = _mm256_add_epi32(tr, prd32);

      __m128i lo = _mm256_castsi256_si128(sum);
      __m128i hi = _mm256_extracti128_si256(sum, 1);
      __m128i p16 = _mm_packs_epi32(lo, hi);
      __m128i p8  = _mm_packus_epi16(p16, p16);

      _mm_storel_epi64((__m128i*)m_rec, p8);
    }
  }
  else
  {
    recon8x8_generic(m7, mb_rec, mpr, max_imgpel_value, ioff);
  }
}

#endif // F264_ARCH_X86

int f264_strategy_register_transform_avx2(void *opaque, uint8_t bitdepth)
{
#if defined(F264_ARCH_X86)
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "inverse8x8", "avx2", 20, (void*)inverse8x8_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "recon8x8", "avx2", 20, (void*)recon8x8_avx2) != 0);
  return success ? 1 : 0;
#else
  return 1;
#endif
}
