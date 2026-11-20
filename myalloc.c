#define _DEFAULT_SOURCE
#include "myalloc.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define ALIGNMENT       16UL
#define MIN_GROW_BYTES  (64UL * 1024UL)
#define MIN_PAYLOAD     16UL
#define BLOCK_MAGIC     0x5A110C42

typedef struct block {
    size_t size;          /* payload size in bytes */
    struct block *next;   /* next block in increasing address order */
    int free;             /* 1 = free, 0 = in use */
    int magic;            /* constant to detect bad pointers */
} block_t;

static inline size_t align_up(size_t n) {
    return (n + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

#define HDR (align_up(sizeof(block_t)))

static inline void *payload(block_t *b) {
    return (void *)((char *)b + HDR);
}

static inline block_t *header(void *ptr) {
    return (block_t *)((char *)ptr - HDR);
}

static block_t *head = NULL;
static block_t *tail = NULL;
static size_t total_heap_bytes = 0;

/* Task 2: Request raw memory from the OS via sbrk() */
static block_t *grow(size_t need) {
    void *cur = sbrk(0);
    if (cur == (void *)-1) {
        return NULL;
    }

    uintptr_t addr = (uintptr_t)cur;
    uintptr_t aligned = (addr + (ALIGNMENT - 1)) & ~(uintptr_t)(ALIGNMENT - 1);
    size_t pad = (size_t)(aligned - addr);

    size_t req_chunk = HDR + need;
    if (req_chunk < MIN_GROW_BYTES) {
        req_chunk = MIN_GROW_BYTES;
    }
    req_chunk = align_up(req_chunk);

    size_t total_inc = pad + req_chunk;
    void *prev = sbrk((intptr_t)total_inc);
    if (prev == (void *)-1) {
        return NULL;
    }
    total_heap_bytes += total_inc;

    block_t *blk = (block_t *)((char *)prev + pad);
    blk->size = req_chunk - HDR;
    blk->next = NULL;
    blk->free = 1;
    blk->magic = BLOCK_MAGIC;

    if (head == NULL) {
        head = blk;
        tail = blk;
    } else {
        tail->next = blk;
        tail = blk;
    }

    return blk;
}
