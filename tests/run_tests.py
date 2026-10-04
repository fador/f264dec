#!/usr/bin/env python3
import argparse
import glob
import hashlib
import os
import subprocess
import sys
import tempfile
import time

def calc_md5(filepath):
    h = hashlib.md5()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()

def main():
    parser = argparse.ArgumentParser(description="Run regression tests against reference streams")
    parser.add_argument("--decoder", required=True, help="Path to f264dec binary")
    parser.add_argument("--streams-dir", required=True, help="Path to streams directory")
    parser.add_argument("-v", "--verbose", action="store_true", help="Verbose output")
    args = parser.parse_args()

    streams = sorted(glob.glob(os.path.join(args.streams_dir, "*.264")))
    if not streams:
        print(f"No .264 streams found in {args.streams_dir}")
        return 1

    print(f"Running regression tests on {len(streams)} streams using {args.decoder}...")
    passed = 0
    failed = 0
    skipped = 0

    with tempfile.TemporaryDirectory() as tmpdir:
        for stream in streams:
            name = os.path.splitext(os.path.basename(stream))[0]
            md5_file = os.path.join(args.streams_dir, f"{name}.md5")
            if not os.path.exists(md5_file):
                print(f"SKIP {name} (no .md5 reference)")
                skipped += 1
                continue

            with open(md5_file, "r") as f:
                expected_md5 = f.read().strip().split()[0]

            out_yuv = os.path.join(tmpdir, f"{name}.yuv")

            t0 = time.time()
            proc = subprocess.run(
                [args.decoder, "-s", "-i", stream, "-o", out_yuv],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            dt = time.time() - t0

            if proc.returncode != 0 or not os.path.exists(out_yuv):
                print(f"FAIL {name}: decoder exited with code {proc.returncode}")
                if args.verbose or proc.returncode != 0:
                    print("  stderr:", proc.stderr.strip()[:300])
                failed += 1
                continue

            actual_md5 = calc_md5(out_yuv)
            os.remove(out_yuv)

            if actual_md5.lower() == expected_md5.lower():
                print(f"PASS {name} ({dt:.2f}s)")
                passed += 1
            else:
                print(f"FAIL {name}: MD5 mismatch (got {actual_md5}, expected {expected_md5})")
                failed += 1

    print(f"\nResults: {passed} passed, {failed} failed, {skipped} skipped out of {len(streams)} total.")
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
