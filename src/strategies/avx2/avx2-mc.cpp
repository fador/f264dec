/**
 * \file avx2-mc.cpp
 * \brief AVX2 SIMD implementations of subpel motion compensation.
 */

#include "strategies/avx2/avx2-mc.h"
#include "strategies/generic/generic-mc.h"
#include "strategies/strategyselector.h"
#include "global.h"
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

// 6-tap filter on 16 8-bit samples: (p0 + p5) - 5*(p1 + p4) + 20*(p2 + p3)
static inline __m128i filter_6tap_16(const uint8_t *src_p0)
{
  __m128i p0_8 = _mm_loadu_si128((const __m128i*)(src_p0 + 0));
  __m128i p1_8 = _mm_loadu_si128((const __m128i*)(src_p0 + 1));
  __m128i p2_8 = _mm_loadu_si128((const __m128i*)(src_p0 + 2));
  __m128i p3_8 = _mm_loadu_si128((const __m128i*)(src_p0 + 3));
  __m128i p4_8 = _mm_loadu_si128((const __m128i*)(src_p0 + 4));
  __m128i p5_8 = _mm_loadu_si128((const __m128i*)(src_p0 + 5));

  __m256i p0_16 = _mm256_cvtepu8_epi16(p0_8);
  __m256i p1_16 = _mm256_cvtepu8_epi16(p1_8);
  __m256i p2_16 = _mm256_cvtepu8_epi16(p2_8);
  __m256i p3_16 = _mm256_cvtepu8_epi16(p3_8);
  __m256i p4_16 = _mm256_cvtepu8_epi16(p4_8);
  __m256i p5_16 = _mm256_cvtepu8_epi16(p5_8);

  __m256i sum05 = _mm256_add_epi16(p0_16, p5_16);
  __m256i sum14 = _mm256_add_epi16(p1_16, p4_16);
  __m256i sum23 = _mm256_add_epi16(p2_16, p3_16);

  __m256i mul20 = _mm256_mullo_epi16(sum23, _mm256_set1_epi16(20));
  __m256i mul5  = _mm256_mullo_epi16(sum14, _mm256_set1_epi16(5));
  __m256i res   = _mm256_add_epi16(_mm256_sub_epi16(sum05, mul5), mul20);

  res = _mm256_srai_epi16(_mm256_add_epi16(res, _mm256_set1_epi16(16)), 5);

  __m128i lo = _mm256_castsi256_si128(res);
  __m128i hi = _mm256_extracti128_si256(res, 1);
  return _mm_packus_epi16(lo, hi);
}

// 6-tap filter on 8 8-bit samples
static inline __m128i filter_6tap_8(const uint8_t *src_p0)
{
  __m128i p0_8 = _mm_loadl_epi64((const __m128i*)(src_p0 + 0));
  __m128i p1_8 = _mm_loadl_epi64((const __m128i*)(src_p0 + 1));
  __m128i p2_8 = _mm_loadl_epi64((const __m128i*)(src_p0 + 2));
  __m128i p3_8 = _mm_loadl_epi64((const __m128i*)(src_p0 + 3));
  __m128i p4_8 = _mm_loadl_epi64((const __m128i*)(src_p0 + 4));
  __m128i p5_8 = _mm_loadl_epi64((const __m128i*)(src_p0 + 5));

  __m128i p0_16 = _mm_cvtepu8_epi16(p0_8);
  __m128i p1_16 = _mm_cvtepu8_epi16(p1_8);
  __m128i p2_16 = _mm_cvtepu8_epi16(p2_8);
  __m128i p3_16 = _mm_cvtepu8_epi16(p3_8);
  __m128i p4_16 = _mm_cvtepu8_epi16(p4_8);
  __m128i p5_16 = _mm_cvtepu8_epi16(p5_8);

  __m256i p0_256 = _mm256_castsi128_si256(p0_16);
  // Just use 128-bit operations for 8 samples
  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  __m128i res   = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

  res = _mm_srai_epi16(_mm_add_epi16(res, _mm_set1_epi16(16)), 5);
  return _mm_packus_epi16(res, res);
}

static void get_luma_20_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i out16 = filter_6tap_16((const uint8_t*)&cur_imgY[j][x_pos - 2]);
        _mm_storeu_si128((__m128i*)block[j], out16);
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i out8 = filter_6tap_8((const uint8_t*)&cur_imgY[j][x_pos - 2]);
        _mm_storel_epi64((__m128i*)block[j], out8);
      }
      return;
    }
  }
  get_luma_20_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_10_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_16((const uint8_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j][x_pos]);
        __m128i qpel = _mm_avg_epu8(hpel, ipel);
        _mm_storeu_si128((__m128i*)block[j], qpel);
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8((const uint8_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j][x_pos]);
        __m128i qpel = _mm_avg_epu8(hpel, ipel);
        _mm_storel_epi64((__m128i*)block[j], qpel);
      }
      return;
    }
  }
  get_luma_10_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_30_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_16((const uint8_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j][x_pos + 1]);
        __m128i qpel = _mm_avg_epu8(hpel, ipel);
        _mm_storeu_si128((__m128i*)block[j], qpel);
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8((const uint8_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j][x_pos + 1]);
        __m128i qpel = _mm_avg_epu8(hpel, ipel);
        _mm_storel_epi64((__m128i*)block[j], qpel);
      }
      return;
    }
  }
  get_luma_30_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static inline __m128i filter_ver_6tap_16(const uint8_t *p0, const uint8_t *p1, const uint8_t *p2,
                                         const uint8_t *p3, const uint8_t *p4, const uint8_t *p5)
{
  __m256i p0_16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)p0));
  __m256i p1_16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)p1));
  __m256i p2_16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)p2));
  __m256i p3_16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)p3));
  __m256i p4_16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)p4));
  __m256i p5_16 = _mm256_cvtepu8_epi16(_mm_loadu_si128((const __m128i*)p5));

  __m256i sum05 = _mm256_add_epi16(p0_16, p5_16);
  __m256i sum14 = _mm256_add_epi16(p1_16, p4_16);
  __m256i sum23 = _mm256_add_epi16(p2_16, p3_16);

  __m256i mul20 = _mm256_mullo_epi16(sum23, _mm256_set1_epi16(20));
  __m256i mul5  = _mm256_mullo_epi16(sum14, _mm256_set1_epi16(5));
  __m256i res   = _mm256_add_epi16(_mm256_sub_epi16(sum05, mul5), mul20);

  res = _mm256_srai_epi16(_mm256_add_epi16(res, _mm256_set1_epi16(16)), 5);

  __m128i lo = _mm256_castsi256_si128(res);
  __m128i hi = _mm256_extracti128_si256(res, 1);
  return _mm_packus_epi16(lo, hi);
}

static inline __m128i filter_ver_6tap_8(const uint8_t *p0, const uint8_t *p1, const uint8_t *p2,
                                        const uint8_t *p3, const uint8_t *p4, const uint8_t *p5)
{
  __m128i p0_16 = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)p0));
  __m128i p1_16 = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)p1));
  __m128i p2_16 = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)p2));
  __m128i p3_16 = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)p3));
  __m128i p4_16 = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)p4));
  __m128i p5_16 = _mm_cvtepu8_epi16(_mm_loadl_epi64((const __m128i*)p5));

  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  __m128i res   = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

  res = _mm_srai_epi16(_mm_add_epi16(res, _mm_set1_epi16(16)), 5);
  return _mm_packus_epi16(res, res);
}

static void get_luma_02_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i out16 = filter_ver_6tap_16(
            (const uint8_t*)&cur_imgY[j - 2][x_pos],
            (const uint8_t*)&cur_imgY[j - 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 0][x_pos],
            (const uint8_t*)&cur_imgY[j + 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 2][x_pos],
            (const uint8_t*)&cur_imgY[j + 3][x_pos]);
        _mm_storeu_si128((__m128i*)block[j], out16);
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i out8 = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos],
            (const uint8_t*)&cur_imgY[j - 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 0][x_pos],
            (const uint8_t*)&cur_imgY[j + 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 2][x_pos],
            (const uint8_t*)&cur_imgY[j + 3][x_pos]);
        _mm_storel_epi64((__m128i*)block[j], out8);
      }
      return;
    }
  }
  get_luma_02_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_01_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_16(
            (const uint8_t*)&cur_imgY[j - 2][x_pos],
            (const uint8_t*)&cur_imgY[j - 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 0][x_pos],
            (const uint8_t*)&cur_imgY[j + 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 2][x_pos],
            (const uint8_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j][x_pos]);
        __m128i qpel = _mm_avg_epu8(vpel, ipel);
        _mm_storeu_si128((__m128i*)block[j], qpel);
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos],
            (const uint8_t*)&cur_imgY[j - 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 0][x_pos],
            (const uint8_t*)&cur_imgY[j + 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 2][x_pos],
            (const uint8_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j][x_pos]);
        __m128i qpel = _mm_avg_epu8(vpel, ipel);
        _mm_storel_epi64((__m128i*)block[j], qpel);
      }
      return;
    }
  }
  get_luma_01_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_03_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_16(
            (const uint8_t*)&cur_imgY[j - 2][x_pos],
            (const uint8_t*)&cur_imgY[j - 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 0][x_pos],
            (const uint8_t*)&cur_imgY[j + 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 2][x_pos],
            (const uint8_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j + 1][x_pos]);
        __m128i qpel = _mm_avg_epu8(vpel, ipel);
        _mm_storeu_si128((__m128i*)block[j], qpel);
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos],
            (const uint8_t*)&cur_imgY[j - 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 0][x_pos],
            (const uint8_t*)&cur_imgY[j + 1][x_pos],
            (const uint8_t*)&cur_imgY[j + 2][x_pos],
            (const uint8_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j + 1][x_pos]);
        __m128i qpel = _mm_avg_epu8(vpel, ipel);
        _mm_storel_epi64((__m128i*)block[j], qpel);
      }
      return;
    }
  }
  get_luma_03_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_22_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255 && (block_size_x == 16 || block_size_x == 8))
  {
    int jj = -2;
    // Step 1: horizontal 6-tap into tmp_res (without division by 32)
    for (int j = 0; j < block_size_y + 5; j++)
    {
      const uint8_t *p0_ptr = (const uint8_t*)&cur_imgY[jj++][x_pos - 2];
      int *tmp_line = tmp_res[j];

      for (int i = 0; i < block_size_x; i += 8)
      {
        __m128i p0_8 = _mm_loadl_epi64((const __m128i*)(p0_ptr + i + 0));
        __m128i p1_8 = _mm_loadl_epi64((const __m128i*)(p0_ptr + i + 1));
        __m128i p2_8 = _mm_loadl_epi64((const __m128i*)(p0_ptr + i + 2));
        __m128i p3_8 = _mm_loadl_epi64((const __m128i*)(p0_ptr + i + 3));
        __m128i p4_8 = _mm_loadl_epi64((const __m128i*)(p0_ptr + i + 4));
        __m128i p5_8 = _mm_loadl_epi64((const __m128i*)(p0_ptr + i + 5));

        __m128i p0_16 = _mm_cvtepu8_epi16(p0_8);
        __m128i p1_16 = _mm_cvtepu8_epi16(p1_8);
        __m128i p2_16 = _mm_cvtepu8_epi16(p2_8);
        __m128i p3_16 = _mm_cvtepu8_epi16(p3_8);
        __m128i p4_16 = _mm_cvtepu8_epi16(p4_8);
        __m128i p5_16 = _mm_cvtepu8_epi16(p5_8);

        __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
        __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
        __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

        __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
        __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
        __m128i res16 = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

        __m256i res32 = _mm256_cvtepi16_epi32(res16);
        _mm256_storeu_si256((__m256i*)&tmp_line[i], res32);
      }
    }

    // Step 2: vertical 6-tap across 6 rows of tmp_res
    const __m256i v512 = _mm256_set1_epi32(512);
    const __m256i c20  = _mm256_set1_epi32(20);
    const __m256i c5   = _mm256_set1_epi32(5);

    for (int j = 0; j < block_size_y; j++)
    {
      imgpel *orig_line = block[j];
      for (int i = 0; i < block_size_x; i += 8)
      {
        __m256i x0 = _mm256_loadu_si256((const __m256i*)&tmp_res[j + 0][i]);
        __m256i x1 = _mm256_loadu_si256((const __m256i*)&tmp_res[j + 1][i]);
        __m256i x2 = _mm256_loadu_si256((const __m256i*)&tmp_res[j + 2][i]);
        __m256i x3 = _mm256_loadu_si256((const __m256i*)&tmp_res[j + 3][i]);
        __m256i x4 = _mm256_loadu_si256((const __m256i*)&tmp_res[j + 4][i]);
        __m256i x5 = _mm256_loadu_si256((const __m256i*)&tmp_res[j + 5][i]);

        __m256i sum05 = _mm256_add_epi32(x0, x5);
        __m256i sum14 = _mm256_add_epi32(x1, x4);
        __m256i sum23 = _mm256_add_epi32(x2, x3);

        __m256i mul20 = _mm256_mullo_epi32(sum23, c20);
        __m256i mul5  = _mm256_mullo_epi32(sum14, c5);
        __m256i res   = _mm256_add_epi32(_mm256_sub_epi32(sum05, mul5), mul20);

        res = _mm256_srai_epi32(_mm256_add_epi32(res, v512), 10);

        __m128i r_lo = _mm256_castsi256_si128(res);
        __m128i r_hi = _mm256_extracti128_si256(res, 1);
        __m128i p16 = _mm_packs_epi32(r_lo, r_hi);
        __m128i p8  = _mm_packus_epi16(p16, p16);

        _mm_storel_epi64((__m128i*)&orig_line[i], p8);
      }
    }
    return;
  }
  get_luma_22_generic(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

#endif // F264_ARCH_X86

int f264_strategy_register_mc_avx2(void *opaque, uint8_t bitdepth)
{
#if defined(F264_ARCH_X86)
  bool success = true;
  success &= (f264_strategyselector_register(opaque, "get_luma_20", "avx2", 20, (void*)get_luma_20_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_10", "avx2", 20, (void*)get_luma_10_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_30", "avx2", 20, (void*)get_luma_30_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_02", "avx2", 20, (void*)get_luma_02_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_01", "avx2", 20, (void*)get_luma_01_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_03", "avx2", 20, (void*)get_luma_03_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_22", "avx2", 20, (void*)get_luma_22_avx2) != 0);
  return success ? 1 : 0;
#else
  return 1;
#endif
}
