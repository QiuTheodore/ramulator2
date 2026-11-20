# CS598APE Reproducibility Test Suite

This directory contains synthetic traces, YAML configurations,
and automated validation scripts for the Ramulator 2.0
performance optimization project.

## Contents

- `configs/`: Nine validated simulation configurations.
- `traces/`: Six synthetic input traces.
- `scripts/prepare_configs.py`: Generates portable YAML configurations.
- `scripts/run_correctness.sh`: Compares baseline and optimized outputs.
- `scripts/run_valgrind.sh`: Runs Valgrind on nine configurations.
- `scripts/run_benchmark.py`: Measures execution time and speedup.

## Requirements

- Linux with a C++20-compatible compiler
- CMake
- Python 3
- Valgrind for memory-safety testing

Build the optimized repository in Release mode:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    cmake --build build -j 2

Build the original Ramulator 2.0 separately using the same
Release configuration.

## Configuration Preparation

Run from the repository root:

    python3 tests/cs598ape/scripts/prepare_configs.py

The generator replaces machine-specific trace paths with paths
to the traces included in this repository. Generated files
are excluded from Git.

## Correctness Tests

    bash tests/cs598ape/scripts/run_correctness.sh /path/to/original/build/ramulator2

This compares the standard output of both implementations
for all nine configurations.

## Valgrind Tests

    bash tests/cs598ape/scripts/run_valgrind.sh

The script performs leak checking on the optimized executable
and reports the result for each configuration.

## Performance Benchmarks

    python3 tests/cs598ape/scripts/run_benchmark.py --baseline /path/to/original/build/ramulator2 --runs 6

Three synthetic workloads are measured with alternating
execution order. Median wall-clock execution times, speedups,
and runtime reductions are reported.

Results and logs are saved under `results/`.

## Validation Scope

The final implementation previously passed all nine output
consistency tests and all nine Valgrind checks.

Validation covers the supplied configurations and traces only.
