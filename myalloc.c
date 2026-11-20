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
/* Largest request that cannot overflow align_up(), HDR + size or the intptr_t passed to sbrk() */
#define MAX_REQUEST     ((size_t)INTPTR_MAX - 64UL * 1024UL)

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
static fit_policy_t current_policy = FIT_FIRST;

static void write_msg(int fd, const char *msg) {
    if (msg != NULL) {
        ssize_t r = write(fd, msg, strlen(msg));
        (void)r;
    }
}

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

/* Task 4: Split block if remainder can hold a header + 16-byte payload */
static void split_block(block_t *blk, size_t need) {
    if (blk->size >= need + HDR + MIN_PAYLOAD) {
        block_t *rem = (block_t *)((char *)payload(blk) + need);
        rem->size = blk->size - need - HDR;
        rem->next = blk->next;
        rem->free = 1;
        rem->magic = BLOCK_MAGIC;

        blk->size = need;
        blk->next = rem;
        if (tail == blk) {
            tail = rem;
        }
    }
}

/* Task 3 & Task 7: Search free list according to active placement policy */
static block_t *find_fit(size_t need) {
    if (current_policy == FIT_FIRST) {
        for (block_t *curr = head; curr != NULL; curr = curr->next) {
            if (curr->free && curr->size >= need) {
                return curr;
            }
        }
        return NULL;
    }

    /* FIT_BEST: find smallest free block with size >= need */
    block_t *best = NULL;
    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (curr->free && curr->size >= need) {
            if (best == NULL || curr->size < best->size) {
                best = curr;
                if (best->size == need) {
                    break; /* exact fit */
                }
            }
        }
    }
    return best;
}

/* Task 5: Merge physically adjacent free blocks */
static void coalesce(void) {
    block_t *curr = head;
    while (curr != NULL && curr->next != NULL) {
        if (curr->free && curr->next->free) {
            char *curr_end = (char *)payload(curr) + curr->size;
            if (curr_end == (char *)curr->next) {
                block_t *victim = curr->next;
                curr->size += HDR + victim->size;
                curr->next = victim->next;
                victim->magic = 0;
                if (tail == victim) {
                    tail = curr;
                }
                continue; /* check if new curr->next is also free and adjacent */
            }
        }
        curr = curr->next;
    }
}

void *my_malloc(size_t size) {
    if (size == 0 || size > MAX_REQUEST) {
        return NULL;
    }

    size_t need = align_up(size);
    block_t *blk = find_fit(need);
    if (blk == NULL) {
        blk = grow(need);
        if (blk == NULL) {
            return NULL;
        }
        /* Coalesce in case the new chunk happens to be adjacent to a free tail */
        coalesce();
        blk = find_fit(need);
        if (blk == NULL) {
            return NULL;
        }
    }

    split_block(blk, need);
    blk->free = 0;
    return payload(blk);
}

void my_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }

    /* Verify ptr corresponds to a real block in our list */
    block_t *found = NULL;
    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (payload(curr) == ptr) {
            found = curr;
            break;
        }
    }

    if (found == NULL || found->magic != BLOCK_MAGIC) {
        write_msg(STDERR_FILENO, "my_free: invalid pointer\n");
        return;
    }

    if (found->free) {
        write_msg(STDERR_FILENO, "my_free: double free detected\n");
        return;
    }

    found->free = 1;
    coalesce();
}

void *my_calloc(size_t nmemb, size_t size) {
    if (nmemb == 0 || size == 0) {
        return NULL;
    }
    if (size > SIZE_MAX / nmemb) {
        return NULL;
    }

    size_t total = nmemb * size;
    void *ptr = my_malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *my_realloc(void *ptr, size_t size) {
    if (ptr == NULL) {
        return my_malloc(size);
    }
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    if (size > MAX_REQUEST) {
        return NULL; /* too large; the original block is left untouched */
    }

    block_t *blk = header(ptr);
    if (blk->magic != BLOCK_MAGIC || blk->free) {
        write_msg(STDERR_FILENO, "my_realloc: invalid pointer\n");
        return NULL;
    }

    size_t need = align_up(size);

    /* Case 1: Current block already fits */
    if (need <= blk->size) {
        split_block(blk, need);
        coalesce(); /* the split-off tail may border a free block */
        return ptr;
    }

    /* Case 2: Next block is free, physically adjacent, and big enough */
    if (blk->next != NULL && blk->next->free) {
        char *blk_end = (char *)payload(blk) + blk->size;
        if (blk_end == (char *)blk->next &&
            blk->size + HDR + blk->next->size >= need) {
            block_t *victim = blk->next;
            blk->size += HDR + victim->size;
            blk->next = victim->next;
            victim->magic = 0;
            if (tail == victim) {
                tail = blk;
            }
            split_block(blk, need);
            coalesce();
            return ptr;
        }
    }

    /* Case 3: Allocate elsewhere, copy, and free old block */
    void *new_ptr = my_malloc(size);
    if (new_ptr == NULL) {
        return NULL;
    }

    size_t copy_bytes = (blk->size < size) ? blk->size : size;
    memcpy(new_ptr, ptr, copy_bytes);
    my_free(ptr);
    return new_ptr;
}

void heap_stats(heap_stats_t *out) {
    if (out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));
    out->heap_bytes = total_heap_bytes;

    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        out->blocks++;
        if (curr->free) {
            out->free_blocks++;
            out->free_bytes += curr->size;
            if (curr->size > out->largest_free) {
                out->largest_free = curr->size;
            }
        } else {
            out->used_bytes += curr->size;
        }
    }
}

void heap_dump(void) {
    char buf[128];
    size_t idx = 0;
    for (block_t *curr = head; curr != NULL; curr = curr->next, idx++) {
        int n = snprintf(buf, sizeof(buf), "[%zu] %p size=%zu %s\n",
                         idx, (void *)curr, curr->size,
                         curr->free ? "FREE" : "USED");
        if (n > 0) {
            size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
            ssize_t r = write(STDOUT_FILENO, buf, len);
            (void)r;
        }
    }
}

void set_policy(fit_policy_t p) {
    current_policy = p;
}

int heap_check(void) {
    block_t *prev = NULL;
    for (block_t *curr = head; curr != NULL; curr = curr->next) {
        if (curr->magic != BLOCK_MAGIC) {
            return 0;
        }
        if (((uintptr_t)payload(curr) % ALIGNMENT) != 0) {
            return 0;
        }
        if (prev != NULL && (uintptr_t)curr <= (uintptr_t)prev) {
            return 0;
        }
        if (prev != NULL && prev->free && curr->free) {
            char *prev_end = (char *)payload(prev) + prev->size;
            if (prev_end == (char *)curr) {
                return 0;
            }
        }
        prev = curr;
    }
    return 1;
}