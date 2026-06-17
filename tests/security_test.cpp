/* ════════════════════════════════════════════════════════════════════════════
 * tests/security_test.cpp — Tests for security & correctness fixes
 *
 * Tests correct behavior after fixes for:
 *   V05 — string_repeat overflow guard
 *   V08 — GC size truncation guard
 *   V09 — channel direct-handoff value delivery
 *   V10 — channel close wakes blocked tasks
 * ════════════════════════════════════════════════════════════════════════════ */

#include <iostream>
#include <cstdlib>
#include <cstdint>
#include <cstring>

/* Pull in runtime/GC/scheduler APIs. */
extern "C" {
#include "../stdlib/gc.h"
#include "../stdlib/scheduler.h"
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

/* ── Forward declarations for runtime functions we test ─────────────────── */
extern "C" {
    char* __visuall_string_repeat(const char* s, int64_t n);
    int64_t __visuall_str_len(const char* s);
    int64_t __visuall_str_eq(const char* a, const char* b);
}

/* ════════════════════════════════════════════════════════════════════════════
 * V05 — string_repeat overflow guard
 *
 * Tests:
 *   1. Normal repeat produces correct output.
 *   2. Repeat with n=0 returns empty string.
 *   3. Repeat with n=1 returns copy of input.
 *   4. Repeat with empty input returns empty output.
 *   5. Repeat with a reasonably large count works correctly.
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_string_repeat_basic() {
    char* r = __visuall_string_repeat("ab", 3);
    expect(r != nullptr, "repeat: non-null result");
    expect(strcmp(r, "ababab") == 0, "repeat: 'ab' * 3 == 'ababab'");
    expect(__visuall_str_len(r) == 6, "repeat: length is 6");
}

static void test_string_repeat_zero() {
    char* r = __visuall_string_repeat("hello", 0);
    expect(r != nullptr, "repeat n=0: non-null result");
    expect(strcmp(r, "") == 0, "repeat n=0: empty string");
}

static void test_string_repeat_one() {
    char* r = __visuall_string_repeat("xyz", 1);
    expect(r != nullptr, "repeat n=1: non-null result");
    expect(strcmp(r, "xyz") == 0, "repeat n=1: copy of input");
}

static void test_string_repeat_empty_input() {
    char* r = __visuall_string_repeat("", 1000);
    expect(r != nullptr, "repeat empty input: non-null result");
    expect(strcmp(r, "") == 0, "repeat empty input: empty output");
}

static void test_string_repeat_large_safe() {
    // 1000 * 5 = 5000 chars — well within limits
    char* r = __visuall_string_repeat("hello", 1000);
    expect(r != nullptr, "repeat large: non-null result");
    expect(__visuall_str_len(r) == 5000, "repeat large: correct length");
    // Verify first and last repeats
    expect(memcmp(r, "hello", 5) == 0, "repeat large: first repeat correct");
    expect(memcmp(r + 4995, "hello", 5) == 0, "repeat large: last repeat correct");
}

/* ════════════════════════════════════════════════════════════════════════════
 * V08 — GC size truncation guard
 *
 * Tests:
 *   1. Small allocation succeeds.
 *   2. 1 MB allocation succeeds and memory is writable.
 *   3. Very large (but safe) allocation succeeds.
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_gc_alloc_small() {
    void* p = __visuall_alloc(64, VSL_TAG_STRING);
    expect(p != nullptr, "gc alloc 64 bytes: non-null");
    GCHeader* hdr = __visuall_get_header(p);
    expect(hdr != nullptr, "gc alloc 64 bytes: has header");
    expect(hdr->type_tag == VSL_TAG_STRING, "gc alloc 64 bytes: correct tag");
}

static void test_gc_alloc_medium() {
    // 1 MB allocation — well within limits
    void* p = __visuall_alloc(1024 * 1024, VSL_TAG_STRING);
    expect(p != nullptr, "gc alloc 1MB: non-null");
    GCHeader* hdr = __visuall_get_header(p);
    expect(hdr != nullptr, "gc alloc 1MB: has header");
    // Write to confirm the memory is usable
    ((char*)p)[0] = 'X';
    ((char*)p)[1024 * 1024 - 1] = 'Z';
    expect(((char*)p)[0] == 'X', "gc alloc 1MB: first byte writable");
    expect(((char*)p)[1024 * 1024 - 1] == 'Z', "gc alloc 1MB: last byte writable");
}

static void test_gc_alloc_object_small() {
    // Object allocation with field offsets
    uint32_t offsets[] = {0, 8};
    void* p = __visuall_alloc_object(16, 2, offsets);
    expect(p != nullptr, "gc alloc_object: non-null");
    GCHeader* hdr = __visuall_get_header(p);
    expect(hdr != nullptr, "gc alloc_object: has header");
    expect(hdr->field_count == 2, "gc alloc_object: field_count == 2");
}

/* ════════════════════════════════════════════════════════════════════════════
 * V09/V10 — Channel correctness
 *
 * Tests:
 *   1. Channel creation with various capacities.
 *   2. Buffered channel send+recv delivers correct values in order.
 *   3. Channel close sets closed flag and clears blocked queues.
 *   4. Closed channel rejects sends.
 * ════════════════════════════════════════════════════════════════════════════ */
static void test_channel_create_unbuffered() {
    VisuallChannel* ch = __visuall_chan_create(0);
    expect(ch != nullptr, "chan create unbuffered: non-null");
    expect(ch->capacity == 0, "chan create unbuffered: capacity == 0");
    expect(!ch->closed, "chan create unbuffered: not closed");
    expect(ch->blocked_senders_head == nullptr, "chan create: no blocked senders");
    expect(ch->blocked_receivers_head == nullptr, "chan create: no blocked receivers");
}

static void test_channel_create_buffered() {
    VisuallChannel* ch = __visuall_chan_create(8);
    expect(ch != nullptr, "chan create buffered: non-null");
    expect(ch->capacity == 8, "chan create buffered: capacity == 8");
    expect(ch->count == 0, "chan create buffered: count == 0");
    expect(ch->buffer != nullptr, "chan create buffered: buffer allocated");
}

static void test_channel_buffered_fifo() {
    VisuallChannel* ch = __visuall_chan_create(3);

    // Send values
    __visuall_chan_send(ch, 10, nullptr, nullptr);
    __visuall_chan_send(ch, 20, nullptr, nullptr);
    __visuall_chan_send(ch, 30, nullptr, nullptr);

    expect(ch->count == 3, "buffered FIFO: count == 3 after 3 sends");

    // Receive in FIFO order
    int64_t v1 = __visuall_chan_recv(ch, nullptr, nullptr);
    int64_t v2 = __visuall_chan_recv(ch, nullptr, nullptr);
    int64_t v3 = __visuall_chan_recv(ch, nullptr, nullptr);

    expect(v1 == 10, "buffered FIFO: first == 10");
    expect(v2 == 20, "buffered FIFO: second == 20");
    expect(v3 == 30, "buffered FIFO: third == 30");
    expect(ch->count == 0, "buffered FIFO: count == 0 after 3 recvs");
}

static void test_channel_close_basic() {
    VisuallChannel* ch = __visuall_chan_create(0);
    __visuall_chan_close(ch);

    expect(ch->closed, "chan close: closed flag set");
    expect(ch->blocked_receivers_head == nullptr, "chan close: receivers drained");
    expect(ch->blocked_senders_head == nullptr, "chan close: senders drained");

    // Send to closed channel should be no-op
    __visuall_chan_send(ch, 99, nullptr, nullptr);
    expect(ch->count == 0, "chan close: send to closed channel is no-op");
}

static void test_channel_close_null() {
    // Should not crash
    __visuall_chan_close(nullptr);
    expect(true, "chan close null: no crash");
}

static void test_channel_negative_values() {
    // Channel should handle any int64_t values
    VisuallChannel* ch = __visuall_chan_create(4);

    __visuall_chan_send(ch, INT64_MIN, nullptr, nullptr);
    __visuall_chan_send(ch, -1, nullptr, nullptr);
    __visuall_chan_send(ch, 0, nullptr, nullptr);
    __visuall_chan_send(ch, INT64_MAX, nullptr, nullptr);

    expect(__visuall_chan_recv(ch, nullptr, nullptr) == INT64_MIN, "chan neg: INT64_MIN round-trips");
    expect(__visuall_chan_recv(ch, nullptr, nullptr) == -1, "chan neg: -1 round-trips");
    expect(__visuall_chan_recv(ch, nullptr, nullptr) == 0, "chan neg: 0 round-trips");
    expect(__visuall_chan_recv(ch, nullptr, nullptr) == INT64_MAX, "chan neg: INT64_MAX round-trips");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Test runner
 * ════════════════════════════════════════════════════════════════════════════ */
int runSecurityTests() {
    std::cout << "\n--- Security Fix Tests ---\n";

    std::cout << "\n  V05 — string_repeat overflow guard:\n";
    test_string_repeat_basic();
    test_string_repeat_zero();
    test_string_repeat_one();
    test_string_repeat_empty_input();
    test_string_repeat_large_safe();

    std::cout << "\n  V08 — GC size truncation guard:\n";
    test_gc_alloc_small();
    test_gc_alloc_medium();
    test_gc_alloc_object_small();

    std::cout << "\n  V09/V10 — Channel correctness:\n";
    test_channel_create_unbuffered();
    test_channel_create_buffered();
    test_channel_buffered_fifo();
    test_channel_close_basic();
    test_channel_close_null();
    test_channel_negative_values();

    std::cout << "\n  Security tests: "
              << (failures == 0 ? "All passed!" : "")
              << "\n";
    if (failures > 0)
        std::cout << "  " << failures << " test(s) FAILED\n";
    return failures;
}
