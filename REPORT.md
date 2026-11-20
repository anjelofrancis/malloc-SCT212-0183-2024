# Task 8 – Benchmark Report: First-fit vs Best-fit vs glibc

**Name:** Anjelo Oyolla  
**Student ID:** SCT212-0183/2024

## Setup

- **Workload:** 50,000 random operations on 512 slots, from `bench.c`. For each operation a random slot is picked: if it is empty, 1–4096 bytes are allocated; if it is full, the block is freed.
- **Seed:** `srand(42)` before every run, so all three runs see the same sequence.
- **Runs:** `FIT_FIRST` and `FIT_BEST` with `my_malloc`/`my_free`, and the same workload with glibc `malloc`/`free`. Each run is done in its own `fork()`ed child process so it starts with a fresh heap.
- **Measurements:** time with `clock_gettime(CLOCK_MONOTONIC)`. Heap figures come from `heap_stats()` at the end of the 50,000 operations, *before* the remaining live blocks are freed.
- **Environment:** Ubuntu Linux (VM), gcc 15.2, built with `make bench` (`-O2`). Times are the median of 3 runs; the heap figures were identical in every run.

Build and run:

```
make bench
./bench
```

## Results

| Policy | Time (ms) | heap_bytes | free_bytes | free_blocks | largest_free | External fragmentation |
|--------|----------:|-----------:|-----------:|------------:|-------------:|-----------------------:|
| FIT_FIRST | 23.1 | 720,896 | 152,256 | 110 | 76,800 | 0.4956 (49.6 %) |
| FIT_BEST  | 29.0 | 720,896 | 152,448 | 98  | 77,280 | 0.4931 (49.3 %) |
| glibc malloc/free | 1.6 | – | – | – | – | – |

External fragmentation = `1 - largest_free / free_bytes`.

## Discussion

### Choice of policy

I would choose **FIT_FIRST**. Both policies needed the same heap (720,896 bytes), and FIT_BEST only reduced the free blocks from 110 to 98 and external fragmentation from 49.6 % to 49.3 %. But FIT_BEST scans the whole list on every allocation, so it took 29.0 ms against 23.1 ms (about 1.26× slower). First-fit gives the better trade-off.

### Why `myalloc` is slower and less space-efficient than glibc

glibc took 1.6 ms, about 14× faster than FIT_FIRST.

- **Speed:** `myalloc` keeps used and free blocks in one unindexed list, so `find_fit()` and `coalesce()` are O(n). glibc uses thread-local caches (tcache) and size-segregated bins, so most operations are close to O(1).
- **Space:** every `myalloc` block carries a 32-byte header, and free memory at the top of the heap is never returned with a negative `sbrk()`. glibc uses a smaller chunk header and trims the top of the heap back to the OS.
