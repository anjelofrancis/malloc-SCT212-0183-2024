# myalloc – A malloc/free Implementation Using sbrk()

**Name:** Anjelo Oyolla  
**Student ID:** SCT212-0183/2024  
**Course:** ICS 2305 Systems Programming – Practical Assignment, Question 2

## Build and run

Built and tested on Ubuntu Linux with gcc.

```
make               # builds and runs the provided test harness (test_myalloc)
make bench         # builds the Task 8 benchmark
./bench            # runs FIT_FIRST, FIT_BEST and glibc on the same workload
make clean         # removes build output
```

AddressSanitizer / UBSan build (Section 7.2):

```
gcc -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
./t_asan
```

## Tasks

| Task | Description | Status |
|------|-------------|--------|
| 1 | Block header, `align_up`, `payload`, `header`, `HDR` | Complete |
| 2 | `grow()` – extend the heap with `sbrk()` (64 KB minimum, aligned) | Complete |
| 3 | `my_malloc` with first-fit | Complete |
| 4 | Block splitting | Complete |
| 5 | `my_free` with invalid-pointer / double-free detection and coalescing | Complete |
| 6 | `my_calloc` (overflow check) and `my_realloc` (all five cases) | Complete |
| 7 | Best-fit policy, `heap_stats`, `heap_dump`, `heap_check` | Complete |
| 8 | `bench.c` and `REPORT.md` | Complete |

## Test results

Last lines of `make` (Ubuntu VM):

```
T7 best-fit vs first-fit
  PASS  first-fit takes the first hole
  PASS  best-fit takes the tightest hole
T8 randomized stress test
  PASS  no data corruption during 20000 operations
  PASS  everything freed
  PASS  fully coalesced (few free blocks remain)
  PASS  heap consistent

Result: 29 passed, 0 failed
```

The AddressSanitizer + UBSan build also passes all 29 checks with no sanitizer errors.

## Design explanation

### 1. Header layout

Every block starts with a `block_t` header with four fields: `size` (payload bytes, excluding the header), `next` (the next block in address order), `free` (1 = free, 0 = in use) and `magic` (`0x5A110C42`, used to detect invalid pointers). On 64-bit Linux, `sizeof(block_t)` is 24 bytes, so `HDR = align_up(sizeof(block_t))` rounds it up to **32 bytes**. Because `HDR` and every payload size are multiples of 16, every payload lands on a 16-byte boundary. `payload(b)` returns `(char *)b + HDR` and `header(ptr)` returns `(char *)ptr - HDR`, using `char *` so the arithmetic is in bytes.

### 2. List organisation

All blocks, used and free, are kept in one singly linked list in increasing address order, with global `head` and `tail` pointers. Address order makes physically adjacent neighbours easy to find during coalescing. `my_malloc()` searches the list with first-fit (the first free block with `size >= need`) or best-fit (the smallest such block). If the chosen block has room for another header plus 16 bytes, `split_block()` turns the remainder into a new free block right after it, updating `tail` if needed. If nothing fits, `grow()` gets at least 64 KB from `sbrk()`, pads the break to 16-byte alignment and appends the new free block at `tail`.

### 3. Coalescing

After `my_free()` validates the pointer and marks the block free, `coalesce()` walks the list and merges two consecutive free blocks only if `(char *)payload(curr) + curr->size == (char *)curr->next`. A merge adds `HDR + next->size` to `curr->size`, unlinks the neighbour and updates `tail`; the loop stays on `curr`, so runs of three or more free blocks merge in one pass. Blocks separated by memory that libc took with `sbrk()` fail the address check and are never merged.

## Questions

**1. Why must the header size be a multiple of 16 as well as the payload sizes?**

<2–4 sentences>

**2. Why can `printf()` inside `my_malloc()` cause infinite recursion?**

<2–4 sentences>

**3. Your `my_free()` is O(n). Which extra field would let you find a block's previous neighbour in O(1), and what is the name of the technique?**

<2–4 sentences>

**4. Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.**

<2–4 sentences>

**5. Why does `sbrk()` memory never get returned to the OS in your allocator? Describe how you could give back memory at the end of the heap.**

<2–4 sentences>

**6. Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?**

<2–4 sentences>

## Known limitations

- `my_free()` and `coalesce()` walk the whole list, so freeing is O(n); best-fit is also O(n) per allocation.
- Memory obtained with `sbrk()` is never given back to the OS, even when the top of the heap is free.
- Not thread-safe: there is one global list with no locking.
- `my_realloc()` checks the header's `magic` directly instead of searching the list, so passing it a pointer that was never allocated can still read invalid memory.
- Requests larger than `INTPTR_MAX - 64 KB` are rejected (return `NULL`).

## Sources and AI-tool declaration

- Linux man pages: `sbrk(2)` / `brk(2)`, `write(2)`, `clock_gettime(2)`, `fork(2)`, `waitpid(2)`.
- <any websites, notes or books you used, and what for>
- **Claude Code (Anthropic AI assistant)** was used to: set up an Ubuntu VM for building and testing; review and test my code (test harness, AddressSanitizer, extra edge-case tests); split my Task 1–7 work into separate commits; fix two bugs it found (coalescing after a split in `my_realloc`, and rejecting sizes that would overflow); change `bench.c` to run each policy in its own forked process and add the `bench` Makefile target; shorten and correct the `REPORT.md` discussion; and provide this README template. <edit to match what you actually used it for>
