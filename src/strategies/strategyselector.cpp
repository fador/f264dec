/**
 * \file strategyselector.cpp
 * \brief Runtime dynamic SIMD/optimization selector modeled after Kvazaar.
 */

#include "strategies/strategyselector.h"
#include "strategies/strategies-transform.h"
#include "strategies/strategies-mc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#define F264_ARCH_INTEL 1
#if defined(_MSC_VER)
#include <intrin.h>
#include <immintrin.h>
#elif defined(__GNUC__)
#include <cpuid.h>
#include <x86intrin.h>
#endif
#endif

#define F264_STRATEGY_LIST_ALLOC_SIZE 16

f264_hardware_flags_t f264_g_hardware_flags;
f264_hardware_flags_t f264_g_strategies_in_use;
f264_hardware_flags_t f264_g_strategies_available;

static const f264_strategy_to_select_t strategies_to_select[] = {
  STRATEGIES_TRANSFORM_EXPORTS
  STRATEGIES_MC_EXPORTS
  { nullptr, nullptr }
};

#if defined(F264_ARCH_INTEL)
typedef struct {
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
} f264_cpuid_t;

static inline int get_cpuid(unsigned int level, unsigned int sublevel, f264_cpuid_t *info)
{
#if defined(_MSC_VER)
  int cpu_info[4] = { 0 };
  __cpuidex(cpu_info, level, sublevel);
  info->eax = (unsigned int)cpu_info[0];
  info->ebx = (unsigned int)cpu_info[1];
  info->ecx = (unsigned int)cpu_info[2];
  info->edx = (unsigned int)cpu_info[3];
  return 1;
#elif defined(__GNUC__)
  if (__get_cpuid_max(level & 0x80000000, nullptr) < level) return 0;
  __cpuid_count(level, sublevel, info->eax, info->ebx, info->ecx, info->edx);
  return 1;
#else
  return 0;
#endif
}
#endif // F264_ARCH_INTEL

static void set_hardware_flags(int32_t cpuid, uint8_t logging)
{
  std::memset(&f264_g_hardware_flags, 0, sizeof(f264_g_hardware_flags));

#if defined(F264_ARCH_INTEL)
  if (cpuid) {
    f264_cpuid_t id1 = { 0, 0, 0, 0 };
    get_cpuid(1, 0, &id1);

    enum {
      CPUID1_EDX_MMX  = 1 << 23,
      CPUID1_EDX_SSE  = 1 << 25,
      CPUID1_EDX_SSE2 = 1 << 26,
      CPUID1_EDX_HT   = 1 << 28,
    };
    enum {
      CPUID1_ECX_SSE3   = 1 << 0,
      CPUID1_ECX_SSSE3  = 1 << 9,
      CPUID1_ECX_SSE41  = 1 << 19,
      CPUID1_ECX_SSE42  = 1 << 20,
      CPUID1_ECX_XSAVE  = 1 << 26,
      CPUID1_ECX_OSXSAVE= 1 << 27,
      CPUID1_ECX_AVX    = 1 << 28,
    };
    enum {
      CPUID7_EBX_AVX2   = 1 << 5,
    };

    if (id1.edx & CPUID1_EDX_MMX)  f264_g_hardware_flags.intel_flags.mmx = 1;
    if (id1.edx & CPUID1_EDX_SSE)  f264_g_hardware_flags.intel_flags.sse = 1;
    if (id1.edx & CPUID1_EDX_SSE2) f264_g_hardware_flags.intel_flags.sse2 = 1;

    if (id1.ecx & CPUID1_ECX_SSE3)  f264_g_hardware_flags.intel_flags.sse3 = 1;
    if (id1.ecx & CPUID1_ECX_SSSE3) f264_g_hardware_flags.intel_flags.ssse3 = 1;
    if (id1.ecx & CPUID1_ECX_SSE41) f264_g_hardware_flags.intel_flags.sse41 = 1;
    if (id1.ecx & CPUID1_ECX_SSE42) f264_g_hardware_flags.intel_flags.sse42 = 1;

    if (id1.ecx & (CPUID1_ECX_XSAVE | CPUID1_ECX_OSXSAVE)) {
      uint64_t xcr0 = 0;
#if defined(_MSC_VER)
      xcr0 = _xgetbv(0);
#elif defined(__GNUC__)
      unsigned eax = 0, edx = 0;
      asm("xgetbv" : "=a"(eax), "=d"(edx) : "c" (0));
      xcr0 = ((uint64_t)edx << 32) | eax;
#endif
      if ((id1.ecx & CPUID1_ECX_AVX) && ((xcr0 & 0x6) == 0x6)) {
        f264_g_hardware_flags.intel_flags.avx = 1;
        f264_cpuid_t id7 = { 0, 0, 0, 0 };
        get_cpuid(7, 0, &id7);
        if (id7.ebx & CPUID7_EBX_AVX2) {
          f264_g_hardware_flags.intel_flags.avx2 = 1;
        }
      }
    }
  }
#elif defined(__ARM_NEON) || defined(__aarch64__)
  f264_g_hardware_flags.arm_flags.neon = 1;
#endif

#if defined(_WIN32)
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);
  f264_g_hardware_flags.logical_cpu_count = sysinfo.dwNumberOfProcessors;
#else
  f264_g_hardware_flags.logical_cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
#endif
  f264_g_hardware_flags.physical_cpu_count = f264_g_hardware_flags.logical_cpu_count;
}

extern "C" {

int f264_strategyselector_register(void *opaque, const char *type, const char *strategy_name, int priority, void *fptr)
{
  auto *strategies = (f264_strategy_list_t*)opaque;

  if (strategies->allocated == strategies->count) {
    auto *new_strats = (f264_strategy_t*)std::realloc(
        strategies->strategies,
        sizeof(f264_strategy_t) * (strategies->allocated + F264_STRATEGY_LIST_ALLOC_SIZE));
    if (!new_strats) return 0;
    strategies->strategies = new_strats;
    strategies->allocated += F264_STRATEGY_LIST_ALLOC_SIZE;
  }

  f264_strategy_t *s = &strategies->strategies[strategies->count++];
  s->type = type;
  s->strategy_name = strategy_name;
  s->priority = priority;
  s->fptr = fptr;

  return 1;
}

static void* strategyselector_choose_for(const f264_strategy_list_t *strategies, const char *strategy_type)
{
  int max_priority = -1;
  int max_idx = -1;

  for (unsigned int i = 0; i < strategies->count; ++i) {
    if (std::strcmp(strategies->strategies[i].type, strategy_type) == 0) {
      if ((int)strategies->strategies[i].priority > max_priority) {
        max_priority = strategies->strategies[i].priority;
        max_idx = i;
      }
    }
  }

  if (max_idx == -1) return nullptr;

  if (std::strcmp(strategies->strategies[max_idx].strategy_name, "sse2") == 0) f264_g_strategies_in_use.intel_flags.sse2++;
  if (std::strcmp(strategies->strategies[max_idx].strategy_name, "avx2") == 0) f264_g_strategies_in_use.intel_flags.avx2++;
  if (std::strcmp(strategies->strategies[max_idx].strategy_name, "neon") == 0) f264_g_strategies_in_use.arm_flags.neon++;

  return strategies->strategies[max_idx].fptr;
}

int f264_strategyselector_init(int32_t cpuid, uint8_t bitdepth, uint8_t logging)
{
  f264_strategy_list_t strategies;
  strategies.allocated = 0;
  strategies.count = 0;
  strategies.strategies = nullptr;

  set_hardware_flags(cpuid, logging);

  // Register transform strategies
  if (!f264_strategy_register_transform(&strategies, bitdepth)) {
    std::fprintf(stderr, "f264_strategy_register_transform failed!\n");
    return 0;
  }

  // Register MC strategies
  if (!f264_strategy_register_mc(&strategies, bitdepth)) {
    std::fprintf(stderr, "f264_strategy_register_mc failed!\n");
    return 0;
  }

  // Select optimal function pointers
  const f264_strategy_to_select_t *cur = strategies_to_select;
  while (cur->strategy_type != nullptr) {
    void *selected = strategyselector_choose_for(&strategies, cur->strategy_type);
    if (!selected) {
      std::fprintf(stderr, "f264: Failed to find strategy for '%s'\n", cur->strategy_type);
      std::free(strategies.strategies);
      return 0;
    }
    *(cur->fptr) = selected;
    cur++;
  }

  if (logging) {
    std::fprintf(stderr, "f264: Strategy selector initialized. SIMD flags: ");
    if (f264_g_hardware_flags.intel_flags.sse2) std::fprintf(stderr, "SSE2 ");
    if (f264_g_hardware_flags.intel_flags.ssse3) std::fprintf(stderr, "SSSE3 ");
    if (f264_g_hardware_flags.intel_flags.sse41) std::fprintf(stderr, "SSE4.1 ");
    if (f264_g_hardware_flags.intel_flags.avx2) std::fprintf(stderr, "AVX2 ");
    if (f264_g_hardware_flags.arm_flags.neon) std::fprintf(stderr, "NEON ");
    std::fprintf(stderr, "\n");
  }

  std::free(strategies.strategies);
  return 1;
}

} // extern "C"
