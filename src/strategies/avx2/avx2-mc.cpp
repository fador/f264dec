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

/// 6-tap filter on 16 16-bit samples: (p0 + p5) - 5*(p1 + p4) + 20*(p2 + p3)
static inline __m256i filter_6tap_16_u16(const uint16_t *src_p0)
{
  __m256i p0_16 = _mm256_loadu_si256((const __m256i*)(src_p0 + 0));
  __m256i p1_16 = _mm256_loadu_si256((const __m256i*)(src_p0 + 1));
  __m256i p2_16 = _mm256_loadu_si256((const __m256i*)(src_p0 + 2));
  __m256i p3_16 = _mm256_loadu_si256((const __m256i*)(src_p0 + 3));
  __m256i p4_16 = _mm256_loadu_si256((const __m256i*)(src_p0 + 4));
  __m256i p5_16 = _mm256_loadu_si256((const __m256i*)(src_p0 + 5));

  __m256i sum05 = _mm256_add_epi16(p0_16, p5_16);
  __m256i sum14 = _mm256_add_epi16(p1_16, p4_16);
  __m256i sum23 = _mm256_add_epi16(p2_16, p3_16);

  __m256i mul20 = _mm256_mullo_epi16(sum23, _mm256_set1_epi16(20));
  __m256i mul5  = _mm256_mullo_epi16(sum14, _mm256_set1_epi16(5));
  __m256i res   = _mm256_add_epi16(_mm256_sub_epi16(sum05, mul5), mul20);

  res = _mm256_srai_epi16(_mm256_add_epi16(res, _mm256_set1_epi16(16)), 5);
  return _mm256_min_epu16(_mm256_max_epi16(res, _mm256_setzero_si256()), _mm256_set1_epi16(255));
}

// 6-tap filter on 8 16-bit samples
static inline __m128i filter_6tap_8_u16(const uint16_t *src_p0)
{
  __m128i p0_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 0));
  __m128i p1_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 1));
  __m128i p2_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 2));
  __m128i p3_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 3));
  __m128i p4_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 4));
  __m128i p5_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 5));

  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  __m128i res   = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

  res = _mm_srai_epi16(_mm_add_epi16(res, _mm_set1_epi16(16)), 5);
  return _mm_min_epu16(_mm_max_epi16(res, _mm_setzero_si128()), _mm_set1_epi16(255));
}

// 6-tap filter on 4 16-bit samples
static inline __m128i filter_6tap_4_u16(const uint16_t *src_p0)
{
  __m128i p0_16 = _mm_loadl_epi64((const __m128i*)(src_p0 + 0));
  __m128i p1_16 = _mm_loadl_epi64((const __m128i*)(src_p0 + 1));
  __m128i p2_16 = _mm_loadl_epi64((const __m128i*)(src_p0 + 2));
  __m128i p3_16 = _mm_loadl_epi64((const __m128i*)(src_p0 + 3));
  __m128i p4_16 = _mm_loadl_epi64((const __m128i*)(src_p0 + 4));
  __m128i p5_16 = _mm_loadl_epi64((const __m128i*)(src_p0 + 5));

  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  __m128i res   = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

  res = _mm_srai_epi16(_mm_add_epi16(res, _mm_set1_epi16(16)), 5);
  return _mm_min_epu16(_mm_max_epi16(res, _mm_setzero_si128()), _mm_set1_epi16(255));
}

// Raw horizontal 6-tap filter on 8 16-bit samples (without shift/clamp, for intermediate tmp_res)
static inline __m128i filter_6tap_raw_8_u16(const uint16_t *src_p0)
{
  __m128i p0_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 0));
  __m128i p1_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 1));
  __m128i p2_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 2));
  __m128i p3_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 3));
  __m128i p4_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 4));
  __m128i p5_16 = _mm_loadu_si128((const __m128i*)(src_p0 + 5));

  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  return _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);
}

// 6-tap vertical filter on 16 16-bit samples
static inline __m256i filter_ver_6tap_16_u16(const uint16_t *p0, const uint16_t *p1, const uint16_t *p2,
                                             const uint16_t *p3, const uint16_t *p4, const uint16_t *p5)
{
  __m256i p0_16 = _mm256_loadu_si256((const __m256i*)p0);
  __m256i p1_16 = _mm256_loadu_si256((const __m256i*)p1);
  __m256i p2_16 = _mm256_loadu_si256((const __m256i*)p2);
  __m256i p3_16 = _mm256_loadu_si256((const __m256i*)p3);
  __m256i p4_16 = _mm256_loadu_si256((const __m256i*)p4);
  __m256i p5_16 = _mm256_loadu_si256((const __m256i*)p5);

  __m256i sum05 = _mm256_add_epi16(p0_16, p5_16);
  __m256i sum14 = _mm256_add_epi16(p1_16, p4_16);
  __m256i sum23 = _mm256_add_epi16(p2_16, p3_16);

  __m256i mul20 = _mm256_mullo_epi16(sum23, _mm256_set1_epi16(20));
  __m256i mul5  = _mm256_mullo_epi16(sum14, _mm256_set1_epi16(5));
  __m256i res   = _mm256_add_epi16(_mm256_sub_epi16(sum05, mul5), mul20);

  res = _mm256_srai_epi16(_mm256_add_epi16(res, _mm256_set1_epi16(16)), 5);
  return _mm256_min_epu16(_mm256_max_epi16(res, _mm256_setzero_si256()), _mm256_set1_epi16(255));
}

// 6-tap vertical filter on 8 16-bit samples
static inline __m128i filter_ver_6tap_8_u16(const uint16_t *p0, const uint16_t *p1, const uint16_t *p2,
                                            const uint16_t *p3, const uint16_t *p4, const uint16_t *p5)
{
  __m128i p0_16 = _mm_loadu_si128((const __m128i*)p0);
  __m128i p1_16 = _mm_loadu_si128((const __m128i*)p1);
  __m128i p2_16 = _mm_loadu_si128((const __m128i*)p2);
  __m128i p3_16 = _mm_loadu_si128((const __m128i*)p3);
  __m128i p4_16 = _mm_loadu_si128((const __m128i*)p4);
  __m128i p5_16 = _mm_loadu_si128((const __m128i*)p5);

  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  __m128i res   = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

  res = _mm_srai_epi16(_mm_add_epi16(res, _mm_set1_epi16(16)), 5);
  return _mm_min_epu16(_mm_max_epi16(res, _mm_setzero_si128()), _mm_set1_epi16(255));
}

// 6-tap vertical filter on 4 16-bit samples
static inline __m128i filter_ver_6tap_4_u16(const uint16_t *p0, const uint16_t *p1, const uint16_t *p2,
                                            const uint16_t *p3, const uint16_t *p4, const uint16_t *p5)
{
  __m128i p0_16 = _mm_loadl_epi64((const __m128i*)p0);
  __m128i p1_16 = _mm_loadl_epi64((const __m128i*)p1);
  __m128i p2_16 = _mm_loadl_epi64((const __m128i*)p2);
  __m128i p3_16 = _mm_loadl_epi64((const __m128i*)p3);
  __m128i p4_16 = _mm_loadl_epi64((const __m128i*)p4);
  __m128i p5_16 = _mm_loadl_epi64((const __m128i*)p5);

  __m128i sum05 = _mm_add_epi16(p0_16, p5_16);
  __m128i sum14 = _mm_add_epi16(p1_16, p4_16);
  __m128i sum23 = _mm_add_epi16(p2_16, p3_16);

  __m128i mul20 = _mm_mullo_epi16(sum23, _mm_set1_epi16(20));
  __m128i mul5  = _mm_mullo_epi16(sum14, _mm_set1_epi16(5));
  __m128i res   = _mm_add_epi16(_mm_sub_epi16(sum05, mul5), mul20);

  res = _mm_srai_epi16(_mm_add_epi16(res, _mm_set1_epi16(16)), 5);
  return _mm_min_epu16(_mm_max_epi16(res, _mm_setzero_si128()), _mm_set1_epi16(255));
}

static void get_luma_20_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        _mm256_storeu_si256((__m256i*)block[j], filter_6tap_16_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        _mm_storeu_si128((__m128i*)block[j], filter_6tap_8_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        _mm_storel_epi64((__m128i*)block[j], filter_6tap_4_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]));
      }
      return;
    }
  }
#endif
  get_luma_20_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_10_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i hpel = filter_6tap_16_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]);
        __m256i ipel = _mm256_loadu_si256((const __m256i*)&cur_imgY[j][x_pos]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(hpel, ipel));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j][x_pos]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(hpel, ipel));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_4_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j][x_pos]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(hpel, ipel));
      }
      return;
    }
  }
#endif
  get_luma_10_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_30_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i hpel = filter_6tap_16_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]);
        __m256i ipel = _mm256_loadu_si256((const __m256i*)&cur_imgY[j][x_pos + 1]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(hpel, ipel));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j][x_pos + 1]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(hpel, ipel));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_4_u16((const uint16_t*)&cur_imgY[j][x_pos - 2]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j][x_pos + 1]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(hpel, ipel));
      }
      return;
    }
  }
#endif
  get_luma_30_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

static void get_luma_02_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      __m256i p0 = _mm256_loadu_si256((const __m256i*)&cur_imgY[-2][x_pos]);
      __m256i p1 = _mm256_loadu_si256((const __m256i*)&cur_imgY[-1][x_pos]);
      __m256i p2 = _mm256_loadu_si256((const __m256i*)&cur_imgY[0][x_pos]);
      __m256i p3 = _mm256_loadu_si256((const __m256i*)&cur_imgY[1][x_pos]);
      __m256i p4 = _mm256_loadu_si256((const __m256i*)&cur_imgY[2][x_pos]);
      __m256i c20 = _mm256_set1_epi16(20);
      __m256i c5 = _mm256_set1_epi16(5);
      __m256i c16 = _mm256_set1_epi16(16);
      __m256i c255 = _mm256_set1_epi16(255);
      __m256i czero = _mm256_setzero_si256();

      for (int j = 0; j < block_size_y; j++) {
        __m256i p5 = _mm256_loadu_si256((const __m256i*)&cur_imgY[j + 3][x_pos]);
        __m256i sum05 = _mm256_add_epi16(p0, p5);
        __m256i sum14 = _mm256_add_epi16(p1, p4);
        __m256i sum23 = _mm256_add_epi16(p2, p3);

        __m256i mul20 = _mm256_mullo_epi16(sum23, c20);
        __m256i mul5  = _mm256_mullo_epi16(sum14, c5);
        __m256i res   = _mm256_add_epi16(_mm256_sub_epi16(sum05, mul5), mul20);

        res = _mm256_srai_epi16(_mm256_add_epi16(res, c16), 5);
        __m256i out = _mm256_min_epu16(_mm256_max_epi16(res, czero), c255);
        _mm256_storeu_si256((__m256i*)block[j], out);

        p0 = p1; p1 = p2; p2 = p3; p3 = p4; p4 = p5;
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i out = filter_ver_6tap_8_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        _mm_storeu_si128((__m128i*)block[j], out);
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i out = filter_ver_6tap_4_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        _mm_storel_epi64((__m128i*)block[j], out);
      }
      return;
    }
  }
#endif
  get_luma_02_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_01_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i vpel = filter_ver_6tap_16_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        __m256i ipel = _mm256_loadu_si256((const __m256i*)&cur_imgY[j][x_pos]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(vpel, ipel));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_8_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j][x_pos]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(vpel, ipel));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_4_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j][x_pos]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(vpel, ipel));
      }
      return;
    }
  }
#endif
  get_luma_01_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_03_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i vpel = filter_ver_6tap_16_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        __m256i ipel = _mm256_loadu_si256((const __m256i*)&cur_imgY[j + 1][x_pos]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(vpel, ipel));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_8_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadu_si128((const __m128i*)&cur_imgY[j + 1][x_pos]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(vpel, ipel));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel = filter_ver_6tap_4_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos],
            (const uint16_t*)&cur_imgY[j - 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 0][x_pos],
            (const uint16_t*)&cur_imgY[j + 1][x_pos],
            (const uint16_t*)&cur_imgY[j + 2][x_pos],
            (const uint16_t*)&cur_imgY[j + 3][x_pos]);
        __m128i ipel = _mm_loadl_epi64((const __m128i*)&cur_imgY[j + 1][x_pos]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(vpel, ipel));
      }
      return;
    }
  }
#endif
  get_luma_03_generic(block, cur_imgY, block_size_y, block_size_x, x_pos, shift_x, max_imgpel_value);
}

static void get_luma_22_avx2(imgpel **block, imgpel **cur_imgY, int **tmp_res, int block_size_y, int block_size_x, int x_pos, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255 && (block_size_x == 16 || block_size_x == 8 || block_size_x == 4))
  {
    int jj = -2;
    // Step 1: horizontal 6-tap into tmp_res (without division by 32)
    for (int j = 0; j < block_size_y + 5; j++)
    {
      const uint16_t *p0_ptr = (const uint16_t*)&cur_imgY[jj++][x_pos - 2];
      int *tmp_line = tmp_res[j];

      for (int i = 0; i < block_size_x; i += 8)
      {
        __m128i res16 = filter_6tap_raw_8_u16(p0_ptr + i);
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
        __m128i p16 = _mm_min_epu16(_mm_packus_epi32(r_lo, r_hi), _mm_set1_epi16(255));

        if (block_size_x == 4) {
          _mm_storel_epi64((__m128i*)&orig_line[i], p16);
          break;
        } else {
          _mm_storeu_si128((__m128i*)&orig_line[i], p16);
        }
      }
    }
    return;
  }
#endif
  get_luma_22_generic(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
}

template <int dx_off, int dy_off>
static void get_luma_diag_qpel_avx2(imgpel **block, imgpel **cur_imgY, int block_size_y, int block_size_x, int x_pos, int shift_x, int max_imgpel_value)
{
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255)
  {
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i hpel = filter_6tap_16_u16((const uint16_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m256i vpel = filter_ver_6tap_16_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(hpel, vpel));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_8_u16((const uint16_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i vpel = filter_ver_6tap_8_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(hpel, vpel));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel = filter_6tap_4_u16((const uint16_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i vpel = filter_ver_6tap_4_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(hpel, vpel));
      }
      return;
    }
  }
#endif
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
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255 && (block_size_x == 16 || block_size_x == 8 || block_size_x == 4))
  {
    get_luma_22_avx2(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i hpel20 = filter_6tap_16_u16((const uint16_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m256i hpel22 = _mm256_loadu_si256((const __m256i*)block[j]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(hpel22, hpel20));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel20 = filter_6tap_8_u16((const uint16_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i hpel22 = _mm_loadu_si128((const __m128i*)block[j]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(hpel22, hpel20));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i hpel20 = filter_6tap_4_u16((const uint16_t*)&cur_imgY[j + dy_off][x_pos - 2]);
        __m128i hpel22 = _mm_loadl_epi64((const __m128i*)block[j]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(hpel22, hpel20));
      }
      return;
    }
  }
#endif
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
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t) && max_imgpel_value == 255 && (block_size_x == 16 || block_size_x == 8 || block_size_x == 4))
  {
    get_luma_22_avx2(block, cur_imgY, tmp_res, block_size_y, block_size_x, x_pos, max_imgpel_value);
    if (block_size_x == 16) {
      for (int j = 0; j < block_size_y; j++) {
        __m256i vpel02 = filter_ver_6tap_16_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m256i hpel22 = _mm256_loadu_si256((const __m256i*)block[j]);
        _mm256_storeu_si256((__m256i*)block[j], _mm256_avg_epu16(hpel22, vpel02));
      }
      return;
    } else if (block_size_x == 8) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel02 = filter_ver_6tap_8_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m128i hpel22 = _mm_loadu_si128((const __m128i*)block[j]);
        _mm_storeu_si128((__m128i*)block[j], _mm_avg_epu16(hpel22, vpel02));
      }
      return;
    } else if (block_size_x == 4) {
      for (int j = 0; j < block_size_y; j++) {
        __m128i vpel02 = filter_ver_6tap_4_u16(
            (const uint16_t*)&cur_imgY[j - 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j - 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 0][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 1][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 2][x_pos + dx_off],
            (const uint16_t*)&cur_imgY[j + 3][x_pos + dx_off]);
        __m128i hpel22 = _mm_loadl_epi64((const __m128i*)block[j]);
        _mm_storel_epi64((__m128i*)block[j], _mm_avg_epu16(hpel22, vpel02));
      }
      return;
    }
  }
#endif
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
#if defined(F264_ARCH_X86)
  if (sizeof(imgpel) == sizeof(uint16_t))
  {
    const uint16_t *b0 = (const uint16_t*)block_l0[0];
    const uint16_t *b1 = (const uint16_t*)block_l1[0];

    if (block_size_x == 16)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m256i row_l0 = _mm256_loadu_si256((const __m256i*)&b0[j * MB_BLOCK_SIZE]);
        __m256i row_l1 = _mm256_loadu_si256((const __m256i*)&b1[j * MB_BLOCK_SIZE]);
        _mm256_storeu_si256((__m256i*)&mb_pred[j][ioff], _mm256_avg_epu16(row_l0, row_l1));
      }
      return;
    }
    else if (block_size_x == 8)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row_l0 = _mm_loadu_si128((const __m128i*)&b0[j * MB_BLOCK_SIZE]);
        __m128i row_l1 = _mm_loadu_si128((const __m128i*)&b1[j * MB_BLOCK_SIZE]);
        _mm_storeu_si128((__m128i*)&mb_pred[j][ioff], _mm_avg_epu16(row_l0, row_l1));
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row_l0 = _mm_loadl_epi64((const __m128i*)&b0[j * MB_BLOCK_SIZE]);
        __m128i row_l1 = _mm_loadl_epi64((const __m128i*)&b1[j * MB_BLOCK_SIZE]);
        _mm_storel_epi64((__m128i*)&mb_pred[j][ioff], _mm_avg_epu16(row_l0, row_l1));
      }
      return;
    }
    else if (block_size_x == 2)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row_l0 = _mm_cvtsi32_si128(*(const int32_t*)&b0[j * MB_BLOCK_SIZE]);
        __m128i row_l1 = _mm_cvtsi32_si128(*(const int32_t*)&b1[j * MB_BLOCK_SIZE]);
        *(int32_t*)&mb_pred[j][ioff] = _mm_cvtsi128_si32(_mm_avg_epu16(row_l0, row_l1));
      }
      return;
    }
  }
#endif
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
  if (sizeof(imgpel) == sizeof(uint16_t))
  {
    const uint16_t *b0 = (const uint16_t*)block_l0;
    const uint16_t *b1 = (const uint16_t*)block_l1;
    uint16_t *dst = (uint16_t*)mb_pred;

    if (wp_scale_l0 == wp_scale_l1 && wp_offset == 0 && wp_scale_l0 == (1 << (weight_denom - 1)))
    {
      if (block_size_x == 16)
      {
        for (int j = 0; j < block_size_y; j++)
        {
          __m256i row0 = _mm256_loadu_si256((const __m256i*)(b0 + j * MB_BLOCK_SIZE));
          __m256i row1 = _mm256_loadu_si256((const __m256i*)(b1 + j * MB_BLOCK_SIZE));
          _mm256_storeu_si256((__m256i*)(dst + j * MB_BLOCK_SIZE), _mm256_avg_epu16(row0, row1));
        }
        return;
      }
      else if (block_size_x == 8)
      {
        for (int j = 0; j < block_size_y; j++)
        {
          __m128i row0 = _mm_loadu_si128((const __m128i*)(b0 + j * MB_BLOCK_SIZE));
          __m128i row1 = _mm_loadu_si128((const __m128i*)(b1 + j * MB_BLOCK_SIZE));
          _mm_storeu_si128((__m128i*)(dst + j * MB_BLOCK_SIZE), _mm_avg_epu16(row0, row1));
        }
        return;
      }
      else if (block_size_x == 4)
      {
        for (int j = 0; j < block_size_y; j++)
        {
          __m128i row0 = _mm_loadl_epi64((const __m128i*)(b0 + j * MB_BLOCK_SIZE));
          __m128i row1 = _mm_loadl_epi64((const __m128i*)(b1 + j * MB_BLOCK_SIZE));
          _mm_storel_epi64((__m128i*)(dst + j * MB_BLOCK_SIZE), _mm_avg_epu16(row0, row1));
        }
        return;
      }
      else if (block_size_x == 2)
      {
        for (int j = 0; j < block_size_y; j++)
        {
          __m128i row0 = _mm_cvtsi32_si128(*(const int32_t*)(b0 + j * MB_BLOCK_SIZE));
          __m128i row1 = _mm_cvtsi32_si128(*(const int32_t*)(b1 + j * MB_BLOCK_SIZE));
          *(int32_t*)(dst + j * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(_mm_avg_epu16(row0, row1));
        }
        return;
      }
    }

    __m128i w = _mm_set1_epi32((uint16_t)wp_scale_l0 | ((uint32_t)(uint16_t)wp_scale_l1 << 16));
    __m128i rnd = _mm_set1_epi32(1 << (weight_denom - 1));
    __m128i off = _mm_set1_epi32(wp_offset);
    __m128i shift = _mm_cvtsi32_si128(weight_denom);
    __m128i clip_max = _mm_set1_epi16((uint16_t)color_clip);

    if (block_size_x == 16)
    {
      __m256i w256 = _mm256_set1_epi32((uint16_t)wp_scale_l0 | ((uint32_t)(uint16_t)wp_scale_l1 << 16));
      __m256i rnd256 = _mm256_set1_epi32(1 << (weight_denom - 1));
      __m256i off256 = _mm256_set1_epi32(wp_offset);
      __m256i clip_max256 = _mm256_set1_epi16((uint16_t)color_clip);

      for (int j = 0; j < block_size_y; j++)
      {
        __m256i row0 = _mm256_loadu_si256((const __m256i*)(b0 + j * MB_BLOCK_SIZE));
        __m256i row1 = _mm256_loadu_si256((const __m256i*)(b1 + j * MB_BLOCK_SIZE));

        __m256i pair_lo = _mm256_unpacklo_epi16(row0, row1);
        __m256i pair_hi = _mm256_unpackhi_epi16(row0, row1);
        __m256i sum_lo = _mm256_add_epi32(_mm256_sra_epi32(_mm256_add_epi32(_mm256_madd_epi16(pair_lo, w256), rnd256), shift), off256);
        __m256i sum_hi = _mm256_add_epi32(_mm256_sra_epi32(_mm256_add_epi32(_mm256_madd_epi16(pair_hi, w256), rnd256), shift), off256);
        __m256i res16 = _mm256_min_epu16(_mm256_packus_epi32(sum_lo, sum_hi), clip_max256);

        _mm256_storeu_si256((__m256i*)(dst + j * MB_BLOCK_SIZE), res16);
      }
      return;
    }
    else if (block_size_x == 8)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row0 = _mm_loadu_si128((const __m128i*)(b0 + j * MB_BLOCK_SIZE));
        __m128i row1 = _mm_loadu_si128((const __m128i*)(b1 + j * MB_BLOCK_SIZE));

        __m128i pair0 = _mm_unpacklo_epi16(row0, row1);
        __m128i pair1 = _mm_unpackhi_epi16(row0, row1);
        __m128i sum0 = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair0, w), rnd), shift), off);
        __m128i sum1 = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair1, w), rnd), shift), off);
        __m128i res16 = _mm_min_epu16(_mm_packus_epi32(sum0, sum1), clip_max);

        _mm_storeu_si128((__m128i*)(dst + j * MB_BLOCK_SIZE), res16);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row0 = _mm_loadl_epi64((const __m128i*)(b0 + j * MB_BLOCK_SIZE));
        __m128i row1 = _mm_loadl_epi64((const __m128i*)(b1 + j * MB_BLOCK_SIZE));

        __m128i pair = _mm_unpacklo_epi16(row0, row1);
        __m128i sum = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), rnd), shift), off);
        __m128i res16 = _mm_min_epu16(_mm_packus_epi32(sum, sum), clip_max);

        _mm_storel_epi64((__m128i*)(dst + j * MB_BLOCK_SIZE), res16);
      }
      return;
    }
    else if (block_size_x == 2)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        __m128i row0 = _mm_cvtsi32_si128(*(const int32_t*)(b0 + j * MB_BLOCK_SIZE));
        __m128i row1 = _mm_cvtsi32_si128(*(const int32_t*)(b1 + j * MB_BLOCK_SIZE));

        __m128i pair = _mm_unpacklo_epi16(row0, row1);
        __m128i sum = _mm_add_epi32(_mm_sra_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), rnd), shift), off);
        __m128i res16 = _mm_min_epu16(_mm_packus_epi32(sum, sum), clip_max);

        *(int32_t*)(dst + j * MB_BLOCK_SIZE) = _mm_cvtsi128_si32(res16);
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
  if (sizeof(imgpel) == sizeof(uint16_t))
  {
    const __m128i w = _mm_set1_epi32((uint16_t)w00 | ((uint32_t)(uint16_t)w01 << 16));
    const __m128i round_vec = _mm_set1_epi32(1 << (total_scale - 1));

    if (block_size_x == 8)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        const uint16_t *n = c + span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i nv = _mm_loadu_si128((const __m128i*)n);

        __m128i pair_lo = _mm_unpacklo_epi16(cv, nv);
        __m128i pair_hi = _mm_unpackhi_epi16(cv, nv);

        __m128i sum_lo = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair_lo, w), round_vec), total_scale);
        __m128i sum_hi = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair_hi, w), round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum_lo, sum_hi);

        _mm_storeu_si128((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        const uint16_t *n = c + span;

        __m128i cv = _mm_loadl_epi64((const __m128i*)c);
        __m128i nv = _mm_loadl_epi64((const __m128i*)n);

        __m128i pair = _mm_unpacklo_epi16(cv, nv);
        __m128i sum = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum, sum);

        _mm_storel_epi64((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 2)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        const uint16_t *n = c + span;

        __m128i cv = _mm_cvtsi32_si128(*(const int32_t*)c);
        __m128i nv = _mm_cvtsi32_si128(*(const int32_t*)n);

        __m128i pair = _mm_unpacklo_epi16(cv, nv);
        __m128i sum = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum, sum);

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
  if (sizeof(imgpel) == sizeof(uint16_t))
  {
    const __m128i w = _mm_set1_epi32((uint16_t)w00 | ((uint32_t)(uint16_t)w10 << 16));
    const __m128i round_vec = _mm_set1_epi32(1 << (total_scale - 1));

    if (block_size_x == 8)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i nv = _mm_loadu_si128((const __m128i*)(c + 1));

        __m128i pair_lo = _mm_unpacklo_epi16(cv, nv);
        __m128i pair_hi = _mm_unpackhi_epi16(cv, nv);

        __m128i sum_lo = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair_lo, w), round_vec), total_scale);
        __m128i sum_hi = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair_hi, w), round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum_lo, sum_hi);

        _mm_storeu_si128((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        __m128i cv = _mm_loadl_epi64((const __m128i*)c);
        __m128i nv = _mm_loadl_epi64((const __m128i*)(c + 1));

        __m128i pair = _mm_unpacklo_epi16(cv, nv);
        __m128i sum = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum, sum);

        _mm_storel_epi64((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 2)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        __m128i cv = _mm_cvtsi32_si128(*(const int32_t*)c);
        __m128i nv = _mm_cvtsi32_si128(*(const int32_t*)(c + 1));

        __m128i pair = _mm_unpacklo_epi16(cv, nv);
        __m128i sum = _mm_srai_epi32(_mm_add_epi32(_mm_madd_epi16(pair, w), round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum, sum);

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
  if (sizeof(imgpel) == sizeof(uint16_t))
  {
    const __m128i w_c = _mm_set1_epi32((uint16_t)w00 | ((uint32_t)(uint16_t)w10 << 16));
    const __m128i w_n = _mm_set1_epi32((uint16_t)w01 | ((uint32_t)(uint16_t)w11 << 16));
    const __m128i round_vec = _mm_set1_epi32(1 << (total_scale - 1));

    if (block_size_x == 8)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        const uint16_t *n = c + span;

        __m128i cv = _mm_loadu_si128((const __m128i*)c);
        __m128i cv_next = _mm_loadu_si128((const __m128i*)(c + 1));
        __m128i nv = _mm_loadu_si128((const __m128i*)n);
        __m128i nv_next = _mm_loadu_si128((const __m128i*)(n + 1));

        __m128i pair_c_lo = _mm_unpacklo_epi16(cv, cv_next);
        __m128i pair_c_hi = _mm_unpackhi_epi16(cv, cv_next);
        __m128i pair_n_lo = _mm_unpacklo_epi16(nv, nv_next);
        __m128i pair_n_hi = _mm_unpackhi_epi16(nv, nv_next);

        __m128i prod_lo = _mm_add_epi32(_mm_madd_epi16(pair_c_lo, w_c), _mm_madd_epi16(pair_n_lo, w_n));
        __m128i prod_hi = _mm_add_epi32(_mm_madd_epi16(pair_c_hi, w_c), _mm_madd_epi16(pair_n_hi, w_n));

        __m128i sum_lo = _mm_srai_epi32(_mm_add_epi32(prod_lo, round_vec), total_scale);
        __m128i sum_hi = _mm_srai_epi32(_mm_add_epi32(prod_hi, round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum_lo, sum_hi);

        _mm_storeu_si128((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 4)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        const uint16_t *n = c + span;

        __m128i cv = _mm_loadl_epi64((const __m128i*)c);
        __m128i cv_next = _mm_loadl_epi64((const __m128i*)(c + 1));
        __m128i nv = _mm_loadl_epi64((const __m128i*)n);
        __m128i nv_next = _mm_loadl_epi64((const __m128i*)(n + 1));

        __m128i pair_c = _mm_unpacklo_epi16(cv, cv_next);
        __m128i pair_n = _mm_unpacklo_epi16(nv, nv_next);

        __m128i prod = _mm_add_epi32(_mm_madd_epi16(pair_c, w_c), _mm_madd_epi16(pair_n, w_n));
        __m128i sum = _mm_srai_epi32(_mm_add_epi32(prod, round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum, sum);

        _mm_storel_epi64((__m128i*)(block + j * MB_BLOCK_SIZE), packed);
      }
      return;
    }
    else if (block_size_x == 2)
    {
      for (int j = 0; j < block_size_y; j++)
      {
        const uint16_t *c = (const uint16_t*)(cur_img + j * span);
        const uint16_t *n = c + span;

        __m128i cv = _mm_cvtsi32_si128(*(const int32_t*)c);
        __m128i cv_next = _mm_cvtsi32_si128(*(const int32_t*)(c + 1));
        __m128i nv = _mm_cvtsi32_si128(*(const int32_t*)n);
        __m128i nv_next = _mm_cvtsi32_si128(*(const int32_t*)(n + 1));

        __m128i pair_c = _mm_unpacklo_epi16(cv, cv_next);
        __m128i pair_n = _mm_unpacklo_epi16(nv, nv_next);

        __m128i prod = _mm_add_epi32(_mm_madd_epi16(pair_c, w_c), _mm_madd_epi16(pair_n, w_n));
        __m128i sum = _mm_srai_epi32(_mm_add_epi32(prod, round_vec), total_scale);
        __m128i packed = _mm_packus_epi32(sum, sum);

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
