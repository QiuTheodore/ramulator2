#!/usr/bin/env python3

import argparse
import csv
import os
import statistics
import subprocess
import time
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parents[1]
REPO_DIR = TEST_DIR.parent.parent

TESTS = ["unique_long", "mixed_long", "write_long"]


def measure(binary, config):
    start = time.perf_counter()

    process = subprocess.run(
        [str(binary), "-f", str(config)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )

    elapsed = time.perf_counter() - start

    if process.returncode != 0:
        raise RuntimeError(
            f"{binary.name} failed:\n{process.stderr}"
        )

    return elapsed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True)
    parser.add_argument(
        "--optimized",
        default=str(REPO_DIR / "build/ramulator2"),
    )
    parser.add_argument("--runs", type=int, default=6)
    args = parser.parse_args()

    if args.runs < 1:
        parser.error("--runs must be positive")

    binaries = {
        "Original": Path(args.baseline).expanduser().resolve(),
        "Optimized": Path(args.optimized).expanduser().resolve(),
    }

    for label, binary in binaries.items():
        if not binary.is_file() or not os.access(binary, os.X_OK):
            parser.error(f"{label} executable not found: {binary}")

    subprocess.run(
        [
            "python3",
            str(TEST_DIR / "scripts/prepare_configs.py"),
        ],
        check=True,
    )

    out_dir = TEST_DIR / "results"
    out_dir.mkdir(parents=True, exist_ok=True)

    raw_rows = []
    summary_rows = []

    for test in TESTS:
        config = TEST_DIR / "generated" / f"{test}.yaml"
        results = {"Original": [], "Optimized": []}

        for i in range(args.runs):
            order = ["Original", "Optimized"]

            if i % 2:
                order.reverse()

            for label in order:
                seconds = measure(binaries[label], config)
                results[label].append(seconds)

                raw_rows.append({
                    "workload": test,
                    "run": i + 1,
                    "implementation": label,
                    "seconds": f"{seconds:.6f}",
                })

        baseline = statistics.median(results["Original"])
        optimized = statistics.median(results["Optimized"])

        speedup = baseline / optimized
        reduction = (1 - optimized / baseline) * 100

        summary_rows.append({
            "workload": test,
            "original_seconds": f"{baseline:.6f}",
            "optimized_seconds": f"{optimized:.6f}",
            "speedup": f"{speedup:.4f}",
            "runtime_reduction_percent": f"{reduction:.2f}",
        })

        print(f"\n{test}")
        print(f"  Original:  {baseline:.3f}s")
        print(f"  Optimized: {optimized:.3f}s")
        print(f"  Speedup:   {speedup:.3f}x")
        print(f"  Reduction: {reduction:.1f}%")

    outputs = [
        ("benchmark_raw.csv", raw_rows),
        ("benchmark_summary.csv", summary_rows),
    ]

    for filename, rows in outputs:
        path = out_dir / filename

        with path.open("w", newline="") as f:
            writer = csv.DictWriter(
                f, fieldnames=list(rows[0].keys())
            )
            writer.writeheader()
            writer.writerows(rows)

        print(f"\nSaved: {path}")


if __name__ == "__main__":
    main()
