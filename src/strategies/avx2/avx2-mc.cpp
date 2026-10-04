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

template <int dx_off, int dy_off>
static void get_luma_diag_qpel_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_16((const uint8_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i vpel = filter_ver_6tap_16(
            (const uint8_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu8(hpel, vpel));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8((const uint8_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i vpel = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu8(hpel, vpel));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8((const uint8_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i vpel = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m128i qpel = _mm_avg_epu8(hpel, vpel);
        *(uint32_t*)block[j] = (uint32_t)_mm_cvtsi128_si32(qpel);
      }
      return;
    }
  }
  if (dx_off == 0 && dy_off == 0) get_luma_11_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
  else if (dx_off == 0 && dy_off == 1) get_luma_13_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
  else if (dx_off == 1 && dy_off == 0) get_luma_31_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
  else get_luma_33_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_11_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  get_luma_diag_qpel_avx2<0, 0>(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_13_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  get_luma_diag_qpel_avx2<0, 1>(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_31_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  get_luma_diag_qpel_avx2<1, 0>(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_33_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  get_luma_diag_qpel_avx2<1, 1>(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

template <int dy_off>
static void get_luma_hpel_qpel_hor_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255 && (block_size_x == 16 || block_size_x == 8 || block_size_x == 4))
  {
    get_luma_22_avx2(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel20 = filter_6tap_16((const uint8_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i hpel22 = _mm_loadu_si128((const __m128i*)block[j]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu8(hpel22, hpel20));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel20 = filter_6tap_8((const uint8_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i hpel22 = _mm_loadl_epi64((const __m128i*)block[j]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu8(hpel22, hpel20));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel20 = filter_6tap_8((const uint8_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i hpel22 = _mm_cvtsi32_si128(*(const int32_t*)block[j]);
        *(uint32_t*)block[j] = (uint32_t)_mm_cvtsi128_si32(_mm_avg_epu8(hpel22, hpel20));
      }
      return;
    }
  }
  if (dy_off == 0) get_luma_21_generic(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
  else get_luma_23_generic(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_21_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  get_luma_hpel_qpel_hor_avx2<0>(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_23_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
  get_luma_hpel_qpel_hor_avx2<1>(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

template <int dx_off>
static void get_luma_hpel_qpel_ver_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  if (sizeof(imgpel) == 1 && max_imgpel_value == 255 && (block_size_x == 16 || block_size_x == 8 || block_size_x == 4))
  {
    get_luma_22_avx2(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel02 = filter_ver_6tap_16(
            (const uint8_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m128i hpel22 = _mm_loadu_si128((const __m128i*)block[j]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu8(hpel22, vpel02));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel02 = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m128i hpel22 = _mm_loadl_epi64((const __m128i*)block[j]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu8(hpel22, vpel02));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel02 = filter_ver_6tap_8(
            (const uint8_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint8_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m128i hpel22 = _mm_cvtsi32_si128(*(const int32_t*)block[j]);
        *(uint32_t*)block[j] = (uint32_t)_mm_cvtsi128_si32(_mm_avg_epu8(hpel22, vpel02));
      }
      return;
    }
  }
  if (dx_off == 0) get_luma_12_generic(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
  else get_luma_32_generic(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_12_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  get_luma_hpel_qpel_ver_avx2<0>(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_32_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
  get_luma_hpel_qpel_ver_avx2<1>(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void bi_prediction_avx2(imgpel **mb_pred, imgpel **block_l0, imgpel **block_l1, int block_size_y, int block_size_x, int ioff)
{
  if (sizeof(imgpel) == 1)
  {
    const uint8_t *b0 = (const uint8_t*)block_l0[0];
    const uint8_t *b1 = (const uint8_t*)block_l1[0];

    if (block_size_x == 16)
    {
      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        __m128i row0_l0 = _mm_loadu_si128((const __m128i*)&b0[(j + 0) * MB_BLOCK_SIZE]);
        __m128i row1_l0 = _mm_loadu_si128((const __m128i*)&b0[(j + 1) * MB_BLOCK_SIZE]);
        __m128i row0_l1 = _mm_loadu_si128((const __m128i*)&b1[(j + 0) * MB_BLOCK_SIZE]);
        __m128i row1_l1 = _mm_loadu_si128((const __m128i*)&b1[(j + 1) * MB_BLOCK_SIZE]);

        __m256i l0_256 = _mm256_set_m128i(row1_l0, row0_l0);
        __m256i l1_256 = _mm256_set_m128i(row1_l1, row0_l1);
        __m256i avg_256 = _mm256_avg_epu8(l0_256, l1_256);

        _mm_storeu_si128((__m128i*)&mb_pred[j + 0][ioff], _mm256_castsi256_si128(avg_256));
        _mm_storeu_si128((__m128i*)&mb_pred[j + 1][ioff], _mm256_extracti128_si256(avg_256, 1));
      }
      for (; j < block_size_y; j++)
      {
        __m128i row_l0 = _mm_loadu_si128((const __m128i*)&b0[j * MB_BLOCK_SIZE]);
        __m128i row_l1 = _mm_loadu_si128((const __m128i*)&b1[j * MB_BLOCK_SIZE]);
        _mm_storeu_si128((__m128i*)&mb_pred[j][ioff], _mm_avg_epu8(row_l0, row_l1));
      }
      return;
    }
    else if (block_size_x == 8)
    {
      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        __m128i row0_l0 = _mm_loadl_epi64((const __m128i*)&b0[(j + 0) * MB_BLOCK_SIZE]);
        __m128i row1_l0 = _mm_loadl_epi64((const __m128i*)&b0[(j + 1) * MB_BLOCK_SIZE]);
        __m128i row0_l1 = _mm_loadl_epi64((const __m128i*)&b1[(j + 0) * MB_BLOCK_SIZE]);
        __m128i row1_l1 = _mm_loadl_epi64((const __m128i*)&b1[(j + 1) * MB_BLOCK_SIZE]);

        __m128i l0_128 = _mm_unpacklo_epi64(row0_l0, row1_l0);
        __m128i l1_128 = _mm_unpacklo_epi64(row0_l1, row1_l1);
        __m128i avg_128 = _mm_avg_epu8(l0_128, l1_128);

        _mm_storel_epi64((__m128i*)&mb_pred[j + 0][ioff], avg_128);
        _mm_storeh_pd((double*)&mb_pred[j + 1][ioff], _mm_castsi128_pd(avg_128));
      }
      for (; j < block_size_y; j++)
      {
        __m128i row_l0 = _mm_loadl_epi64((const __m128i*)&b0[j * MB_BLOCK_SIZE]);
        __m128i row_l1 = _mm_loadl_epi64((const __m128i*)&b1[j * MB_BLOCK_SIZE]);
        _mm_storel_epi64((__m128i*)&mb_pred[j][ioff], _mm_avg_epu8(row_l0, row_l1));
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        int32_t val0 = *(const int32_t*)&b0[j * MB_BLOCK_SIZE];
        int32_t val1 = *(const int32_t*)&b1[j * MB_BLOCK_SIZE];
        __m128i a0 = _mm_cvtsi32_si128(val0);
        __m128i a1 = _mm_cvtsi32_si128(val1);
        __m128i avg = _mm_avg_epu8(a0, a1);
        *(int32_t*)&mb_pred[j][ioff] = _mm_cvtsi128_si32(avg);
      }
      return;
    }
  }
  bi_prediction_generic(mb_pred, block_l0, block_l1, block_size_y, block_size_x, ioff);
}

static void weighted_bi_prediction_avx2(imgpel *mb_pred, 
                                        imgpel *block_l0, 
                                        imgpel *block_l1, 
                                        int block_size_y, 
                                        int block_size_x, 
                                        int wp_scale_l0, 
                                        int wp_scale_l1, 
                                        int wp_offset, 
                                        int weight_denom, 
                                        int color_clip)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == 1)
  {
    const uint8_t *b0 = (const uint8_t*)block_l0;
    const uint8_t *b1 = (const uint8_t*)block_l1;
    uint8_t *dst = (uint8_t*)mb_pred;

    __m128i w = _mm_set1_epi32((uint16_t)wp_scale_l0 | ((uint32_t)(uint16_t)wp_scale_l1 << 16));
    __m128i rnd = _mm_set1_epi32(1 << (weight_denom - 1));
    __m128i off = _mm_set1_epi32(wp_offset);
    __m128i shift = _mm_cvtsi32_si128(weight_denom);
    __m128i clip_max = _mm_set1_epi8((uint8_t)color_clip);

    if (block_size_x == 16)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row0 = _mm_loadu_si128((const __m128i*)(b0 + j * MB_BLOCK_SIZE));
        __m128i row1 = _mm_loadu_si128((const __m128i*)(b1 + j * MB_BLOCK_SIZE));

        __m128i p0_lo = _mm_cvtepu8_epi16(row0);
        __m128i p1_lo = _mm_cvtepu8_epi16(row1);
        __m128i pair0_lo = _mm_unpacklo_epi16(p0_lo, p1_lo);
        __m128i pair1_lo = _mm_unpackhi_epi16(p0_lo, p1_lo);

        __m128i sum0_lo = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair0_lo, w), rnd), shift), off);
        __m128i sum1_lo = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair1_lo, w), rnd), shift), off);
        __m128i res16_lo = _mm_packs_epi32(sum0_lo, sum1_lo);
        __m128i res8_lo = _mm_packus_epi16(res16_lo, res16_lo);

        __m128i p0_hi = _mm_unpackhi_epi8(row0, _mm_setzero_si128());
        __m128i p1_hi = _mm_unpackhi_epi8(row1, _mm_setzero_si128());
        __m128i pair0_hi = _mm_unpacklo_epi16(p0_hi, p1_hi);
        __m128i pair1_hi = _mm_unpackhi_epi16(p0_hi, p1_hi);

        __m128i sum0_hi = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair0_hi, w), rnd), shift), off);
        __m128i sum1_hi = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair1_hi, w), rnd), shift), off);
        __m128i res16_hi = _mm_packs_epi32(sum0_hi, sum1_hi);
        __m128i res8_hi = _mm_packus_epi16(res16_hi, res16_hi);

        __m128i res8 = _mm_unpacklo_epi64(res8_lo, res8_hi);
        if (color_clip < 255)
        {
          res8 = _mm_min_epu8(res8, clip_max);
        }
        _mm_storeu_si128((__m128i*)(dst + j * MB_BLOCK_SIZE), res8);
      }
      return;
    }
    else if (block_size_x == 8)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row0 = _mm_loadl_epi64((const __m128i*)(b0 + j * MB_BLOCK_SIZE));
        __m128i row1 = _mm_loadl_epi64((const __m128i*)(b1 + j * MB_BLOCK_SIZE));

        __m128i p0 = _mm_cvtepu8_epi16(row0);
        __m128i p1 = _mm_cvtepu8_epi16(row1);
        __m128i pair0 = _mm_unpacklo_epi16(p0, p1);
        __m128i pair1 = _mm_unpackhi_epi16(p0, p1);

        __m128i sum0 = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair0, w), rnd), shift), off);
        __m128i sum1 = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair1, w), rnd), shift), off);
        __m128i res16 = _mm_packs_epi32(sum0, sum1);
        __m128i res8 = _mm_packus_epi16(res16, res16);
        if (color_clip < 255)
        {
          res8 = _mm_min_epu8(res8, clip_max);
        }
        _mm_storel_epi64((__m128i*)(dst + j * MB_BLOCK_SIZE), res8);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row0 = _mm_cvtsi32_si128(*(const int32_t*)(b0 + j * MB_BLOCK_SIZE));
        __m128i row1 = _mm_cvtsi32_si128(*(const int32_t*)(b1 + j * MB_BLOCK_SIZE));

        __m128i p0 = _mm_cvtepu8_epi16(row0);
        __m128i p1 = _mm_cvtepu8_epi16(row1);
        __m128i pair = _mm_unpacklo_epi16(p0, p1);

        __m128i sum = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), rnd), shift), off);
        __m128i res16 = _mm_packs_epi32(sum, sum);
        __m128i res8 = _mm_packus_epi16(res16, res16);
        if (color_clip < 255)
        {
          res8 = _mm_min_epu8(res8, clip_max);
        }
        *(int32_t*)(dst + j * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(res8);
      }
      return;
    }
  }
#endif
  weighted_bi_prediction_generic(mb_pred, block_l0, block_l1, block_size_y, block_size_x, wp_scale_l0, wp_scale_l1, wp_offset, weight_denom, color_clip);
}

void get_chroma_0X_avx2(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w01, int total_scale)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == 1)
  {
    const __m128i weights_0X = _mm_set1_epi16((short)((w01 << 8) | (w00 & 0xff)));
    const __m128i round_vec  = _mm_set1_epi16((short)(1 << (total_scale - 1)));

    if (block_size_x == 8)
    {
      const __m256i weights_0X256 = _mm256_set_m128i(weights_0X, weights_0X);
      const __m256i round_vec256  = _mm256_set_m128i(round_vec, round_vec);

      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        const imgpel *c0 = cur_img + (j + 0) * span;
        const imgpel *n0 = c0 + span;
        const imgpel *c1 = cur_img + (j + 1) * span;
        const imgpel *n1 = c1 + span;

        __m256i c_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)c1), _mm_loadu_si128((const __m128i*)c0));
        __m256i n_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)n1), _mm_loadu_si128((const __m128i*)n0));

        __m256i pairs_256 = _mm256_unpacklo_epi8(c_256, n_256);
        __m256i sum = _mm256_add_epi16(_mm256_maddubs_epi16(pairs_256, weights_0X256), round_vec256);
        __m256i res = _mm256_srai_epi16(sum, total_scale);
        __m256i packed = _mm256_packus_epi16(res, res);

        _mm_storel_epi64((__m128i*)(block + (j + 0) * MB_BLOCK_SIZE), _mm256_castsi256_si128(packed));
        _mm_storel_epi64((__m128i*)(block + (j + 1) * MB_BLOCK_SIZE), _mm256_extracti128_si256(packed, 1));
      }
      for (; j < block_size_y; j++)
      {
        const imgpel *c = cur_img + j * span;
        const imgpel *n = c + span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i nv = _mm_loadu_si128((const __m128i*)n);

        __m128i pairs = _mm_unpacklo_epi8(cv, nv);
        __m128i sum = _mm_add_epi16(_mm_maddubs_epi16(pairs, weights_0X), round_vec);
        __m128i res = _mm_srai_epi16(sum, total_scale);
        __m128i packed = _mm_packus_epi16(res, res);

        _mm_storel_epi64((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      const __m256i weights_0X256 = _mm256_set_m128i(weights_0X, weights_0X);
      const __m256i round_vec256  = _mm256_set_m128i(round_vec, round_vec);

      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        const imgpel *c0 = cur_img + (j + 0) * span;
        const imgpel *n0 = c0 + span;
        const imgpel *c1 = cur_img + (j + 1) * span;
        const imgpel *n1 = c1 + span;

        __m256i c_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)c1), _mm_loadu_si128((const __m128i*)c0));
        __m256i n_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)n1), _mm_loadu_si128((const __m128i*)n0));

        __m256i pairs_256 = _mm256_unpacklo_epi8(c_256, n_256);
        __m256i sum = _mm256_add_epi16(_mm256_maddubs_epi16(pairs_256, weights_0X256), round_vec256);
        __m256i res = _mm256_srai_epi16(sum, total_scale);
        __m256i packed = _mm256_packus_epi16(res, res);

        *(int32_t*)(block + (j + 0) * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm256_castsi256_si128(packed));
        *(int32_t*)(block + (j + 1) * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm256_extracti128_si256(packed, 1));
      }
      for (; j < block_size_y; j++)
      {
        const imgpel *c = cur_img + j * span;
        const imgpel *n = c + span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i nv = _mm_loadu_si128((const __m128i*)n);

        __m128i pairs = _mm_unpacklo_epi8(cv, nv);
        __m128i sum = _mm_add_epi16(_mm_maddubs_epi16(pairs, weights_0X), round_vec);
        __m128i res = _mm_srai_epi16(sum, total_scale);
        __m128i packed = _mm_packus_epi16(res, res);

        *(int32_t*)(block + j * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(packed);
      }
      return;
    }
  }
#endif
  get_chroma_0X_generic(block, cur_img, span, block_size_y, block_size_x, w00, w01, total_scale);
}

void get_chroma_X0_avx2(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w10, int total_scale)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == 1)
  {
    const __m128i weights_X0 = _mm_set1_epi16((short)((w10 << 8) | (w00 & 0xff)));
    const __m128i round_vec  = _mm_set1_epi16((short)(1 << (total_scale - 1)));

    if (block_size_x == 8)
    {
      const __m256i weights_X0256 = _mm256_set_m128i(weights_X0, weights_X0);
      const __m256i round_vec256  = _mm256_set_m128i(round_vec, round_vec);

      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        const imgpel *c0 = cur_img + (j + 0) * span;
        const imgpel *c1 = cur_img + (j + 1) * span;

        __m256i c_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)c1), _mm_loadu_si128((const __m128i*)c0));
        __m256i cs_256 = _mm256_srli_si256(c_256, 1);

        __m256i pairs_256 = _mm256_unpacklo_epi8(c_256, cs_256);
        __m256i sum = _mm256_add_epi16(_mm256_maddubs_epi16(pairs_256, weights_X0256), round_vec256);
        __m256i res = _mm256_srai_epi16(sum, total_scale);
        __m256i packed = _mm256_packus_epi16(res, res);

        _mm_storel_epi64((__m128i*)(block + (j + 0) * MB_BLOCK_SIZE), _mm256_castsi256_si128(packed));
        _mm_storel_epi64((__m128i*)(block + (j + 1) * MB_BLOCK_SIZE), _mm256_extracti128_si256(packed, 1));
      }
      for (; j < block_size_y; j++)
      {
        const imgpel *c = cur_img + j * span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i cs = _mm_srli_si128(cv, 1);

        __m128i pairs = _mm_unpacklo_epi8(cv, cs);
        __m128i sum = _mm_add_epi16(_mm_maddubs_epi16(pairs, weights_X0), round_vec);
        __m128i res = _mm_srai_epi16(sum, total_scale);
        __m128i packed = _mm_packus_epi16(res, res);

        _mm_storel_epi64((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      const __m256i weights_X0256 = _mm256_set_m128i(weights_X0, weights_X0);
      const __m256i round_vec256  = _mm256_set_m128i(round_vec, round_vec);

      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        const imgpel *c0 = cur_img + (j + 0) * span;
        const imgpel *c1 = cur_img + (j + 1) * span;

        __m256i c_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)c1), _mm_loadu_si128((const __m128i*)c0));
        __m256i cs_256 = _mm256_srli_si256(c_256, 1);

        __m256i pairs_256 = _mm256_unpacklo_epi8(c_256, cs_256);
        __m256i sum = _mm256_add_epi16(_mm256_maddubs_epi16(pairs_256, weights_X0256), round_vec256);
        __m256i res = _mm256_srai_epi16(sum, total_scale);
        __m256i packed = _mm256_packus_epi16(res, res);

        *(int32_t*)(block + (j + 0) * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm256_castsi256_si128(packed));
        *(int32_t*)(block + (j + 1) * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm256_extracti128_si256(packed, 1));
      }
      for (; j < block_size_y; j++)
      {
        const imgpel *c = cur_img + j * span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i cs = _mm_srli_si128(cv, 1);

        __m128i pairs = _mm_unpacklo_epi8(cv, cs);
        __m128i sum = _mm_add_epi16(_mm_maddubs_epi16(pairs, weights_X0), round_vec);
        __m128i res = _mm_srai_epi16(sum, total_scale);
        __m128i packed = _mm_packus_epi16(res, res);

        *(int32_t*)(block + j * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(packed);
      }
      return;
    }
  }
#endif
  get_chroma_X0_generic(block, cur_img, span, block_size_y, block_size_x, w00, w10, total_scale);
}

void get_chroma_XY_avx2(imgpel *block, imgpel *cur_img, int span, int block_size_y, int block_size_x, int w00, int w01, int w10, int w11, int total_scale)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == 1)
  {
    const __m128i weights_cur = _mm_set1_epi16((short)((w10 << 8) | (w00 & 0xff)));
    const __m128i weights_nxt = _mm_set1_epi16((short)((w11 << 8) | (w01 & 0xff)));
    const __m128i round_vec   = _mm_set1_epi16((short)(1 << (total_scale - 1)));

    if (block_size_x == 8)
    {
      const __m256i weights_cur256 = _mm256_set_m128i(weights_cur, weights_cur);
      const __m256i weights_nxt256 = _mm256_set_m128i(weights_nxt, weights_nxt);
      const __m256i round_vec256   = _mm256_set_m128i(round_vec, round_vec);

      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        const imgpel *c0 = cur_img + (j + 0) * span;
        const imgpel *n0 = c0 + span;
        const imgpel *c1 = cur_img + (j + 1) * span;
        const imgpel *n1 = c1 + span;

        __m256i c_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)c1), _mm_loadu_si128((const __m128i*)c0));
        __m256i n_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)n1), _mm_loadu_si128((const __m128i*)n0));

        __m256i cs_256 = _mm256_srli_si256(c_256, 1);
        __m256i ns_256 = _mm256_srli_si256(n_256, 1);

        __m256i pc_256 = _mm256_unpacklo_epi8(c_256, cs_256);
        __m256i pn_256 = _mm256_unpacklo_epi8(n_256, ns_256);

        __m256i t1 = _mm256_maddubs_epi16(pc_256, weights_cur256);
        __m256i t2 = _mm256_maddubs_epi16(pn_256, weights_nxt256);

        __m256i sum = _mm256_add_epi16(_mm256_add_epi16(t1, t2), round_vec256);
        __m256i res = _mm256_srai_epi16(sum, total_scale);
        __m256i packed = _mm256_packus_epi16(res, res);

        _mm_storel_epi64((__m128i*)(block + (j + 0) * MB_BLOCK_SIZE), _mm256_castsi256_si128(packed));
        _mm_storel_epi64((__m128i*)(block + (j + 1) * MB_BLOCK_SIZE), _mm256_extracti128_si256(packed, 1));
      }
      for (; j < block_size_y; j++)
      {
        const imgpel *c = cur_img + j * span;
        const imgpel *n = c + span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i nv = _mm_loadu_si128((const __m128i*)n);

        __m128i cs = _mm_srli_si128(cv, 1);
        __m128i ns = _mm_srli_si128(nv, 1);

        __m128i pc = _mm_unpacklo_epi8(cv, cs);
        __m128i pn = _mm_unpacklo_epi8(nv, ns);

        __m128i t1 = _mm_maddubs_epi16(pc, weights_cur);
        __m128i t2 = _mm_maddubs_epi16(pn, weights_nxt);

        __m128i sum = _mm_add_epi16(_mm_add_epi16(t1, t2), round_vec);
        __m128i res = _mm_srai_epi16(sum, total_scale);
        __m128i packed = _mm_packus_epi16(res, res);

        _mm_storel_epi64((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      const __m256i weights_cur256 = _mm256_set_m128i(weights_cur, weights_cur);
      const __m256i weights_nxt256 = _mm256_set_m128i(weights_nxt, weights_nxt);
      const __m256i round_vec256   = _mm256_set_m128i(round_vec, round_vec);

      int j = 0;
      for (; j + 1 < block_size_y; j += 2)
      {
        const imgpel *c0 = cur_img + (j + 0) * span;
        const imgpel *n0 = c0 + span;
        const imgpel *c1 = cur_img + (j + 1) * span;
        const imgpel *n1 = c1 + span;

        __m256i c_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)c1), _mm_loadu_si128((const __m128i*)c0));
        __m256i n_256 = _mm256_set_m128i(_mm_loadu_si128((const __m128i*)n1), _mm_loadu_si128((const __m128i*)n0));

        __m256i cs_256 = _mm256_srli_si256(c_256, 1);
        __m256i ns_256 = _mm256_srli_si256(n_256, 1);

        __m256i pc_256 = _mm256_unpacklo_epi8(c_256, cs_256);
        __m256i pn_256 = _mm256_unpacklo_epi8(n_256, ns_256);

        __m256i t1 = _mm256_maddubs_epi16(pc_256, weights_cur256);
        __m256i t2 = _mm256_maddubs_epi16(pn_256, weights_nxt256);

        __m256i sum = _mm256_add_epi16(_mm256_add_epi16(t1, t2), round_vec256);
        __m256i res = _mm256_srai_epi16(sum, total_scale);
        __m256i packed = _mm256_packus_epi16(res, res);

        *(int32_t*)(block + (j + 0) * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm256_castsi256_si128(packed));
        *(int32_t*)(block + (j + 1) * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm256_extracti128_si256(packed, 1));
      }
      for (; j < block_size_y; j++)
      {
        const imgpel *c = cur_img + j * span;
        const imgpel *n = c + span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i nv = _mm_loadu_si128((const __m128i*)n);

        __m128i cs = _mm_srli_si128(cv, 1);
        __m128i ns = _mm_srli_si128(nv, 1);

        __m128i pc = _mm_unpacklo_epi8(cv, cs);
        __m128i pn = _mm_unpacklo_epi8(nv, ns);

        __m128i t1 = _mm_maddubs_epi16(pc, weights_cur);
        __m128i t2 = _mm_maddubs_epi16(pn, weights_nxt);

        __m128i sum = _mm_add_epi16(_mm_add_epi16(t1, t2), round_vec);
        __m128i res = _mm_srai_epi16(sum, total_scale);
        __m128i packed = _mm_packus_epi16(res, res);

        *(int32_t*)(block + j * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(packed);
      }
      return;
    }
  }
#endif
  get_chroma_XY_generic(block, cur_img, span, block_size_y, block_size_x, w00, w01, w10, w11, total_scale);
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
  success &= (f264_strategyselector_register(opaque, "get_luma_11", "avx2", 20, (void*)get_luma_11_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_13", "avx2", 20, (void*)get_luma_13_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_31", "avx2", 20, (void*)get_luma_31_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_33", "avx2", 20, (void*)get_luma_33_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_22", "avx2", 20, (void*)get_luma_22_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_21", "avx2", 20, (void*)get_luma_21_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_23", "avx2", 20, (void*)get_luma_23_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_12", "avx2", 20, (void*)get_luma_12_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_luma_32", "avx2", 20, (void*)get_luma_32_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "bi_prediction", "avx2", 20, (void*)bi_prediction_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "weighted_bi_prediction", "avx2", 20, (void*)weighted_bi_prediction_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_chroma_0X", "avx2", 20, (void*)get_chroma_0X_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_chroma_X0", "avx2", 20, (void*)get_chroma_X0_avx2) != 0);
  success &= (f264_strategyselector_register(opaque, "get_chroma_XY", "avx2", 20, (void*)get_chroma_XY_avx2) != 0);
  return success ? 1 : 0;
#else
  return 1;
#endif
}
