/*
 * GC interior-pointer resolution benchmark.
 * Triggers actual GC collections and measures pause time via gc_stats.
 *
 * Strategy:
 *   1. Allocate many objects to build up a large heap.
 *   2. Use recursive calls to create a deep stack (many stack slots for mark_stack).
 *   3. Trigger a GC and measure pause time.
 *
 * The key difference:
 *   OLD: mark_stack calls find_gc_header() for every stack word.
 *        Interior pointers cause O(N) linear heap-list walk.
 *   NEW: Chunk table resolves interior pointers in O(1).
 *
 * Compile:
 *   gcc -O2 -I../stdlib benchmarks/gc_bench.c stdlib/gc.c stdlib/scheduler.c
 *       -o gc_bench -lpthread
 *
 * Run: ./gc_bench <num_objects> <stack_depth>
 */

#include "gc.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── wall-clock timer ─────────────────────────────────────────────────── */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static double now_ns(void) {
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1e9 / (double)freq.QuadPart;
}
#else
static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
#endif

/* ── Recursive stack-filler ───────────────────────────────────────────── */
/*
 * Creates a deep stack frame with many live GC pointers in local variables,
 * then triggers a garbage collection.  mark_stack() will scan all these
 * frames, calling find_gc_header() for every word-aligned slot.
 *
 * Many of those slots contain interior pointers (e.g., pointers into the
 * middle of string payloads), which exercise the slow path.
 */

static void** g_objects = NULL;
static int    g_num_objects = 0;
static int    g_max_depth = 0;

static void recurse_and_collect(int depth, int max_depth) {
    if (depth >= max_depth) {
        /* At the bottom: trigger a collection.
         * The mark_stack will scan all stack frames above us. */
        double t0 = now_ns();
        __visuall_collect();
        double t1 = now_ns();
        printf("  collect at depth %d: %.3f ms\n", depth, (t1 - t0) / 1e6);
        return;
    }

    /*
     * Hold several heap pointers in locals so mark_stack has work to do.
     * Use interior pointers (ptr + offset) to stress the slow path.
     * Spread across the allocation range so they can't all be cache-hot.
     */
    void* a = (void*)((char*)g_objects[(depth * 7)     % g_num_objects] + 8);
    void* b = (void*)((char*)g_objects[(depth * 7 + 1) % g_num_objects] + 16);
    void* c = (void*)((char*)g_objects[(depth * 7 + 2) % g_num_objects] + 4);
    void* d = (void*)((char*)g_objects[(depth * 7 + 3) % g_num_objects] + 12);
    void* e = (void*)((char*)g_objects[(depth * 7 + 4) % g_num_objects] + 20);

    /* Volatile to prevent the compiler from optimizing them away. */
    volatile void* va = a;
    volatile void* vb = b;
    volatile void* vc = c;
    volatile void* vd = d;
    volatile void* ve = e;

    recurse_and_collect(depth + 1, max_depth);

    /*
     * Touch the volatiles after the recursive call to prevent tail-call
     * optimization and ensure these pointers stay live across the call.
     */
    (void)va; (void)vb; (void)vc; (void)vd; (void)ve;
}

int main(int argc, char** argv) {
    int num_objects = (argc > 1) ? atoi(argv[1]) : 20000;
    int stack_depth = (argc > 2) ? atoi(argv[2]) : 200;

    printf("=== GC Interior-Pointer Benchmark ===\n");
    printf("  heap objects: %d\n", num_objects);
    printf("  stack depth:  %d\n", stack_depth);

    /* Init GC. */
    int dummy;
    __visuall_gc_init(&dummy);
    __visuall_gc_enable_stats(1);

    /* Allocate objects — fixed 56-byte payloads (80 total w/ header). */
    g_objects = (void**)malloc(num_objects * sizeof(void*));
    g_num_objects = num_objects;
    for (int i = 0; i < num_objects; i++) {
        g_objects[i] = __visuall_alloc(56, VSL_TAG_STRING);
        /* Write some data to ensure pages are touched. */
        memset(g_objects[i], 0xAA, 56);
    }

    printf("  heap bytes before: %zu\n",
           __visuall_gc_get_stats().current_live_bytes);

    /* Keep every 10th object as a global root to prevent sweeping them all.
     * Without roots, the entire heap would be swept and we'd measure
     * sweep time, not mark time. */
    for (int i = 0; i < num_objects; i += 10) {
        __visuall_register_global(&g_objects[i]);
    }

    /* ── Benchmark: deep stack → GC collection ── */
    double t0 = now_ns();
    recurse_and_collect(0, stack_depth);
    double t1 = now_ns();

    GCStats stats = __visuall_gc_get_stats();
    printf("\n=== Results ===\n");
    printf("  collections:        %llu\n",
           (unsigned long long)stats.total_collections);
    printf("  total pause:        %.3f ms\n",
           stats.total_pause_ns / 1e6);
    printf("  avg pause:          %.3f us\n",
           (stats.total_pause_ns / (double)stats.total_collections) / 1000.0);
    printf("  bytes collected:    %llu\n",
           (unsigned long long)stats.total_bytes_collected);
    printf("  peak heap:          %llu\n",
           (unsigned long long)stats.peak_heap_bytes);
    printf("  wall clock total:   %.3f ms\n", (t1 - t0) / 1e6);

    /* Cleanup. */
    free(g_objects);
    __visuall_gc_shutdown();

    return 0;
}
