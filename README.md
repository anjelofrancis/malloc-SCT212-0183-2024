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

The pointer returned to the user is `block + HDR`, and the next header starts at `payload + size`. If only the payload sizes were multiples of 16 but `HDR` were 24 (the raw `sizeof(block_t)`), a block starting on a 16-byte boundary would give a payload that is only 8-byte aligned. Keeping `HDR` (32) and every size a multiple of 16 means every header and every payload address stays 16-byte aligned, which is what malloc must guarantee on 64-bit Linux.

**2. Why can `printf()` inside `my_malloc()` cause infinite recursion?**

`printf()` uses stdio, which may call `malloc()` internally, for example to allocate the buffer for `stdout` the first time it prints. If that `malloc()` were ours, or if the allocator were re-entered while its list is half-updated, `my_malloc()` would call `printf()`, which would call `malloc()` again, and so on until the stack overflows or the list is corrupted. That is why the allocator only formats text with `snprintf()` into a local array and prints it with `write()`.

**3. Your `my_free()` is O(n). Which extra field would let you find a block's previous neighbour in O(1), and what is the name of the technique?**

Adding a footer at the end of every block (a copy of its `size` and free flag), or a `prev` pointer in the header, would let `my_free()` find the previous block directly. With a footer, the word just before a block's header is the previous block's footer, so the previous neighbour's start can be computed in O(1) instead of walking the list. This technique is called boundary tags (from Knuth).

**4. Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.**

Internal fragmentation is unused space inside an allocated block, while external fragmentation is free memory that is split into separate holes, so a large request can fail even though enough total memory is free. In `myalloc`, internal fragmentation comes from `align_up()` rounding every request up to 16 bytes (a 1-byte request uses 16) and from `split_block()` giving away the whole block when the leftover is smaller than `HDR + 16`. External fragmentation comes from freed blocks between used ones that cannot be merged; in the benchmark it reached about 49.6 % with first-fit.

**5. Why does `sbrk()` memory never get returned to the OS in your allocator? Describe how you could give back memory at the end of the heap.**

Memory is never returned because the allocator only ever calls `sbrk()` with a positive increment; `my_free()` just marks blocks free and merges them. To give memory back, after coalescing I could check whether the `tail` block is free and ends exactly at the current break (`payload(tail) + tail->size == sbrk(0)`, meaning libc has not moved the break since). If so, call `sbrk(-(HDR + tail->size))`, remove the tail from the list, and subtract that amount from the heap total.

**6. Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?**

The C library's own `malloc()` also moves the program break, for example when `printf()` in the test harness allocates its output buffer. So between two calls to `grow()` some memory that does not belong to my allocator can appear, and the new chunk is appended to the list even though it does not start where the old tail ended. `coalesce()` only merges two blocks when `(char *)payload(curr) + curr->size == (char *)curr->next`, so blocks with a gap between them fail this address check and are left as separate blocks.

## Known limitations

- `my_free()` and `coalesce()` walk the whole list, so freeing is O(n); best-fit is also O(n) per allocation.
- Memory obtained with `sbrk()` is never given back to the OS, even when the top of the heap is free.
- Not thread-safe: there is one global list with no locking.
- `my_realloc()` checks the header's `magic` directly instead of searching the list, so passing it a pointer that was never allocated can still read invalid memory.
- Requests larger than `INTPTR_MAX - 64 KB` are rejected (return `NULL`).

## Sources and AI-tool declaration

- Linux man pages: `sbrk(2)` / `brk(2)`, `write(2)`, `clock_gettime(2)`, `fork(2)`, `waitpid(2)`.
- **Claude Code (Anthropic AI assistant)** was used to: set up an Ubuntu VM for building and testing; review and test my self written code; draft the answers to the six written questions (willingly); fix two bugs it found (coalescing after a split in `my_realloc`, and rejecting sizes that would overflow); add the `bench` Makefile target; shorten and correct the `REPORT.md` discussion; and provide this README template.
