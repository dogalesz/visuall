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
#include "scheduler.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static CRITICAL_SECTION gc_lock;
#define GC_LOCK()   EnterCriticalSection(&gc_lock)
#define GC_UNLOCK() LeaveCriticalSection(&gc_lock)
#else
#include <pthread.h>
static pthread_mutex_t gc_lock;
#define GC_LOCK()   pthread_mutex_lock(&gc_lock)
#define GC_UNLOCK() pthread_mutex_unlock(&gc_lock)
#endif
static int gc_lock_initialized = 0;

/* Global lock API */
void __visuall_gc_lock(void) { GC_LOCK(); }
void __visuall_gc_unlock(void) { GC_UNLOCK(); }

/* Linked list of all live GC objects. */
static GCHeader* heap_head = NULL;

/* Total bytes currently allocated (including headers). */
static size_t heap_bytes = 0;

/* Collection threshold — collect when heap_bytes > threshold. */
static size_t gc_threshold = 1 * 1024 * 1024;   /* 1 MB initial */

/* Bottom of the C stack (set once by __visuall_gc_init). */
static void* stack_bottom = NULL;

/* Registered global roots. */
#define MAX_GLOBAL_ROOTS 4096
static void** global_roots[MAX_GLOBAL_ROOTS];
static size_t num_global_roots = 0;

/* Stats tracking. */
static GCStats gc_stats;
static int gc_stats_enabled = 0;

/* Heap address range — quickly rejects non-heap stack candidates. */
static uintptr_t heap_lo = UINTPTR_MAX;
static uintptr_t heap_hi = 0;

/* ═══════════════════════════════════════════════════════════════════════════
 * Free-list pool for small allocations — avoids malloc/free overhead
 *
 * Bucketed by size class: 32, 64, 128, 256 bytes (total incl. header).
 * During sweep, dead small objects go to the free list instead of free().
 * During alloc, check the free list before calling malloc().
 * ═══════════════════════════════════════════════════════════════════════════ */

#define FL_NUM_CLASSES   4
#define FL_MAX_PER_CLASS 32768

static const size_t fl_class_sizes[FL_NUM_CLASSES] = { 32, 64, 128, 256 };
static GCHeader*    fl_heads[FL_NUM_CLASSES]  = { NULL, NULL, NULL, NULL };
static size_t       fl_counts[FL_NUM_CLASSES] = { 0, 0, 0, 0 };

/* Find the smallest size class that fits `total` bytes, or -1 if none. */
static int fl_class_for(size_t total) {
    for (int c = 0; c < FL_NUM_CLASSES; c++) {
        if (total <= fl_class_sizes[c]) return c;
    }
    return -1;
}

/* Try to pop a block from the free list for the given size class. */
static GCHeader* fl_pop(int cls) {
    if (cls < 0 || !fl_heads[cls]) return NULL;
    GCHeader* hdr = fl_heads[cls];
    fl_heads[cls] = hdr->next;
    fl_counts[cls]--;
    return hdr;
}

/* Push a dead block onto the free list. Returns 1 if cached, 0 if full. */
static int fl_push(GCHeader* hdr) {
    int cls = fl_class_for(hdr->size);
    if (cls < 0 || fl_counts[cls] >= FL_MAX_PER_CLASS) return 0;
    hdr->next = fl_heads[cls];
    fl_heads[cls] = hdr;
    fl_counts[cls]++;
    return 1;
}

static void fl_free_all(void) {
    for (int c = 0; c < FL_NUM_CLASSES; c++) {
        GCHeader* h = fl_heads[c];
        while (h) {
            GCHeader* next = h->next;
            free(h);
            h = next;
        }
        fl_heads[c]  = NULL;
        fl_counts[c] = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Pointer hash table — O(1) average lookup for find_gc_header
 *
 * Open-addressing hash map: user_ptr → GCHeader*.
 * Power-of-2 table size with linear probing.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    void*     key;   /* user_ptr (pointer past header), NULL = empty slot */
    GCHeader* value; /* corresponding header */
} PtrMapEntry;

static PtrMapEntry* ptr_map = NULL;
static size_t ptr_map_cap   = 0;   /* always a power of 2 */
static size_t ptr_map_size  = 0;   /* number of occupied slots */

#define PTR_MAP_INIT_CAP 1024
#define PTR_MAP_LOAD_MAX_NUM 3   /* grow when size * 4 > cap * 3 (75%) */
#define PTR_MAP_LOAD_MAX_DEN 4

static size_t ptr_hash(void* p, size_t mask) {
    uintptr_t h = (uintptr_t)p;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return (size_t)(h & mask);
}

static void ptr_map_init(void) {
    ptr_map_cap  = PTR_MAP_INIT_CAP;
    ptr_map_size = 0;
    ptr_map = (PtrMapEntry*)calloc(ptr_map_cap, sizeof(PtrMapEntry));
}

static void ptr_map_grow(void) {
    size_t old_cap = ptr_map_cap;
    PtrMapEntry* old = ptr_map;
    ptr_map_cap *= 2;
    ptr_map = (PtrMapEntry*)calloc(ptr_map_cap, sizeof(PtrMapEntry));
    ptr_map_size = 0;
    size_t mask = ptr_map_cap - 1;
    for (size_t i = 0; i < old_cap; i++) {
        if (old[i].key) {
            size_t idx = ptr_hash(old[i].key, mask);
            while (ptr_map[idx].key) idx = (idx + 1) & mask;
            ptr_map[idx].key   = old[i].key;
            ptr_map[idx].value = old[i].value;
            ptr_map_size++;
        }
    }
    free(old);
}

static void ptr_map_insert(void* key, GCHeader* value) {
    if (!ptr_map) ptr_map_init();
    if (ptr_map_size * PTR_MAP_LOAD_MAX_DEN >= ptr_map_cap * PTR_MAP_LOAD_MAX_NUM)
        ptr_map_grow();
    size_t mask = ptr_map_cap - 1;
    size_t idx = ptr_hash(key, mask);
    while (ptr_map[idx].key && ptr_map[idx].key != key)
        idx = (idx + 1) & mask;
    if (!ptr_map[idx].key) ptr_map_size++;
    ptr_map[idx].key   = key;
    ptr_map[idx].value = value;
}

static void ptr_map_remove(void* key) {
    if (!ptr_map || !key) return;
    size_t mask = ptr_map_cap - 1;
    size_t idx = ptr_hash(key, mask);
    while (ptr_map[idx].key) {
        if (ptr_map[idx].key == key) {
            /* Delete and re-probe the cluster (backward-shift deletion). */
            ptr_map[idx].key   = NULL;
            ptr_map[idx].value = NULL;
            ptr_map_size--;
            size_t j = (idx + 1) & mask;
            while (ptr_map[j].key) {
                size_t k = ptr_hash(ptr_map[j].key, mask);
                /* Check if j is in the gap between idx and its natural slot k */
                if ((j > idx && (k <= idx || k > j)) ||
                    (j < idx && (k <= idx && k > j))) {
                    ptr_map[idx] = ptr_map[j];
                    ptr_map[j].key   = NULL;
                    ptr_map[j].value = NULL;
                    idx = j;
                }
                j = (j + 1) & mask;
            }
            return;
        }
        idx = (idx + 1) & mask;
    }
}

static GCHeader* ptr_map_lookup(void* key) {
    if (!ptr_map || !key) return NULL;
    size_t mask = ptr_map_cap - 1;
    size_t idx = ptr_hash(key, mask);
    while (ptr_map[idx].key) {
        if (ptr_map[idx].key == key) return ptr_map[idx].value;
        idx = (idx + 1) & mask;
    }
    return NULL;
}

static void ptr_map_free(void) {
    free(ptr_map);
    ptr_map = NULL;
    ptr_map_cap = 0;
    ptr_map_size = 0;
}

/* Rebuild the hash table from the surviving heap list.
   Much faster than per-object ptr_map_remove during sweep when most objects die. */
static void ptr_map_rebuild(void) {
    if (!ptr_map) return;
    memset(ptr_map, 0, ptr_map_cap * sizeof(PtrMapEntry));
    ptr_map_size = 0;
    size_t mask = ptr_map_cap - 1;
    for (GCHeader* h = heap_head; h; h = h->next) {
        void* user = (char*)h + sizeof(GCHeader);
        size_t idx = ptr_hash(user, mask);
        while (ptr_map[idx].key) idx = (idx + 1) & mask;
        ptr_map[idx].key   = user;
        ptr_map[idx].value = h;
        ptr_map_size++;
    }
    /* Shrink table if it became very sparse after collection. */
    while (ptr_map_cap > PTR_MAP_INIT_CAP &&
           ptr_map_size * 8 < ptr_map_cap) {
        /* Table is <12.5% full — halve it and rebuild. */
        size_t old_cap = ptr_map_cap;
        PtrMapEntry* old = ptr_map;
        ptr_map_cap /= 2;
        ptr_map = (PtrMapEntry*)calloc(ptr_map_cap, sizeof(PtrMapEntry));
        ptr_map_size = 0;
        mask = ptr_map_cap - 1;
        for (size_t i = 0; i < old_cap; i++) {
            if (old[i].key) {
                size_t idx = ptr_hash(old[i].key, mask);
                while (ptr_map[idx].key) idx = (idx + 1) & mask;
                ptr_map[idx].key   = old[i].key;
                ptr_map[idx].value = old[i].value;
                ptr_map_size++;
            }
        }
        free(old);
    }
}

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

/* Forward declaration for VisualDict defined in runtime.c. */
typedef struct {
    uint8_t _state;
} DictEntry_GC;

typedef struct {
    void*   entries;   /* DictEntry array (malloc'd) */
    int64_t capacity;
    int64_t length;
} VisualDict_GC;

/* Forward declaration for VisualGenerator defined in runtime.c. */
typedef struct {
    int32_t state;
    int32_t num_slots;
    int64_t value;
    void*   resume_fn;
    /* followed by int64_t slots[num_slots] inline */
} VisualGenerator_GC;

/* Each DictEntry is: char* key (8) + int64_t value (8) + uint8_t state (1) + padding = 24 bytes */
#define DICT_ENTRY_SIZE 24
#define DICT_ENTRY_STATE_USED 1

/* Return current monotonic time in nanoseconds (for pause measurement). */
static double now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart * 1e9 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
#endif
}

/* Check whether `ptr` looks like it points into one of our GC objects.
   O(1) average via hash table for exact user_ptr matches.
   Falls back to linear scan for interior pointers (conservative scanning). */
static GCHeader* find_gc_header(void* ptr) {
    if (!ptr) return NULL;
    /* Fast path: exact match on the user_ptr (allocation start). */
    GCHeader* h = ptr_map_lookup(ptr);
    if (h) return h;
    /* Range guard: skip O(n) scan if ptr is outside the heap bounding box. */
    uintptr_t addr = (uintptr_t)ptr;
    if (addr < heap_lo || addr >= heap_hi) return NULL;
    /* Slow path for interior pointers (conservative stack scan).
       Walk the heap list to find if ptr falls within any allocation. */
    h = heap_head;
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
    if (!gc_lock_initialized) {
#ifdef _WIN32
        InitializeCriticalSection(&gc_lock);
#else
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&gc_lock, &attr);
        pthread_mutexattr_destroy(&attr);
#endif
        gc_lock_initialized = 1;
    }
    GC_LOCK();
    stack_bottom = sb;
    heap_head = NULL;
    heap_bytes = 0;
    gc_threshold = 1 * 1024 * 1024;
    heap_lo = UINTPTR_MAX;
    heap_hi = 0;
    num_global_roots = 0;
    memset(&gc_stats, 0, sizeof(gc_stats));
    ptr_map_init();
    GC_UNLOCK();
}

void __visuall_gc_enable_stats(int enable) {
    GC_LOCK();
    gc_stats_enabled = enable;
    GC_UNLOCK();
}

GCStats __visuall_gc_get_stats(void) {
    GC_LOCK();
    GCStats stats = gc_stats;
    GC_UNLOCK();
    return stats;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Allocation
 * ═══════════════════════════════════════════════════════════════════════════ */

void* __visuall_alloc(size_t size, uint8_t type_tag) {
    GC_LOCK();
    /* Check if we should collect before allocating. */
    if (heap_bytes + sizeof(GCHeader) + size > gc_threshold) {
        __visuall_collect();
    }

    size_t total = sizeof(GCHeader) + size;

    /* Round up to size-class boundary so free-list blocks are interchangeable. */
    int fl_cls = fl_class_for(total);
    if (fl_cls >= 0) total = fl_class_sizes[fl_cls];

    /* Try the free-list pool first (avoids malloc for small, common sizes). */
    GCHeader* hdr = fl_pop(fl_cls);
    if (!hdr) {
        hdr = (GCHeader*)malloc(total);
        if (!hdr) {
            /* OOM — try collecting and retry once */
            __visuall_collect();
            hdr = (GCHeader*)malloc(total);
            if (!hdr) {
                fprintf(stderr, "out of memory (GC alloc %zu bytes)\n", size);
                GC_UNLOCK();
                exit(1);
            }
        }
    }

    hdr->type_tag   = type_tag;
    hdr->marked     = 0;
    hdr->flags      = 0;
    hdr->size       = (uint32_t)total;
    hdr->field_count = 0;

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

    /* Insert into the pointer hash table for O(1) lookup. */
    ptr_map_insert(user, hdr);

    /* Update heap address range for fast out-of-range rejection. */
    uintptr_t haddr = (uintptr_t)hdr;
    if (haddr < heap_lo) heap_lo = haddr;
    uintptr_t htop  = haddr + total;
    if (htop  > heap_hi) heap_hi = htop;

    GC_UNLOCK();
    return user;
}

void* __visuall_alloc_object(size_t payload, uint32_t field_count,
                              const uint32_t* field_offsets) {
    GC_LOCK();
    /* Total allocation: GCHeader + user payload + field_offsets array. */
    size_t offsets_bytes = field_count * sizeof(uint32_t);
    size_t total_payload = payload + offsets_bytes;

    /* Trigger GC before allocating if needed. */
    if (heap_bytes + sizeof(GCHeader) + total_payload > gc_threshold) {
        __visuall_collect();
    }

    size_t total = sizeof(GCHeader) + total_payload;
    GCHeader* hdr = (GCHeader*)malloc(total);
    if (!hdr) {
        __visuall_collect();
        hdr = (GCHeader*)malloc(total);
        if (!hdr) {
            fprintf(stderr, "out of memory (GC alloc_object %zu bytes)\n", total_payload);
            GC_UNLOCK();
            exit(1);
        }
    }

    hdr->type_tag    = VSL_TAG_OBJECT;
    hdr->marked      = 0;
    hdr->flags       = 0;
    hdr->size        = (uint32_t)total;
    hdr->field_count = field_count;

    hdr->next = heap_head;
    heap_head = hdr;

    heap_bytes += total;
    if (heap_bytes > gc_stats.peak_heap_bytes) {
        gc_stats.peak_heap_bytes = heap_bytes;
    }
    gc_stats.current_live_bytes = heap_bytes;

    void* user = (char*)hdr + sizeof(GCHeader);
    /* Zero user payload. */
    memset(user, 0, payload);

    /* Copy field offsets to tail of allocation. */
    if (field_count > 0 && field_offsets) {
        uint32_t* tail = (uint32_t*)((char*)user + payload);
        memcpy(tail, field_offsets, offsets_bytes);
    }

    ptr_map_insert(user, hdr);

    uintptr_t haddr = (uintptr_t)hdr;
    if (haddr < heap_lo) heap_lo = haddr;
    uintptr_t htop = haddr + total;
    if (htop > heap_hi) heap_hi = htop;

    GC_UNLOCK();
    return user;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Global root registration
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_register_global(void** ptr) {
    GC_LOCK();
    if (num_global_roots < MAX_GLOBAL_ROOTS) {
        global_roots[num_global_roots++] = ptr;
    } else {
        fprintf(stderr, "warning: GC global root table full\n");
    }
    GC_UNLOCK();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Mark phase
 * ═══════════════════════════════════════════════════════════════════════════ */

static void mark_internal(void* ptr) {
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
                mark_internal(elem);
            }
        }
        break;
    }

    case VSL_TAG_DICT: {
        /* Dict entries: trace keys (strings) and values (may be GC pointers). */
        VisualDict_GC* dict = (VisualDict_GC*)ptr;
        if (dict->entries) {
            char* base = (char*)dict->entries;
            for (int64_t i = 0; i < dict->capacity; i++) {
                char* ent = base + i * DICT_ENTRY_SIZE;
                uint8_t state = *(uint8_t*)(ent + 16); /* offset of state field */
                if (state == DICT_ENTRY_STATE_USED) {
                    void* key = *(void**)ent;           /* char* key at offset 0 */
                    if (key) {
                        GCHeader* kh = find_gc_header(key);
                        if (kh) mark_internal(key);
                    }
                    void* val = *(void**)(ent + 8);     /* int64_t value at offset 8, may be ptr */
                    GCHeader* vh = find_gc_header(val);
                    if (vh) mark_internal(val);
                }
            }
        }
        break;
    }

    case VSL_TAG_CLOSURE: {
        /* Closure fat pointer: { void* env, void* fn_ptr }
           env may point to a GC-allocated struct.  fn_ptr is code, skip. */
        void** fields = (void**)ptr;
        void* env = fields[0];
        if (env) {
            GCHeader* eh = find_gc_header(env);
            if (eh) {
                mark_internal(env);
            }
        }
        break;
    }

    case VSL_TAG_OBJECT: {
        /* Class instances allocated via __visuall_alloc_object store the
           number of GC-traced fields in hdr->field_count and the byte
           offsets of those fields at the tail of the allocation (after
           the user payload).  Objects allocated via the legacy
           __visuall_alloc path have field_count == 0 and are scanned
           conservatively for backwards compatibility. */
        if (hdr->field_count > 0) {
            /* Precise tracing: only follow the registered field offsets. */
            size_t offsets_bytes = hdr->field_count * sizeof(uint32_t);
            size_t payload = hdr->size - (uint32_t)sizeof(GCHeader) - (uint32_t)offsets_bytes;
            uint32_t* offsets = (uint32_t*)((char*)ptr + payload);
            for (uint32_t i = 0; i < hdr->field_count; i++) {
                int64_t raw = *(int64_t*)((char*)ptr + offsets[i]);
                void* fptr = (void*)(uintptr_t)(uint64_t)raw;
                if (fptr) {
                    GCHeader* eh = find_gc_header(fptr);
                    if (eh) mark_internal(fptr);
                }
            }
        } else {
            /* Conservative scan: treat every aligned word as a potential ptr. */
            size_t payload = hdr->size - (uint32_t)sizeof(GCHeader);
            size_t nwords = payload / sizeof(void*);
            void** words = (void**)ptr;
            for (size_t i = 0; i < nwords; i++) {
                GCHeader* eh = find_gc_header(words[i]);
                if (eh) {
                    mark_internal(words[i]);
                }
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
                mark_internal(elem);
            }
        }
        break;
    }

    case VSL_TAG_BOXED:
        /* Box: a single int64_t payload — no child GC pointers to trace. */
        break;

    case VSL_TAG_SET:
        /* VisualSet — no GC child pointers (data array is int64_t values). */
        break;

    case VSL_TAG_GENERATOR: {
        /* Scan saved-variable slots — each may hold a GC pointer as int64_t. */
        VisualGenerator_GC* gen = (VisualGenerator_GC*)ptr;
        int64_t* slots = (int64_t*)((char*)ptr + sizeof(VisualGenerator_GC));
        for (int32_t i = 0; i < gen->num_slots; i++) {
            void* candidate = (void*)(uintptr_t)slots[i];
            GCHeader* hdr = find_gc_header(candidate);
            if (hdr) mark_internal(candidate);
        }
        break;
    }

    case VSL_TAG_TASK: {
        // VisuallTask: mark the env (closure + saved variables).
        VisuallTask* task = (VisuallTask*)ptr;
        if (task->env) {
            GCHeader* eh = find_gc_header(task->env);
            if (eh) mark_internal(task->env);
        }
        break;
    }

    case VSL_TAG_CHANNEL: {
        // VisuallChannel: walk blocked-sender and blocked-receiver queues,
        // marking each suspended task's env (Trap 9).
        VisuallChannel* ch = (VisuallChannel*)ptr;
        VisuallTask* t = ch->blocked_senders_head;
        while (t) {
            if (t->env) {
                GCHeader* eh = find_gc_header(t->env);
                if (eh) mark_internal(t->env);
            }
            t = t->next;
        }
        t = ch->blocked_receivers_head;
        while (t) {
            if (t->env) {
                GCHeader* eh = find_gc_header(t->env);
                if (eh) mark_internal(t->env);
            }
            t = t->next;
        }
        break;
    }

    default:
        break;
    }
}

void __visuall_mark(void* ptr) {
    GC_LOCK();
    mark_internal(ptr);
    GC_UNLOCK();
}

/* ── Mark all registered global roots ──────────────────────────────────── */
static void mark_global_roots(void) {
    for (size_t i = 0; i < num_global_roots; i++) {
        void* val = *global_roots[i];
        if (val) {
            GCHeader* hdr = find_gc_header(val);
            if (hdr) {
                mark_internal(val);
            }
        }
    }
}

/* ── Read the hardware stack pointer reliably via inline asm.
 *    The C idiom `volatile void* sp; sp = &sp;` is UB under optimisation
 *    because the compiler may keep `sp` in a register, making &sp useless.
 *    Reading RSP/ESP directly is the only correct approach. */
static void* get_sp(void) {
#if defined(__GNUC__) || defined(__clang__)
#  if defined(__x86_64__)
    void* sp;
    __asm__ volatile ("movq %%rsp, %0" : "=r"(sp));
    return sp;
#  elif defined(__i386__)
    void* sp;
    __asm__ volatile ("movl %%esp, %0" : "=r"(sp));
    return sp;
#  elif defined(__aarch64__)
    void* sp;
    __asm__ volatile ("mov %0, sp" : "=r"(sp));
    return sp;
#  else
    /* Fallback for other GCC/Clang targets — less reliable but safe. */
    volatile uintptr_t sp_val = (uintptr_t)__builtin_frame_address(0);
    return (void*)sp_val;
#  endif
#elif defined(_MSC_VER)
    /* MSVC x64: _AddressOfReturnAddress() points just above the return
       address slot, giving a conservative (high-side) SP approximation. */
    return (void*)((uintptr_t)_AddressOfReturnAddress() - sizeof(void*));
#else
    /* Last-resort C fallback — may be imprecise under heavy optimisation. */
    volatile void* sp = NULL;
    sp = (void*)&sp;
    return (void*)sp;
#endif
}

/* ── Conservative stack scan ───────────────────────────────────────────── */
static void mark_stack(void) {
    if (!stack_bottom) return;

    void* sp = get_sp();

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
            mark_internal(candidate);
        }
    }
}

static void mark_roots(void) {
    mark_global_roots();
    mark_stack();

    // Walk the scheduler's ready queue — every queued goroutine's task
    // and its env must survive GC (Trap 5).
    VisuallTask* t = ready_head;
    while (t) {
        GCHeader* th = find_gc_header(t);
        if (th) mark_internal(t);
        t = t->next;
    }
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
    case VSL_TAG_DICT: {
        /* Free the dict entries array. */
        VisualDict_GC* dict = (VisualDict_GC*)user;
        if (dict->entries) {
            free(dict->entries);
            dict->entries = NULL;
        }
        break;
    }
    case VSL_TAG_SET: {
        /* VisualSet: { int64_t* data, int64_t length, int64_t capacity } */
        int64_t** pdata = (int64_t**)user;
        if (*pdata) {
            free(*pdata);
            *pdata = NULL;
        }
        break;
    }
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
            /* Object is garbage — unlink. */
            *pp = hdr->next;

            size_t obj_size = hdr->size;
            heap_bytes -= obj_size;
            gc_stats.total_bytes_collected += obj_size;

            finalize(hdr);

            /* Try to cache on free list; otherwise free to OS. */
            if (!fl_push(hdr)) {
                free(hdr);
            }
        }
    }
    gc_stats.current_live_bytes = heap_bytes;

    /* Rebuild hash table from survivors (O(survivors) instead of O(dead) removes). */
    ptr_map_rebuild();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Collection entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_collect(void) {
    GC_LOCK();
    double t0 = 0;
    if (gc_stats_enabled) t0 = now_ns();

    mark_roots();
    sweep();

    /* Adjust threshold: 2× current live bytes, but at least 1 MB. */
    size_t new_threshold = heap_bytes * 2;
    if (new_threshold < 1 * 1024 * 1024) new_threshold = 1 * 1024 * 1024;
    gc_threshold = new_threshold;

    if (gc_stats_enabled) {
        double t1 = now_ns();
        gc_stats.total_collections++;
        gc_stats.total_pause_ns += (t1 - t0);
    } else {
        gc_stats.total_collections++;
    }
    GC_UNLOCK();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Thread registration — worker threads call this so the GC scans their stack
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_gc_register_thread(void) {
    /* v1: single-threaded, no-op.  v2 will register the thread's stack
       base in a global registry for multi-threaded GC scanning. */
    (void)0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Shutdown — free everything, optionally print stats
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_gc_shutdown(void) {
    GC_LOCK();
    /* Frees all remaining objects. */
    GCHeader* h = heap_head;
    while (h) {
        GCHeader* next = h->next;
        finalize(h);
        free(h);
        h = next;
    }
    heap_head = NULL;
    heap_bytes = 0;

    ptr_map_free();

    fl_free_all();

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
    GC_UNLOCK();

    /* Delete the lock if initialized. */
    if (gc_lock_initialized) {
#ifdef _WIN32
        DeleteCriticalSection(&gc_lock);
#else
        pthread_mutex_destroy(&gc_lock);
#endif
        gc_lock_initialized = 0;
    }
}
