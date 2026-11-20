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
