# f264dec

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License: ITU-T / ISO/IEC](https://img.shields.io/badge/License-ITU--T%20%2F%20ISO%20IEC-green.svg)](https://www.itu.int)
[![Bit-Exact Conformance](https://img.shields.io/badge/Conformance-100%25%20(35%2F35)-brightgreen.svg)](#conformance--testing)
[![1080p Performance](https://img.shields.io/badge/1080p%20Decode-82%2B%20FPS-orange.svg)](#performance-benchmarks)

**f264dec** is an ultra-fast, modern C++20 H.264/AVC video decoder featuring AVX2 SIMD acceleration, fine-grained row-level inter-frame overlap pipelining, and DAG-based multi-threading. It is engineered for low latency and high throughput while preserving **100% bit-exact conformance** with standard reference decoders across baseline, main, and high profiles.

---

## Key Features

- **High-Throughput 1080p Real-Time Decoding**:
  Decodes single-slice 1080p 60fps High Profile streams at **>82 FPS** (0.73 s wall time for 60 frames on an AMD Ryzen 9 3900X), delivering a **3.28x multi-threaded speedup** over single-threaded decoding.
- **Progressive Row-Level Inter-Frame Pipeline**:
  Features an asynchronous directed acyclic graph (DAG) job queue with atomic macroblock-row tracking (`progress_rows`). Completed macroblock rows are immediately deblocked and padded in-flight, allowing downstream P- and B-frames to perform motion compensation without waiting for upstream reference frames to finish.
- **Dynamic Work Scheduling**:
  Dynamic idle/finished job slot queries eliminate pipeline bubbles and frame serialization across GOP structures.
- **Comprehensive AVX2 / SSE Vectorization**:
  - **Motion Compensation**: SIMD 6-tap FIR interpolation across all 15 half-pel and quarter-pel subpel positions, vectorized bilinear chroma interpolation, dual-row AVX2 bi-prediction, and AVX2 implicit weighted bi-prediction.
  - **Deblocking Filter**: Vectorized 4-pel horizontal and vertical normal in-loop deblocking with branchless threshold evaluation and 32-bit zero-strength early-out bypasses.
  - **Inverse Transforms**: AVX2 256-bit 8x8 IDCT and SSE2 4x4 IDCT with in-register saturated sample reconstruction.
- **Modular Strategy Architecture**:
  Clean strategy interfaces (`src/strategies/`) with automatic CPUID feature detection and runtime fallback to generic portable C++20 routines.
- **Strict Bit-Exact Conformance**:
  Verified 100% bit-exact across the complete 35-stream regression test suite, including High 10-bit, High 4:2:2, High 4:4:4 Predictive, MBAFF, PAFF, CAVLC, CABAC, FMO, Data Partitioning, Lossless, and Weighted Prediction.

---

## Performance Benchmarks

Measured on an AMD Ryzen 9 3900X (12 cores / 24 threads, 3.8 GHz base) running Windows 11 MSVC Release:

| Bitstream | Profile / Resolution | Frames | Single-Thread (1T) | Multi-Thread (24T) | Multi-Thread Speedup |
| :--- | :--- | :---: | :---: | :---: | :---: |
| `x264_1080p_bench.264` | 1080p 60fps High | 60 | 2.40 s (25.0 FPS) | **0.73 s (82.1 FPS)** | **3.28x** |
| `x264_720p_high_cavlc.264` | 720p High Profile CAVLC | 30 | 0.33 s (91.7 FPS) | **0.22 s (139.1 FPS)** | **1.52x** |
| `x264_720p_main_slices.264` | 720p Multi-Slice Main | 30 | 0.53 s (56.5 FPS) | **0.26 s (114.8 FPS)** | **2.03x** |
| `x264_720p_high10.264` | 720p High 10-bit | 30 | 0.32 s (95.1 FPS) | **0.32 s (94.2 FPS)** | 1.00x |
| `base_cavlc_slices.264` | QCIF Multi-Slice Baseline | 10 | 0.04 s (243.6 FPS) | **0.03 s (325.2 FPS)** | **1.33x** |

For detailed hotspot profiles, architectural analysis, and optimization milestones, see [BENCHMARK.md](BENCHMARK.md).

---

## Building

### Requirements
- **C++ Compiler**: Clang 15+, GCC 12+, or MSVC 2022 (with full C++20 support)
- **Build System**: [CMake](https://cmake.org/) 3.20 or newer
- **Target CPU**: x86-64 (AVX2 + FMA recommended for SIMD acceleration)

### Windows (MSVC)
```powershell
# Configure CMake build directory
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build release executable
cmake --build build --config Release -j
```
The output binary will be located at `build/Release/f264dec.exe`.

### Linux / macOS
```bash
# Configure CMake build directory
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build release executable
cmake --build build -j$(nproc)
```
The output binary will be located at `build/f264dec`.

---

## Usage

```text
f264dec [-i input.264] [-o output.yuv] [-t threads] [-s] [--cpuid 0|1]
```

### Options
| Option | Description |
| :--- | :--- |
| `-i <file>` | Input H.264 / AVC Annex-B bitstream file path. |
| `-o <file>` | Output raw YUV file path (omitting this skips file writes for pure decoding benchmark). |
| `-t <num>` | Number of worker threads (`0` = auto-detect hardware concurrency, `1` = single-threaded). |
| `-s` | Silent mode: suppresses per-frame console logging and displays performance hotspot summaries. |
| `--cpuid <0\|1>` | Runtime SIMD dispatch control: `1` = enable hardware SIMD (AVX2/SSE), `0` = force generic C++20 routines. |
| `-f <file>` | Path to custom decoder configuration file. |
| `-h`, `--help` | Display command-line usage and help. |

### Examples

```bash
# Decode stream to YUV using auto-detected CPU threads:
f264dec -i input.264 -o output.yuv

# Benchmark 1080p decode speed in silent mode using all available threads:
f264dec -s -i tests/streams/x264_1080p_bench.264 -t 0

# Single-threaded benchmark run:
f264dec -s -i tests/streams/x264_1080p_bench.264 -t 1

# Verify performance with SIMD disabled (pure generic C++20 path):
f264dec -s -i tests/streams/x264_1080p_bench.264 -t 0 --cpuid 0
```

---

## Conformance & Testing

The test suite validates output hashes against reference decoded bitstreams:

```bash
# Run 100% bit-exact conformance test suite across all 35 streams
python tests/run_tests.py --decoder build/Release/f264dec.exe --streams-dir tests/streams

# Run automated multi-run benchmark suite
python tests/benchmark.py --decoder build/Release/f264dec.exe --streams-dir tests/streams --runs 3
```

---

## Project Structure

```text
f264dec/
├── CMakeLists.txt              # Top-level CMake configuration
├── BENCHMARK.md                # Comprehensive profiling analysis and performance milestones
├── tests/
│   ├── run_tests.py            # Bit-exact regression test runner
│   ├── benchmark.py            # Automated performance benchmarking harness
│   └── streams/                # Test bitstreams (Baseline, Main, High, 4:2:2, 4:4:4, etc.)
└── src/
    ├── ldecod.cpp              # Entry point and command-line parser
    ├── image.cpp               # Picture decoding, slice dispatch, and frame pipelining
    ├── mc_prediction.cpp       # Motion compensation coordination and reference wait sync
    ├── loopFilter.cpp          # In-loop deblocking filter and progressive row-deblocking
    ├── mbuffer.cpp             # Decoded Picture Buffer (DPB) and frame memory management
    ├── threading/
    │   ├── threadqueue.cpp     # Thread pool with DAG task-dependency resolution
    │   └── frame_pipeline.h    # Multi-frame pipeline coordinator
    └── strategies/             # Modular SIMD & Generic Algorithm Implementations
        ├── strategyselector.h  # Dynamic CPUID detection and strategy dispatcher
        ├── generic/            # Clean reference C++20 implementations
        ├── sse2/               # SSE2 transform implementations
        └── avx2/               # AVX2 256-bit vectorized routines (MC, Deblock, Transforms)
```

---

## License & Acknowledgments

`f264dec` builds upon and heavily modernizes the reference implementation developed within the ISO/IEC 14496-10 and ITU-T H.264 standards committees (Joint Model / JM reference software).

- Refer to ITU-T and ISO/IEC standard licensing terms for underlying reference algorithm copyrights.
- Modifications, C++20 modernization, multithreading pipelines, and SIMD strategy modules are provided under the respective permissive project terms.
