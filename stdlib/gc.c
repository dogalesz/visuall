/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Garbage Collector — Implementation
 *
 * Mark-and-sweep GC with conservative stack scanning.
 *
 * Heap layout:
 *   [ GCHeader | user payload ... ]
 *   ^                ^
 *   header_ptr       user_ptr (returned by __visuall_alloc)
 *
 * All live objects are linked via GCHeader.next into a global intrusive
 * list headed by `heap_head`.
 *
 * Root set:
 *   1. Registered globals       (__visuall_register_global)
 *   2. Conservative stack scan  (stack_bottom → current SP)
 *
 * Limitation (v1): conservative scanning may retain dead objects that
 * look like valid pointers.  This is safe (no false frees) but may
 * delay collection of some garbage.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "gc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Linked list of all live GC objects. */
static GCHeader* heap_head = NULL;

/* Total bytes currently allocated (including headers). */
static size_t heap_bytes = 0;

/* Collection threshold — collect when heap_bytes > threshold. */
static size_t gc_threshold = 1024 * 1024;   /* 1 MB initial */

/* Bottom of the C stack (set once by __visuall_gc_init). */
static void* stack_bottom = NULL;

/* Registered global roots. */
#define MAX_GLOBAL_ROOTS 4096
static void** global_roots[MAX_GLOBAL_ROOTS];
static size_t num_global_roots = 0;

/* Stats tracking. */
static GCStats gc_stats;
static int gc_stats_enabled = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Forward declarations for the VisualList type defined in runtime.c.
   We only need the struct layout to trace child pointers during mark. */
typedef struct {
    int64_t* data;
    int64_t  length;
    int64_t  capacity;
} VisualList_GC;

/* Return current monotonic time in nanoseconds (for pause measurement). */
static double now_ns(void) {
    struct timespec ts;
#ifdef _WIN32
    timespec_get(&ts, TIME_UTC);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* Check whether `ptr` looks like it points into one of our GC objects.
   We walk the full heap list — O(n) per candidate, acceptable for v1. */
static GCHeader* find_gc_header(void* ptr) {
    if (!ptr) return NULL;
    GCHeader* h = heap_head;
    while (h) {
        void* obj_start = (char*)h + sizeof(GCHeader);
        void* obj_end   = (char*)h + h->size;
        if ((char*)ptr >= (char*)obj_start && (char*)ptr < (char*)obj_end) {
            return h;
        }
        h = h->next;
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_gc_init(void* sb) {
    stack_bottom = sb;
    heap_head = NULL;
    heap_bytes = 0;
    gc_threshold = 1024 * 1024;
    num_global_roots = 0;
    memset(&gc_stats, 0, sizeof(gc_stats));
}

void __visuall_gc_enable_stats(int enable) {
    gc_stats_enabled = enable;
}

GCStats __visuall_gc_get_stats(void) {
    return gc_stats;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Allocation
 * ═══════════════════════════════════════════════════════════════════════════ */

void* __visuall_alloc(size_t size, uint8_t type_tag) {
    /* Check if we should collect before allocating. */
    if (heap_bytes + sizeof(GCHeader) + size > gc_threshold) {
        __visuall_collect();
    }

    size_t total = sizeof(GCHeader) + size;
    GCHeader* hdr = (GCHeader*)malloc(total);
    if (!hdr) {
        /* OOM — try collecting and retry once */
        __visuall_collect();
        hdr = (GCHeader*)malloc(total);
        if (!hdr) {
            fprintf(stderr, "out of memory (GC alloc %zu bytes)\n", size);
            exit(1);
        }
    }

    hdr->type_tag = type_tag;
    hdr->marked   = 0;
    hdr->flags    = 0;
    hdr->size     = (uint32_t)total;

    /* Prepend to heap list. */
    hdr->next = heap_head;
    heap_head = hdr;

    heap_bytes += total;
    if (heap_bytes > gc_stats.peak_heap_bytes) {
        gc_stats.peak_heap_bytes = heap_bytes;
    }
    gc_stats.current_live_bytes = heap_bytes;

    /* Zero the user payload. */
    void* user = (char*)hdr + sizeof(GCHeader);
    memset(user, 0, size);
    return user;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Global root registration
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_register_global(void** ptr) {
    if (num_global_roots < MAX_GLOBAL_ROOTS) {
        global_roots[num_global_roots++] = ptr;
    } else {
        fprintf(stderr, "warning: GC global root table full\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Mark phase
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_mark(void* ptr) {
    if (!ptr) return;

    GCHeader* hdr = __visuall_get_header(ptr);
    if (!hdr) return;
    if (hdr->marked) return;        /* already visited — cycle safe */

    hdr->marked = 1;

    /* Recursively trace child pointers based on type. */
    switch (hdr->type_tag) {

    case VSL_TAG_STRING:
        /* Strings have no child GC pointers. */
        break;

    case VSL_TAG_LIST: {
        /* VisualList: { int64_t* data, int64_t length, int64_t capacity }
           Each element may be a pointer (string, list, closure, etc.).
           We conservatively mark anything that looks like a GC pointer. */
        VisualList_GC* list = (VisualList_GC*)ptr;
        for (int64_t i = 0; i < list->length; i++) {
            void* elem = (void*)(uintptr_t)list->data[i];
            GCHeader* eh = find_gc_header(elem);
            if (eh) {
                __visuall_mark(elem);
            }
        }
        break;
    }

    case VSL_TAG_DICT:
        /* Dict not fully implemented yet — no child tracing needed. */
        break;

    case VSL_TAG_CLOSURE: {
        /* Closure fat pointer: { void* env, void* fn_ptr }
           env may point to a GC-allocated struct.  fn_ptr is code, skip. */
        void** fields = (void**)ptr;
        void* env = fields[0];
        if (env) {
            GCHeader* eh = find_gc_header(env);
            if (eh) {
                __visuall_mark(env);
            }
        }
        break;
    }

    case VSL_TAG_OBJECT: {
        /* Class instances: we don't know the layout at runtime, so
           conservatively scan the entire payload for GC pointers. */
        size_t payload = hdr->size - (uint32_t)sizeof(GCHeader);
        size_t nwords = payload / sizeof(void*);
        void** words = (void**)ptr;
        for (size_t i = 0; i < nwords; i++) {
            GCHeader* eh = find_gc_header(words[i]);
            if (eh) {
                __visuall_mark(words[i]);
            }
        }
        break;
    }

    case VSL_TAG_TUPLE: {
        /* Tuples stored as VisualList internally. */
        VisualList_GC* tup = (VisualList_GC*)ptr;
        for (int64_t i = 0; i < tup->length; i++) {
            void* elem = (void*)(uintptr_t)tup->data[i];
            GCHeader* eh = find_gc_header(elem);
            if (eh) {
                __visuall_mark(elem);
            }
        }
        break;
    }

    default:
        break;
    }
}

/* ── Mark all registered global roots ──────────────────────────────────── */
static void mark_global_roots(void) {
    for (size_t i = 0; i < num_global_roots; i++) {
        void* val = *global_roots[i];
        if (val) {
            GCHeader* hdr = find_gc_header(val);
            if (hdr) {
                __visuall_mark(val);
            }
        }
    }
}

/* ── Conservative stack scan ───────────────────────────────────────────── */
static void mark_stack(void) {
    if (!stack_bottom) return;

    /* Get an approximation of the current stack pointer. */
    volatile void* sp;
    sp = (void*)&sp;

    void* lo;
    void* hi;
    if ((uintptr_t)sp < (uintptr_t)stack_bottom) {
        lo = (void*)sp;
        hi = stack_bottom;
    } else {
        lo = stack_bottom;
        hi = (void*)sp;
    }

    /* Walk every word-aligned slot from lo to hi. */
    for (void** p = (void**)lo; (uintptr_t)p < (uintptr_t)hi; p++) {
        void* candidate = *p;
        GCHeader* hdr = find_gc_header(candidate);
        if (hdr) {
            __visuall_mark(candidate);
        }
    }
}

static void mark_roots(void) {
    mark_global_roots();
    mark_stack();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Sweep phase
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Type-specific finalizers — called before freeing. */
static void finalize(GCHeader* hdr) {
    void* user = (char*)hdr + sizeof(GCHeader);
    switch (hdr->type_tag) {
    case VSL_TAG_LIST:
    case VSL_TAG_TUPLE: {
        /* Free the internal data array. */
        VisualList_GC* list = (VisualList_GC*)user;
        if (list->data) {
            free(list->data);
            list->data = NULL;
        }
        break;
    }
    case VSL_TAG_DICT:
        /* Dict hash table cleanup would go here. */
        break;
    default:
        break;
    }
}

static void sweep(void) {
    GCHeader** pp = &heap_head;
    while (*pp) {
        GCHeader* hdr = *pp;
        if (hdr->marked) {
            /* Object is reachable — clear mark for next cycle. */
            hdr->marked = 0;
            pp = &hdr->next;
        } else {
            /* Object is garbage — unlink and free. */
            *pp = hdr->next;

            size_t obj_size = hdr->size;
            heap_bytes -= obj_size;
            gc_stats.total_bytes_collected += obj_size;

            finalize(hdr);
            free(hdr);
        }
    }
    gc_stats.current_live_bytes = heap_bytes;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Collection entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_collect(void) {
    double t0 = now_ns();

    mark_roots();
    sweep();

    /* Adjust threshold: 2× current live bytes, but at least 1 MB. */
    size_t new_threshold = heap_bytes * 2;
    if (new_threshold < 1024 * 1024) new_threshold = 1024 * 1024;
    gc_threshold = new_threshold;

    double t1 = now_ns();
    gc_stats.total_collections++;
    gc_stats.total_pause_ns += (t1 - t0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shutdown — free everything, optionally print stats
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_gc_shutdown(void) {
    /* Free all remaining objects. */
    GCHeader* h = heap_head;
    while (h) {
        GCHeader* next = h->next;
        finalize(h);
        free(h);
        h = next;
    }
    heap_head = NULL;
    heap_bytes = 0;

    if (gc_stats_enabled) {
        fprintf(stderr, "\n=== GC Statistics ===\n");
        fprintf(stderr, "Collections:        %llu\n",
                (unsigned long long)gc_stats.total_collections);
        fprintf(stderr, "Bytes collected:    %llu\n",
                (unsigned long long)gc_stats.total_bytes_collected);
        fprintf(stderr, "Peak heap bytes:    %llu\n",
                (unsigned long long)gc_stats.peak_heap_bytes);
        fprintf(stderr, "Final live bytes:   %llu\n",
                (unsigned long long)gc_stats.current_live_bytes);
        if (gc_stats.total_collections > 0) {
            double avg_us = (gc_stats.total_pause_ns /
                             (double)gc_stats.total_collections) / 1000.0;
            fprintf(stderr, "Avg pause (us):     %.2f\n", avg_us);
        }
        fprintf(stderr, "====================\n");
    }
}
