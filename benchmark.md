# Benchmark Results

## Dataset
- 10k documents

## Index Build Time
Measured using `std::chrono::high_resolution_clock`.

- 10k documents (total build): **1035 ms**
- Single-thread indexing (10k): **67 ms**
- Multi-thread indexing (10k): **45 ms**

## Query Latency
Measured using automated in-memory benchmark (10 runs per query).

- Average phrase query latency (10k docs): **~1 µs**

## Multithreading Impact
- Single-thread indexing: **67 ms**
- Multi-thread indexing: **45 ms**
- Speedup: **~1.49x**

## Notes
- Query latency benchmark excludes console I/O.
- Interactive query latency (~1400 ms) is dominated by user input and output printing.
