#!/usr/bin/env python3
import subprocess
import time
import sys
import os
import json

TEST_STREAMS = [
    ("x264_1080p_bench.264", 60, "1080p 60fps High Profile"),
    ("x264_720p_main_slices.264", 30, "720p Multi-slice Main Profile"),
    ("x264_720p_high10.264", 30, "720p 10-bit High10 Profile"),
    ("x264_720p_high_cavlc.264", 30, "720p CAVLC High Profile"),
    ("x264_720p_high_tff.264", 30, "720p Interlaced Field/TFF"),
    ("high444_lossless.264", 10, "CIF 4:4:4 Lossless High Profile"),
    ("base_cavlc_slices.264", 10, "QCIF Multi-slice Baseline Profile"),
]

def run_bench(decoder, stream_path, threads, cpuid=None, runs=3):
    cmd = [decoder, "-s", "-i", stream_path]
    if threads is not None:
        cmd.extend(["-t", str(threads)])
    if cpuid is not None:
        cmd.extend(["--cpuid", str(cpuid)])
    
    times = []
    for _ in range(runs):
        t0 = time.perf_counter()
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        t1 = time.perf_counter()
        if res.returncode != 0:
            print(f"Error running {cmd}: {res.stderr.decode()}", file=sys.stderr)
            return None
        times.append(t1 - t0)
    
    # take minimum of runs (standard practice for benchmarking without noise)
    best_time = min(times)
    mean_time = sum(times) / len(times)
    return best_time, mean_time

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--decoder", default="build_msvc/Release/f264dec.exe")
    parser.add_argument("--streams-dir", default="tests/streams")
    parser.add_argument("--runs", type=int, default=3)
    parser.add_argument("--cpuid", type=int, default=None, help="1=SIMD, 0=Generic")
    parser.add_argument("--json", help="Output JSON results file")
    args = parser.parse_args()

    results = []

    print(f"{'Stream':<30} | {'Config':<10} | {'Frames':<7} | {'Time (s)':<10} | {'FPS':<10}")
    print("-" * 75)

    for filename, frames, desc in TEST_STREAMS:
        path = os.path.join(args.streams_dir, filename)
        if not os.path.exists(path):
            continue

        for th, label in [(1, "1T"), (0, "Multi-T")]:
            res = run_bench(args.decoder, path, th, args.cpuid, args.runs)
            if res:
                best_t, mean_t = res
                fps = frames / best_t if best_t > 0 else 0
                print(f"{filename:<30} | {label:<10} | {frames:<7} | {best_t:<10.4f} | {fps:<10.2f}")
                results.append({
                    "stream": filename,
                    "desc": desc,
                    "frames": frames,
                    "threads": label,
                    "best_time_s": best_t,
                    "mean_time_s": mean_t,
                    "fps": fps
                })

    if args.json:
        with open(args.json, "w") as f:
            json.dump(results, f, indent=2)

if __name__ == "__main__":
    main()
