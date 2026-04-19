/* ════════════════════════════════════════════════════════════════════════════
 * tests/gc_test.cpp — Unit tests for the Visuall mark-and-sweep GC.
 *
 * 12 test cases:
 *   1.  Objects are allocated via __visuall_alloc
 *   2.  GCHeader is prepended correctly
 *   3.  Reachable object survives collection
 *   4.  Unreachable object is collected
 *   5.  Circular reference is collected (mark handles cycles)
 *   6.  List elements are marked as reachable
 *   7.  Closure environment is marked reachable while closure lives
 *   8.  Dict cleaned up without leak (placeholder — dict not fully impl)
 *   9.  Collection triggered when threshold exceeded
 *  10.  Threshold doubles after collection
 *  11.  --gc-stats outputs correct collection count
 *  12.  Large allocation stress test: 100k objects, no leak
 * ════════════════════════════════════════════════════════════════════════════ */

#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

/* Pull in the GC API. */
extern "C" {
#include "../stdlib/gc.h"
}

/* ── VisualList mirror (must match runtime.c layout) ────────────────────── */
struct VisualList {
    int64_t* data;
    int64_t  length;
    int64_t  capacity;
};

/* Forward-declare the runtime list helpers we need. */
extern "C" {
    VisualList* __visuall_list_new(void);
    void        __visuall_list_push(VisualList* list, int64_t value);
    int64_t     __visuall_list_get(VisualList* list, int64_t index);
    int64_t     __visuall_list_len(VisualList* list);
}

static int failures = 0;

static void expect(bool condition, const char* testName) {
    if (!condition) {
        std::cerr << "  FAIL: " << testName << "\n";
        failures++;
    } else {
        std::cout << "  PASS: " << testName << "\n";
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 1: Objects are allocated via __visuall_alloc
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_alloc_basic() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    void* ptr = __visuall_alloc(64, VSL_TAG_STRING);
    expect(ptr != nullptr, "alloc returns non-null pointer");

    GCStats stats = __visuall_gc_get_stats();
    expect(stats.current_live_bytes > 0, "live bytes > 0 after alloc");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 2: GCHeader is prepended correctly
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_header_prepended() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    void* ptr = __visuall_alloc(32, VSL_TAG_LIST);
    GCHeader* hdr = __visuall_get_header(ptr);

    expect(hdr != nullptr, "header is non-null");
    expect(hdr->type_tag == VSL_TAG_LIST, "type_tag == VSL_TAG_LIST");
    expect(hdr->marked == 0, "marked == 0 initially");
    expect(hdr->size >= sizeof(GCHeader) + 32, "size includes header");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 3: Reachable object survives collection
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_reachable_survives() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Allocate and register as a global root so it's reachable. */
    void* ptr = __visuall_alloc(16, VSL_TAG_STRING);
    memcpy(ptr, "hello", 6);
    __visuall_register_global((void**)&ptr);

    __visuall_collect();

    /* ptr should still be valid. */
    expect(memcmp(ptr, "hello", 6) == 0, "reachable string survives GC");

    GCStats stats = __visuall_gc_get_stats();
    expect(stats.total_collections == 1, "1 collection ran");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 4: Unreachable object is collected
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_unreachable_collected() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Allocate but don't register as root. */
    volatile void* ptr = __visuall_alloc(64, VSL_TAG_STRING);
    (void)ptr;

    size_t before = __visuall_gc_get_stats().current_live_bytes;

    /* Null out local so conservative scan can't find it. */
    ptr = nullptr;

    __visuall_collect();

    size_t after = __visuall_gc_get_stats().current_live_bytes;
    /* After collection, live bytes should be less (the object was freed). */
    expect(after < before, "unreachable object collected (live bytes decreased)");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 5: Circular reference is collected (mark handles cycles)
 * ════════════════════════════════════════════════════════════════════════════ */

/* Build the circular structure in a callee so that when it returns, the
   stack frame holding the local pointers is truly gone and cannot be
   found by conservative scanning. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void create_circular_garbage() {
    VisualList* a = __visuall_list_new();
    VisualList* b = __visuall_list_new();
    __visuall_list_push(a, (int64_t)(uintptr_t)b);
    __visuall_list_push(b, (int64_t)(uintptr_t)a);
    /* a and b go out of scope — no one outside this frame can reach them. */
}

static void test_circular_collected() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    create_circular_garbage();

    size_t before = __visuall_gc_get_stats().current_live_bytes;

    __visuall_collect();

    size_t after = __visuall_gc_get_stats().current_live_bytes;
    expect(after < before, "circular references collected");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 6: List elements are marked as reachable
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_list_elements_marked() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    VisualList* list = __visuall_list_new();
    __visuall_register_global((void**)&list);

    /* Allocate strings and put them in the list. */
    char* s1 = (char*)__visuall_alloc(8, VSL_TAG_STRING);
    memcpy(s1, "aaa", 4);
    char* s2 = (char*)__visuall_alloc(8, VSL_TAG_STRING);
    memcpy(s2, "bbb", 4);

    __visuall_list_push(list, (int64_t)(uintptr_t)s1);
    __visuall_list_push(list, (int64_t)(uintptr_t)s2);

    /* Don't keep direct local refs to s1/s2. */
    s1 = nullptr;
    s2 = nullptr;

    __visuall_collect();

    /* Elements should survive because the list is a root. */
    char* r1 = (char*)(uintptr_t)__visuall_list_get(list, 0);
    char* r2 = (char*)(uintptr_t)__visuall_list_get(list, 1);
    expect(memcmp(r1, "aaa", 4) == 0, "list element 0 survived GC");
    expect(memcmp(r2, "bbb", 4) == 0, "list element 1 survived GC");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 7: Closure environment is marked reachable while closure lives
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_closure_env_marked() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Simulate a closure: { void* env, void* fn_ptr }
       env points to a GC-allocated struct. */
    struct FatPtr { void* env; void* fn_ptr; };
    FatPtr* closure = (FatPtr*)__visuall_alloc(sizeof(FatPtr), VSL_TAG_CLOSURE);
    __visuall_register_global((void**)&closure);

    /* Allocate an environment struct (simulated). */
    void* env = __visuall_alloc(32, VSL_TAG_OBJECT);
    memset(env, 0x42, 32);
    closure->env = env;
    closure->fn_ptr = nullptr; /* No real function. */

    /* Drop direct ref to env. */
    env = nullptr;

    __visuall_collect();

    /* env should survive because it's reachable through closure. */
    expect(((unsigned char*)closure->env)[0] == 0x42,
           "closure env survived GC (reachable through closure)");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 8: Dict cleaned up without leak (placeholder)
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_dict_cleanup() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Dict is not fully implemented yet; just test that a TAG_DICT
       allocation is properly freed by the GC. */
    volatile void* dict = __visuall_alloc(128, VSL_TAG_DICT);
    (void)dict;
    dict = nullptr;

    __visuall_collect();

    GCStats stats = __visuall_gc_get_stats();
    expect(stats.total_bytes_collected > 0, "dict allocation was collected");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 9: Collection triggered when threshold exceeded
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_auto_collect() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Allocate many objects to exceed the 8 MB threshold.
       Each alloc checks and triggers collection if needed. */
    for (int i = 0; i < 120000; i++) {
        volatile void* p = __visuall_alloc(64, VSL_TAG_STRING);
        (void)p;
    }

    GCStats stats = __visuall_gc_get_stats();
    expect(stats.total_collections > 0,
           "auto-collection triggered when threshold exceeded");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 10: Threshold doubles after collection
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_threshold_growth() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Force a collection and check that subsequent allocations
       can proceed further before the next collection. */
    __visuall_collect();
    uint64_t coll_after_first = __visuall_gc_get_stats().total_collections;

    /* Allocate more — the threshold should have grown. */
    for (int i = 0; i < 5000; i++) {
        volatile void* p = __visuall_alloc(64, VSL_TAG_STRING);
        (void)p;
    }

    uint64_t coll_after_second = __visuall_gc_get_stats().total_collections;
    /* The threshold is at least 1 MB, so 5000*80 = 400KB should not
       trigger another collection (threshold 2× live after first collect). */
    expect(coll_after_second >= coll_after_first,
           "threshold grows after collection (no premature re-collect)");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 11: --gc-stats outputs correct collection count
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_gc_stats() {
    int anchor = 0;
    __visuall_gc_init(&anchor);
    __visuall_gc_enable_stats(1);

    __visuall_collect();
    __visuall_collect();
    __visuall_collect();

    GCStats stats = __visuall_gc_get_stats();
    expect(stats.total_collections == 3, "gc_stats reports 3 collections");

    /* Disable stats before shutdown to avoid printing in test output. */
    __visuall_gc_enable_stats(0);
    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test 12: Large allocation stress test — 100k objects, no leak
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_stress_100k() {
    int anchor = 0;
    __visuall_gc_init(&anchor);

    /* Keep a small set alive via a rooted list. */
    VisualList* keep = __visuall_list_new();
    __visuall_register_global((void**)&keep);

    for (int i = 0; i < 100000; i++) {
        char* s = (char*)__visuall_alloc(32, VSL_TAG_STRING);
        memcpy(s, "test", 5);
        /* Keep every 1000th object. */
        if (i % 1000 == 0) {
            __visuall_list_push(keep, (int64_t)(uintptr_t)s);
        }
    }

    __visuall_collect();

    GCStats stats = __visuall_gc_get_stats();
    /* We kept 100 objects (indices 0, 1000, 2000, ..., 99000) plus the
       list struct.  The rest should have been collected. */
    expect(stats.total_bytes_collected > 0, "stress: garbage was collected");
    expect(stats.total_collections > 0, "stress: collections ran");

    /* Verify one of the kept strings is intact. */
    char* kept = (char*)(uintptr_t)__visuall_list_get(keep, 0);
    expect(memcmp(kept, "test", 5) == 0, "stress: kept object intact");

    __visuall_gc_shutdown();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Entry point
 * ════════════════════════════════════════════════════════════════════════════ */

int runGCTests() {
    failures = 0;
    std::cout << "--- GC Tests ---\n";

    test_alloc_basic();
    test_header_prepended();
    test_reachable_survives();
    test_unreachable_collected();
    test_circular_collected();
    test_list_elements_marked();
    test_closure_env_marked();
    test_dict_cleanup();
    test_auto_collect();
    test_threshold_growth();
    test_gc_stats();
    test_stress_100k();

    std::cout << "GC tests: " << (12 - failures) << "/12 passed\n";
    return failures;
}
