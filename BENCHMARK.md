# f264dec Performance Benchmark & Hotspot Analysis

## System Environment
- **CPU**: AMD Ryzen 9 3900X (12 Cores, 24 Logical Processors, Base: 3.8 GHz)
- **SIMD Capabilities**: MMX, SSE, SSE2, SSSE3, SSE4.1, SSE4.2, AVX, AVX2, FMA3
- **Operating System**: Windows 11 (MSVC 2022 C++20 / x64 Release) & Ubuntu WSL2 (GCC 13.3.0)

---

## Baseline Performance Results

Benchmarks conducted using [tests/benchmark.py](tests/benchmark.py) across 3 runs (best time recorded):

| Bitstream | Format / Profile | Frames | Single-Thread (1T) | Multi-Thread (Auto) | Multi-Thread Speedup |
| :--- | :--- | :---: | :---: | :---: | :---: |
| `x264_1080p_bench.264` | 1080p High Profile | 60 | 2.29 s (26.2 FPS) | 2.32 s (25.8 FPS) | 1.0x (single slice) |
| `x264_720p_main_slices.264` | 720p Multi-Slice Main | 30 | 0.52 s (57.3 FPS) | 0.25 s (121.7 FPS) | **2.12x** |
| `x264_720p_high_cavlc.264` | 720p High Profile CAVLC | 30 | 0.32 s (95.1 FPS) | 0.20 s (148.3 FPS) | **1.56x** |
| `x264_720p_high10.264` | 720p High 10-bit | 30 | 0.30 s (100.1 FPS) | 0.30 s (101.3 FPS) | 1.0x |
| `x264_720p_high_tff.264` | 720p Interlaced / TFF | 30 | 0.52 s (57.7 FPS) | 0.54 s (55.5 FPS) | 1.0x |
| `high444_lossless.264` | CIF 4:4:4 Lossless | 10 | 0.12 s (84.3 FPS) | 0.12 s (85.1 FPS) | 1.0x |
| `base_cavlc_slices.264` | QCIF Multi-Slice Baseline | 10 | 0.04 s (244.9 FPS) | 0.03 s (330.0 FPS) | **1.35x** |

---

## Execution Hotspot Breakdown

Profiling conducted using high-resolution nanosecond timers on 1080p stream (`x264_1080p_bench.264`, 60 frames):

```
=======================================================
           f264dec Performance Hotspot Profile         
=======================================================
  Component             Time (ms)     Percentage       
-------------------------------------------------------
  Motion Compensation     871.28 ms        42.2 %
  CABAC/Entropy Decode    642.91 ms        31.1 %
  Deblocking Filter       412.50 ms        20.0 %
  Inverse Transforms      139.68 ms         6.8 %
-------------------------------------------------------
  Profiled Core Total    2066.37 ms       100.0 %
=======================================================
```

### Analysis & Targets for AVX2 Optimization
1. **Motion Compensation (42.2% of compute)**:
   - **Primary Bottleneck**: Subpel quarter-pixel and half-pixel interpolation (`get_luma_...`).
   - The 6-tap FIR filter `(p0 + p5) - 5*(p1 + p4) + 20*(p2 + p3)` and chroma bilinear interpolation previously ran scalar byte-by-byte in nested loops.
   - **Target**: AVX2 SIMD implementation processing 16 and 8 pixels in parallel using vector arithmetic.

2. **Deblocking Filter (20.0% of compute)**:
   - **Primary Bottleneck**: `EdgeLoopLumaNormal` and `EdgeLoopChromaNormal` in `src/loop_filter_normal.cpp`.
   - Edges are currently evaluated and filtered one sample at a time.
   - **Target**: Vectorized filtering operating across 8 and 16 edge samples simultaneously.

3. **8x8 Inverse Transform & Reconstruction (6.8% of compute)**:
   - **Primary Bottleneck**: `inverse8x8` and `recon8x8` in `src/transform8x8.cpp` and `src/transform.cpp`.
   - **Target**: AVX2 256-bit implementation computing complete 8x8 IDCT + saturated clipping in registers.

---

## Modular Strategy Architecture

All optimizations and platform-specific implementations are organized into clean subdirectories under `src/strategies/`:

```
src/strategies/
├── strategyselector.h / .cpp       # Dynamic CPUID detection and strategy dispatcher
├── strategies-transform.h / .cpp   # Transform & reconstruction strategy dispatch hub
├── strategies-mc.h / .cpp          # Motion compensation subpel dispatch hub
├── generic/                        # Generic portable C++20 reference algorithms
│   ├── generic-transform.h / .cpp  # 4x4 IDCT, 8x8 IDCT, sample reconstruction
│   └── generic-mc.h / .cpp         # Subpel luma 6-tap FIR interpolation
├── sse2/                           # SSE2 SIMD implementations
│   └── sse2-transform.h / .cpp     # 4x4 IDCT and sample reconstruction
└── avx2/                           # AVX2 256-bit SIMD implementations
    ├── avx2-transform.h / .cpp     # 8x8 IDCT and 8x8 sample reconstruction
    └── avx2-mc.h / .cpp            # 6-tap subpel FIR (half-pel & quarter-pel: 10, 20, 30, 01, 02, 03, 22)
```

Each SIMD strategy includes automated fallback to generic routines for edge cases (e.g. non-8-bit depth or small sub-macroblock partition sizes), guaranteeing 100% bit-exact conformance across the entire JM test stream suite on both MSVC and GCC/Clang.

