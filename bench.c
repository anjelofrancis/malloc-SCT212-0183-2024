#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "myalloc.h"

#define NUM_OPS   50000
#define SLOTS     512
#define MAX_SIZE  4096
#define SEED      42

static double elapsed_ms(struct timespec start, struct timespec end) {
    return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
           (double)(end.tv_nsec - start.tv_nsec) / 1e6;
}
static void run_myalloc_bench(fit_policy_t policy, const char *label) {
    void *ptrs[SLOTS] = {0};
    set_policy(policy);
    srand(SEED);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % SLOTS;
        if (ptrs[idx] != NULL) {
            my_free(ptrs[idx]);
            ptrs[idx] = NULL;
        } else {
            size_t sz = 1 + (size_t)(rand() % MAX_SIZE);
            ptrs[idx] = my_malloc(sz);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    heap_stats_t st;
    heap_stats(&st);

    double ext_frag = (st.free_bytes > 0)
        ? 1.0 - ((double)st.largest_free / (double)st.free_bytes)
        : 0.0;

    printf("=== %s ===\n", label);
    printf("Time (ms)      : %.3f\n", elapsed_ms(t0, t1));
    printf("heap_bytes     : %zu\n", st.heap_bytes);
    printf("free_bytes     : %zu\n", st.free_bytes);
    printf("free_blocks    : %zu\n", st.free_blocks);
    printf("largest_free   : %zu\n", st.largest_free);
    printf("ext_frag       : %.4f (%.2f%%)\n\n", ext_frag, ext_frag * 100.0);

    for (int i = 0; i < SLOTS; i++) {
        if (ptrs[i] != NULL) {
            my_free(ptrs[i]);
            ptrs[i] = NULL;
        }
    }
}

static void run_glibc_bench(void) {
    void *ptrs[SLOTS] = {0};
    srand(SEED);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < NUM_OPS; i++) {
        int idx = rand() % SLOTS;
        if (ptrs[idx] != NULL) {
            free(ptrs[idx]);
            ptrs[idx] = NULL;
        } else {
            size_t sz = 1 + (size_t)(rand() % MAX_SIZE);
            ptrs[idx] = malloc(sz);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("=== glibc malloc/free ===\n");
    printf("Time (ms)      : %.3f\n\n", elapsed_ms(t0, t1));

    for (int i = 0; i < SLOTS; i++) {
        if (ptrs[i] != NULL) {
            free(ptrs[i]);
        }
    }
}

/* Run one benchmark in a child process so each run starts with a fresh heap. */
static void run_in_child(int which) {
    fflush(stdout);                 /* don't let the child re-print buffered text */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    if (pid == 0) {                 /* child */
        if (which == 0)      run_myalloc_bench(FIT_FIRST, "FIT_FIRST");
        else if (which == 1) run_myalloc_bench(FIT_BEST, "FIT_BEST");
        else                 run_glibc_bench();
        fflush(stdout);
        _exit(0);
    }
    waitpid(pid, NULL, 0);          /* parent waits before starting the next run */
}

int main(void) {
    run_in_child(0);   /* FIT_FIRST */
    run_in_child(1);   /* FIT_BEST  */
    run_in_child(2);   /* glibc     */
    return 0;
}