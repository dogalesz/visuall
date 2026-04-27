/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Garbage Collector — Header
 *
 * Mark-and-sweep GC for all Visuall heap objects (strings, lists, dicts,
 * closures, tuples, class instances).  Every allocation goes through
 * __visuall_alloc() which prepends a GCHeader.  Collection is triggered
 * automatically when total allocated bytes exceed a dynamic threshold.
 *
 * v1 uses conservative stack scanning — no gcroot intrinsics required.
 * ════════════════════════════════════════════════════════════════════════════ */

#ifndef VISUALL_GC_H
#define VISUALL_GC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Type tags — identify heap object kind for tracing
 * ═══════════════════════════════════════════════════════════════════════════ */
#define VSL_TAG_STRING   1
#define VSL_TAG_LIST     2
#define VSL_TAG_DICT     3
#define VSL_TAG_CLOSURE  4
#define VSL_TAG_OBJECT   5
#define VSL_TAG_TUPLE    6
#define VSL_TAG_BOXED    7   /* single-element heap box for byReference captures */

/* ═══════════════════════════════════════════════════════════════════════════
 * GCHeader — prefixed to every GC-managed allocation
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct GCHeader {
    uint8_t         type_tag;   /* VSL_TAG_* constant                     */
    uint8_t         marked;     /* 1 = reachable, 0 = garbage             */
    uint16_t        flags;      /* reserved for future use                */
    uint32_t        size;       /* total allocation size (header + body)  */
    uint32_t        field_count;/* VSL_TAG_OBJECT: # GC-traced fields     */
    struct GCHeader* next;      /* intrusive linked list of all objects   */
} GCHeader;

/* ═══════════════════════════════════════════════════════════════════════════
 * GC Statistics — exposed via --gc-stats
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    uint64_t total_collections;
    uint64_t total_bytes_collected;
    uint64_t current_live_bytes;
    uint64_t peak_heap_bytes;
    double   total_pause_ns;        /* nanoseconds spent in collect()     */
} GCStats;

/* ═══════════════════════════════════════════════════════════════════════════
 * Public API — called from generated code and runtime.c
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Allocate `size` bytes of GC-managed memory.  Returns a pointer past the
   header (the user-visible pointer).  Triggers collection if threshold
   exceeded. */
void* __visuall_alloc(size_t size, uint8_t type_tag);

/* Allocate a class instance of `payload` bytes with `field_count` GC-traced
   fields whose byte offsets (from the user pointer) are in `field_offsets`.
   The offsets array is copied to the tail of the allocation (after payload).
   Returns the user-visible pointer (past the GCHeader). */
void* __visuall_alloc_object(size_t payload, uint32_t field_count,
                              const uint32_t* field_offsets);

/* Register a global variable as a GC root.  `ptr` must point to a
   location that holds a GC-managed pointer (or NULL). */
void __visuall_register_global(void** ptr);

/* Run a full mark-and-sweep collection. */
void __visuall_collect(void);

/* ── Mark-phase helpers (also usable from runtime finalizers) ────────── */
void __visuall_mark(void* ptr);

/* ── Lifecycle ──────────────────────────────────────────────────────── */

/* Initialise the GC.  Call once at program start.
   `stack_bottom` is an address near the bottom of the C stack (e.g.
   address of a local in main()). */
void __visuall_gc_init(void* stack_bottom);

/* Shut down the GC: free every remaining object and print stats if
   gc_stats_enabled is set. */
void __visuall_gc_shutdown(void);

/* Enable/disable stats reporting at shutdown. */
void __visuall_gc_enable_stats(int enable);

/* Retrieve a copy of the current stats. */
GCStats __visuall_gc_get_stats(void);

/* Get the GCHeader for a user-visible pointer.  Returns NULL if ptr is
   NULL.  Caller must ensure ptr was returned by __visuall_alloc(). */
static inline GCHeader* __visuall_get_header(void* ptr) {
    if (!ptr) return NULL;
    return (GCHeader*)((char*)ptr - sizeof(GCHeader));
}

#ifdef __cplusplus
}
#endif

#endif /* VISUALL_GC_H */
