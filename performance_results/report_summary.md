# Final Ramulator Performance Comparison

Measurements: CMake Release builds, one warmup and repeated rotated A/B/C order; medians of wall-clock elapsed time and peak RSS.

| Workload | Version | Median time (s) | Speedup vs original | Runtime reduction | Median peak RSS (MB) | RSS reduction |
|---|---|---:|---:|---:|---:|---:|
| unique_memory_long | ORIGINAL_BASELINE | 4.835 | 1.000x | 0.0% | 131.0 | 0.0% |
| unique_memory_long | V5_ALL | 3.145 | 1.537x | 35.0% | 131.2 | -0.1% |
| unique_memory_long | V6_CLEANUP | 2.830 | 1.708x | 41.5% | 35.0 | 73.3% |

| mixed_memory_long | ORIGINAL_BASELINE | 0.685 | 1.000x | 0.0% | 34.3 | 0.0% |
| mixed_memory_long | V5_ALL | 0.480 | 1.427x | 29.9% | 34.6 | -0.8% |
| mixed_memory_long | V6_CLEANUP | 0.425 | 1.612x | 38.0% | 21.4 | 37.7% |

| write_mix_long | ORIGINAL_BASELINE | 1.670 | 1.000x | 0.0% | 48.6 | 0.0% |
| write_mix_long | V5_ALL | 1.180 | 1.415x | 29.3% | 48.9 | -0.6% |
| write_mix_long | V6_CLEANUP | 1.030 | 1.621x | 38.3% | 16.7 | 65.6% |

All checked configurations produced exactly matching complete stdout. This is not
equivalent to validating all possible DRAM timing or callback behaviors.

**Limitations:** synthetic trace workloads, one machine, relatively short runs,
timing noise, and no guarantee the speedups generalize to other traces or DDR configurations.
Report these results as measured experimental findings, not universal claims.
