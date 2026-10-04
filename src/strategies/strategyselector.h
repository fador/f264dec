#ifndef F264_STRATEGYSELECTOR_H_
#define F264_STRATEGYSELECTOR_H_

/**
 * \file strategyselector.h
 * \brief Dynamic SIMD/optimization strategy selector inspired by Kvazaar.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  const char *type;          // Function type / identifier name
  const char *strategy_name; // Strategy name (e.g. "generic", "sse2", "avx2", "neon")
  unsigned int priority;     // Priority (0 = lowest/generic, higher = more specialized)
  void *fptr;                // Function pointer
} f264_strategy_t;

typedef struct {
  unsigned int count;
  unsigned int allocated;
  f264_strategy_t *strategies;
} f264_strategy_list_t;

typedef struct {
  const char *strategy_type;
  void **fptr;
} f264_strategy_to_select_t;

typedef struct {
  struct {
    int mmx;
    int sse;
    int sse2;
    int sse3;
    int ssse3;
    int sse41;
    int sse42;
    int avx;
    int avx2;
    bool hyper_threading;
  } intel_flags;

  struct {
    int neon;
  } arm_flags;

  int logical_cpu_count;
  int physical_cpu_count;
} f264_hardware_flags_t;

extern f264_hardware_flags_t f264_g_hardware_flags;
extern f264_hardware_flags_t f264_g_strategies_in_use;
extern f264_hardware_flags_t f264_g_strategies_available;

/**
 * \brief Initialize strategy selector and dispatch appropriate SIMD implementations.
 * \param cpuid If nonzero, auto-detect CPU features. If 0, use generic C only.
 * \param bitdepth Bitdepth of stream (8, 10, etc.)
 * \param logging If nonzero, log chosen SIMD strategies to stderr.
 */
int f264_strategyselector_init(int32_t cpuid, uint8_t bitdepth, uint8_t logging);

/**
 * \brief Register an optimization strategy.
 */
int f264_strategyselector_register(void *opaque, const char *type, const char *strategy_name, int priority, void *fptr);

#ifdef __cplusplus
}
#endif

#endif // F264_STRATEGYSELECTOR_H_
