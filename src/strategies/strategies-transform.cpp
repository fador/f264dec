/**
 * \file strategies-transform.cpp
 * \brief Generic and SIMD implementations of inverse transform and reconstruction.
 */

#include "strategies/strategies-transform.h"
#include "strategies/strategyselector.h"
#include <cstring>
#include <algorithm>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define F264_ARCH_X86 1
#endif

// Function pointer storage
f264_inverse4x4_func f264_inverse4x4 = nullptr;
f264_sample_recon_func f264_sample_reconstruct = nullptr;

// =========================================================================
// Generic Implementations
// =========================================================================

static void inverse4x4_generic(int **tblock, int **block, int pos_y, int pos_x)
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

static void sample_reconstruct_generic(imgpel **curImg, imgpel **mpr, int **mb_rres,
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

// =========================================================================
// SSE2 SIMD Implementations
// =========================================================================
#if defined(F264_ARCH_X86)

static void inverse4x4_sse2(int **tblock, int **block, int pos_y, int pos_x)
{
  // Load 4 rows of 4 ints (128-bit each)
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

  // Horizontal transform on transposed columns (which are the original rows)
  // p0 = c0 + c2; p1 = c0 - c2;
  __m128i p0 = _mm_add_epi32(c0, c2);
  __m128i p1 = _mm_sub_epi32(c0, c2);
  // p2 = (c1 >> 1) - c3; p3 = c1 + (c3 >> 1);
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

  // Vertical transform
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

  // Store 4 rows
  _mm_storeu_si128((__m128i*)&block[pos_y + 0][pos_x], v0);
  _mm_storeu_si128((__m128i*)&block[pos_y + 1][pos_x], v1);
  _mm_storeu_si128((__m128i*)&block[pos_y + 2][pos_x], v2);
  _mm_storeu_si128((__m128i*)&block[pos_y + 3][pos_x], v3);
}

static void sample_reconstruct_sse2(imgpel **curImg, imgpel **mpr, int **mb_rres,
                                    int mb_x, int opix_x, int width, int height,
                                    int max_imgpel_value, int dq_bits)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255 && dq_bits == 6 && (width % 4 == 0))
  {
    const __m128i offset = _mm_set1_epi32(1 << (6 - 1)); // 32
    const __m128i zero = _mm_setzero_si128();

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

        // Load 4 prediction bytes
        int pred4 = *(const int*)&imgPred[i];
        __m128i pred_vec = _mm_cvtsi32_si128(pred4);
        pred_vec = _mm_unpacklo_epi8(pred_vec, zero);
        pred_vec = _mm_unpacklo_epi16(pred_vec, zero); // 4 32-bit ints

        __m128i sum = _mm_add_epi32(res, pred_vec);

        // Pack 32-bit to 16-bit signed, then 16-bit to 8-bit unsigned with saturation
        __m128i packed16 = _mm_packs_epi32(sum, sum);
        __m128i packed8 = _mm_packus_epi16(packed16, packed16);

        int out4 = _mm_cvtsi128_si32(packed8);
        *(int*)&imgOrg[i] = out4;
      }
    }
  }
  else
  {
    sample_reconstruct_generic(curImg, mpr, mb_rres, mb_x, opix_x, width, height, max_imgpel_value, dq_bits);
  }
}

#endif // F264_ARCH_X86

// =========================================================================
// Strategy Registration
// =========================================================================

int f264_strategy_register_transform(void *opaque, uint8_t bitdepth)
{
  bool success = true;

  // Generic C++ implementations
  success &= (f264_strategyselector_register(opaque, "inverse4x4", "generic", 0, (void*)inverse4x4_generic) != 0);
  success &= (f264_strategyselector_register(opaque, "sample_reconstruct", "generic", 0, (void*)sample_reconstruct_generic) != 0);

#if defined(F264_ARCH_X86)
  if (f264_g_hardware_flags.intel_flags.sse2) {
    success &= (f264_strategyselector_register(opaque, "inverse4x4", "sse2", 10, (void*)inverse4x4_sse2) != 0);
    success &= (f264_strategyselector_register(opaque, "sample_reconstruct", "sse2", 10, (void*)sample_reconstruct_sse2) != 0);
  }
#endif

  return success ? 1 : 0;
}
