/* ════════════════════════════════════════════════════════════════════════════
 * Visuall Standard Library — C Runtime
 *
 * Low-level implementations of all builtin functions and module backends.
 * Compiled as a static library and linked into every Visuall binary.
 * ════════════════════════════════════════════════════════════════════════════ */

#include "gc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEP '\\'
#else
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#define PATH_SEP '/'
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

/* GC-aware string duplication — allocates via __visuall_alloc so the
   resulting string is tracked by the garbage collector. */
static char* vsl_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* out = (char*)__visuall_alloc(len + 1, VSL_TAG_STRING);
    memcpy(out, s, len + 1);
    return out;
}

/* Plain malloc wrapper for internal (non-GC) allocations such as
   the data array inside a VisualList (the list *struct* itself is
   GC-managed, but its resizable data buffer is ordinary malloc). */
static void* vsl_malloc(size_t n) {
    void* p = malloc(n);
    if (!p && n > 0) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall List runtime  (opaque struct — array-backed)
 *
 * Each element is stored as an int64_t. Pointers (strings, nested lists)
 * are cast to int64_t for storage and cast back on retrieval.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int64_t* data;
    int64_t  length;
    int64_t  capacity;
} VisualList;

VisualList* __visuall_list_new(void) {
    VisualList* list = (VisualList*)__visuall_alloc(sizeof(VisualList),
                                                     VSL_TAG_LIST);
    list->capacity = 8;
    list->length   = 0;
    list->data     = (int64_t*)vsl_malloc(sizeof(int64_t) * (size_t)list->capacity);
    return list;
}

void __visuall_list_push(VisualList* list, int64_t value) {
    if (!list) return;
    if (list->length >= list->capacity) {
        list->capacity *= 2;
        list->data = (int64_t*)realloc(list->data, sizeof(int64_t) * (size_t)list->capacity);
        if (!list->data) { fprintf(stderr, "out of memory\n"); exit(1); }
    }
    list->data[list->length++] = value;
}

int64_t __visuall_list_get(VisualList* list, int64_t index) {
    if (!list || index < 0 || index >= list->length) {
        fprintf(stderr, "IndexError: list index %lld out of range [0, %lld)\n",
                (long long)index, (long long)(list ? list->length : 0));
        exit(1);
    }
    return list->data[index];
}

void __visuall_list_set(VisualList* list, int64_t index, int64_t value) {
    if (!list || index < 0 || index >= list->length) {
        fprintf(stderr, "IndexError: list index %lld out of range [0, %lld)\n",
                (long long)index, (long long)(list ? list->length : 0));
        exit(1);
    }
    list->data[index] = value;
}

int64_t __visuall_list_len(VisualList* list) {
    return list ? list->length : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall Tuple runtime  (fixed-size VisualList with VSL_TAG_TUPLE)
 * ═══════════════════════════════════════════════════════════════════════════ */

VisualList* __visuall_tuple_new(int64_t count) {
    VisualList* t = (VisualList*)__visuall_alloc(sizeof(VisualList), VSL_TAG_TUPLE);
    t->capacity = count > 0 ? count : 1;
    t->length   = 0;
    t->data     = (int64_t*)vsl_malloc(sizeof(int64_t) * (size_t)t->capacity);
    return t;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall List Slice
 * ═══════════════════════════════════════════════════════════════════════════ */

VisualList* __visuall_list_slice(VisualList* list, int64_t start,
                                  int64_t stop, int64_t step) {
    if (!list) return __visuall_list_new();
    int64_t len = list->length;

    /* Clamp start/stop to [0, len] */
    if (start < 0) { start += len; if (start < 0) start = 0; }
    if (start > len) start = len;
    if (stop < 0) { stop += len; if (stop < 0) stop = 0; }
    if (stop > len) stop = len;

    if (step == 0) step = 1; /* protect against infinite loop */

    VisualList* out = __visuall_list_new();
    if (step > 0) {
        for (int64_t i = start; i < stop; i += step)
            __visuall_list_push(out, list->data[i]);
    } else {
        for (int64_t i = start; i > stop; i += step)
            __visuall_list_push(out, list->data[i]);
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall Dict runtime  (open-addressing hash table, string keys, i64 values)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define DICT_INIT_CAP  16
#define DICT_LOAD_MAX  0.7
#define DICT_EMPTY     0
#define DICT_USED      1
#define DICT_DELETED   2

typedef struct {
    char*   key;      /* GC-managed string (or NULL) */
    int64_t value;
    uint8_t state;    /* DICT_EMPTY / DICT_USED / DICT_DELETED */
} DictEntry;

typedef struct {
    DictEntry* entries;
    int64_t    capacity;
    int64_t    length;    /* number of USED entries */
} VisualDict;

static uint64_t dict_hash(const char* key) {
    uint64_t h = 14695981039346656037ULL;
    for (const char* p = key; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 1099511628211ULL;
    }
    return h;
}

static void dict_resize(VisualDict* d, int64_t new_cap);

static int64_t dict_find_slot(DictEntry* entries, int64_t cap, const char* key) {
    uint64_t h = dict_hash(key);
    int64_t idx = (int64_t)(h % (uint64_t)cap);
    int64_t first_deleted = -1;
    for (int64_t i = 0; i < cap; i++) {
        int64_t slot = (idx + i) % cap;
        if (entries[slot].state == DICT_EMPTY) {
            return first_deleted >= 0 ? first_deleted : slot;
        }
        if (entries[slot].state == DICT_DELETED) {
            if (first_deleted < 0) first_deleted = slot;
            continue;
        }
        if (entries[slot].key && strcmp(entries[slot].key, key) == 0) {
            return slot;
        }
    }
    return first_deleted >= 0 ? first_deleted : -1;
}

VisualDict* __visuall_dict_new(void) {
    VisualDict* d = (VisualDict*)__visuall_alloc(sizeof(VisualDict), VSL_TAG_DICT);
    d->capacity = DICT_INIT_CAP;
    d->length   = 0;
    d->entries  = (DictEntry*)vsl_malloc(sizeof(DictEntry) * (size_t)d->capacity);
    memset(d->entries, 0, sizeof(DictEntry) * (size_t)d->capacity);
    return d;
}

static void dict_resize(VisualDict* d, int64_t new_cap) {
    DictEntry* old = d->entries;
    int64_t old_cap = d->capacity;
    d->entries  = (DictEntry*)vsl_malloc(sizeof(DictEntry) * (size_t)new_cap);
    d->capacity = new_cap;
    d->length   = 0;
    memset(d->entries, 0, sizeof(DictEntry) * (size_t)new_cap);
    for (int64_t i = 0; i < old_cap; i++) {
        if (old[i].state == DICT_USED) {
            int64_t slot = dict_find_slot(d->entries, d->capacity, old[i].key);
            d->entries[slot].key   = old[i].key;
            d->entries[slot].value = old[i].value;
            d->entries[slot].state = DICT_USED;
            d->length++;
        }
    }
    free(old);
}

void __visuall_dict_set(VisualDict* d, const char* key, int64_t value) {
    if (!d || !key) return;
    if ((double)(d->length + 1) > (double)d->capacity * DICT_LOAD_MAX) {
        dict_resize(d, d->capacity * 2);
    }
    int64_t slot = dict_find_slot(d->entries, d->capacity, key);
    if (slot < 0) { dict_resize(d, d->capacity * 2); slot = dict_find_slot(d->entries, d->capacity, key); }
    if (d->entries[slot].state == DICT_USED) {
        d->entries[slot].value = value;
    } else {
        d->entries[slot].key   = vsl_strdup(key);
        d->entries[slot].value = value;
        d->entries[slot].state = DICT_USED;
        d->length++;
    }
}

int64_t __visuall_dict_get(VisualDict* d, const char* key) {
    if (!d || !key) return 0;
    int64_t slot = dict_find_slot(d->entries, d->capacity, key);
    if (slot >= 0 && d->entries[slot].state == DICT_USED) {
        return d->entries[slot].value;
    }
    fprintf(stderr, "KeyError: '%s'\n", key);
    exit(1);
}

int64_t __visuall_dict_has(VisualDict* d, const char* key) {
    if (!d || !key) return 0;
    int64_t slot = dict_find_slot(d->entries, d->capacity, key);
    return (slot >= 0 && d->entries[slot].state == DICT_USED) ? 1 : 0;
}

int64_t __visuall_dict_len(VisualDict* d) {
    return d ? d->length : 0;
}

int64_t __visuall_dict_contains(VisualDict* d, const char* key) {
    return __visuall_dict_has(d, key);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Tag introspection & membership tests
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t __visuall_get_tag(void* ptr) {
    if (!ptr) return 0;
    GCHeader* hdr = (GCHeader*)ptr - 1;
    return (int64_t)hdr->type_tag;
}

int64_t __visuall_list_contains(VisualList* list, int64_t value) {
    if (!list) return 0;
    for (int64_t i = 0; i < list->length; i++) {
        if (list->data[i] == value) return 1;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion builtins: __visuall_to_str
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Convert an i64 to a heap-allocated decimal string */
char* __visuall_int_to_str(int64_t val) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)val);
    return vsl_strdup(buf);
}

/* Convert a double to a heap-allocated string */
char* __visuall_float_to_str(double val) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", val);
    return vsl_strdup(buf);
}

/* Convert a bool (i1 stored as i64 0/1) to "true"/"false" */
char* __visuall_bool_to_str(int64_t val) {
    return vsl_strdup(val ? "true" : "false");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String concatenation
 * ═══════════════════════════════════════════════════════════════════════════ */

char* __visuall_str_concat(const char* a, const char* b) {
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char* out = (char*)__visuall_alloc(la + lb + 1, VSL_TAG_STRING);
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * String comparison (value-based)
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t __visuall_str_eq(const char* a, const char* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    return strcmp(a, b) == 0 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Fast integer-to-buffer (no snprintf overhead)
 * ═══════════════════════════════════════════════════════════════════════════ */

static int fast_i64_to_buf(int64_t val, char* buf) {
    char tmp[21]; /* max 20 digits + sign */
    int pos = 0;
    int neg = 0;
    uint64_t uv;

    if (val < 0) {
        neg = 1;
        uv = (uint64_t)(-(val + 1)) + 1;
    } else {
        uv = (uint64_t)val;
    }

    do {
        tmp[pos++] = '0' + (char)(uv % 10);
        uv /= 10;
    } while (uv);

    int len = 0;
    if (neg) buf[len++] = '-';
    for (int i = pos - 1; i >= 0; i--)
        buf[len++] = tmp[i];
    buf[len] = '\0';
    return len;
}

int __visuall_int_to_buf(int64_t val, char* buf, int bufsize) {
    (void)bufsize;
    return fast_i64_to_buf(val, buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * f-string builder — single allocation for all parts
 *
 * Supports tagged integers: any entry in `parts` whose low bit is set
 * is actually an unboxed int64_t encoded as (val << 1) | 1.  These are
 * formatted inline via __visuall_int_to_buf — zero extra GC allocs.
 * ═══════════════════════════════════════════════════════════════════════════ */

char* __visuall_fstring_build(const char** parts, int count) {
    /* Pre-format tagged integers into stack buffers so we only format once. */
    char  ibufs[16][24];   /* up to 16 tagged-int parts, 24 chars each */
    const char* resolved[32];
    size_t      lengths[32];
    int ibuf_idx = 0;
    size_t total = 0;

    for (int i = 0; i < count && i < 32; i++) {
        uintptr_t raw = (uintptr_t)parts[i];
        if (raw & 1) {
            int64_t val = (int64_t)((intptr_t)raw >> 1);
            int n = fast_i64_to_buf(val, ibufs[ibuf_idx]);
            resolved[i] = ibufs[ibuf_idx++];
            lengths[i]  = (size_t)n;
        } else if (parts[i]) {
            resolved[i] = parts[i];
            lengths[i]  = strlen(parts[i]);
        } else {
            resolved[i] = NULL;
            lengths[i]  = 0;
        }
        total += lengths[i];
    }

    char* out = (char*)__visuall_alloc(total + 1, VSL_TAG_STRING);
    size_t pos = 0;
    for (int i = 0; i < count && i < 32; i++) {
        if (resolved[i]) {
            memcpy(out + pos, resolved[i], lengths[i]);
            pos += lengths[i];
        }
    }
    out[pos] = '\0';
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: print / println
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_print_str(const char* s) {
    if (s) fputs(s, stdout);
}

void __visuall_print_int(int64_t v) {
    printf("%lld", (long long)v);
}

void __visuall_print_float(double v) {
    printf("%g", v);
}

void __visuall_print_bool(int64_t v) {
    fputs(v ? "true" : "false", stdout);
}

void __visuall_print_newline(void) {
    putchar('\n');
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: input(prompt)
 * ═══════════════════════════════════════════════════════════════════════════ */

char* __visuall_input(const char* prompt) {
    if (prompt) fputs(prompt, stdout);
    fflush(stdout);
    char buf[4096];
    if (!fgets(buf, (int)sizeof(buf), stdin)) {
        return vsl_strdup("");
    }
    /* Strip trailing newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';
    if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
    return vsl_strdup(buf);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: len(x)   — dispatched by tag in codegen
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t __visuall_str_len(const char* s) {
    return s ? (int64_t)strlen(s) : 0;
}

/* list length is __visuall_list_len above */

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: range(start, stop, step) -> list[int]
 * ═══════════════════════════════════════════════════════════════════════════ */

VisualList* __visuall_range(int64_t start, int64_t stop, int64_t step) {
    VisualList* list = __visuall_list_new();
    if (step == 0) return list;
    if (step > 0) {
        for (int64_t i = start; i < stop; i += step)
            __visuall_list_push(list, i);
    } else {
        for (int64_t i = start; i > stop; i += step)
            __visuall_list_push(list, i);
    }
    return list;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: type(x) -> str
 * The codegen inserts a type-tag integer argument to dispatch.
 * Tags: 0=int, 1=float, 2=str, 3=bool, 4=list, 5=dict, 6=null
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* __visuall_type_name(int64_t tag) {
    switch (tag) {
        case 0: return "int";
        case 1: return "float";
        case 2: return "str";
        case 3: return "bool";
        case 4: return "list";
        case 5: return "dict";
        case 6: return "null";
        case 7: return "tuple";
        default: return "unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversions: int(), float(), str(), bool()
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t __visuall_str_to_int(const char* s) {
    if (!s) return 0;
    char* end;
    errno = 0;
    long long val = strtoll(s, &end, 10);
    if (errno || end == s) {
        fprintf(stderr, "ValueError: invalid literal for int(): '%s'\n", s);
        exit(1);
    }
    return (int64_t)val;
}

double __visuall_str_to_float(const char* s) {
    if (!s) return 0.0;
    char* end;
    errno = 0;
    double val = strtod(s, &end);
    if (errno || end == s) {
        fprintf(stderr, "ValueError: invalid literal for float(): '%s'\n", s);
        exit(1);
    }
    return val;
}

int64_t __visuall_float_to_int(double v) {
    return (int64_t)v;
}

double __visuall_int_to_float(int64_t v) {
    return (double)v;
}

int64_t __visuall_bool_to_int(int64_t v) {
    return v ? 1 : 0;
}

const char* __visuall_int_to_str_const(int64_t v) {
    return __visuall_int_to_str(v);
}

const char* __visuall_float_to_str_const(double v) {
    return __visuall_float_to_str(v);
}

int64_t __visuall_str_to_bool(const char* s) {
    if (!s || *s == '\0') return 0;
    return 1;
}

int64_t __visuall_int_to_bool(int64_t v) {
    return v != 0 ? 1 : 0;
}

int64_t __visuall_float_to_bool(double v) {
    return v != 0.0 ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: abs, min, max, round
 * ═══════════════════════════════════════════════════════════════════════════ */

int64_t __visuall_abs_int(int64_t v) {
    return v < 0 ? -v : v;
}

double __visuall_abs_float(double v) {
    return fabs(v);
}

int64_t __visuall_min_int(int64_t a, int64_t b) {
    return a < b ? a : b;
}

int64_t __visuall_max_int(int64_t a, int64_t b) {
    return a > b ? a : b;
}

double __visuall_min_float(double a, double b) {
    return a < b ? a : b;
}

double __visuall_max_float(double a, double b) {
    return a > b ? a : b;
}

int64_t __visuall_round(double v) {
    return (int64_t)round(v);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: sorted, reversed
 * ═══════════════════════════════════════════════════════════════════════════ */

static int cmp_int64_asc(const void* a, const void* b) {
    int64_t va = *(const int64_t*)a;
    int64_t vb = *(const int64_t*)b;
    return (va > vb) - (va < vb);
}

VisualList* __visuall_sorted(VisualList* src) {
    VisualList* out = __visuall_list_new();
    if (!src) return out;
    for (int64_t i = 0; i < src->length; i++)
        __visuall_list_push(out, src->data[i]);
    qsort(out->data, (size_t)out->length, sizeof(int64_t), cmp_int64_asc);
    return out;
}

VisualList* __visuall_reversed(VisualList* src) {
    VisualList* out = __visuall_list_new();
    if (!src) return out;
    for (int64_t i = src->length - 1; i >= 0; i--)
        __visuall_list_push(out, src->data[i]);
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: enumerate(list) -> list[tuple[int, T]]
 * Stored as a flat list: [0, elem0, 1, elem1, ...]
 * Tuples are pairs of (index, element) stored as two consecutive i64s.
 * ═══════════════════════════════════════════════════════════════════════════ */

VisualList* __visuall_enumerate(VisualList* src) {
    VisualList* out = __visuall_list_new();
    if (!src) return out;
    for (int64_t i = 0; i < src->length; i++) {
        __visuall_list_push(out, i);
        __visuall_list_push(out, src->data[i]);
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: zip(list_a, list_b) -> list[tuple[A, B]]
 * Stored as flat pairs: [a0, b0, a1, b1, ...]
 * ═══════════════════════════════════════════════════════════════════════════ */

VisualList* __visuall_zip(VisualList* a, VisualList* b) {
    VisualList* out = __visuall_list_new();
    if (!a || !b) return out;
    int64_t min_len = a->length < b->length ? a->length : b->length;
    for (int64_t i = 0; i < min_len; i++) {
        __visuall_list_push(out, a->data[i]);
        __visuall_list_push(out, b->data[i]);
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: map(fn, list) -> list
 * fn is a closure fat pointer: { i8* env, i8* fn_ptr }.
 * The function pointer signature is: int64_t (*)(i8* env, int64_t elem)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef int64_t (*vsl_map_fn)(void* env, int64_t elem);

VisualList* __visuall_map(void* env, void* fn_ptr, VisualList* src) {
    VisualList* out = __visuall_list_new();
    if (!src || !fn_ptr) return out;
    vsl_map_fn fn = (vsl_map_fn)fn_ptr;
    for (int64_t i = 0; i < src->length; i++) {
        __visuall_list_push(out, fn(env, src->data[i]));
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Builtin: filter(fn, list) -> list
 * fn is a closure fat pointer.  Signature: int64_t (*)(i8* env, int64_t elem)
 * Returns nonzero => keep.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef int64_t (*vsl_filter_fn)(void* env, int64_t elem);

VisualList* __visuall_filter(void* env, void* fn_ptr, VisualList* src) {
    VisualList* out = __visuall_list_new();
    if (!src || !fn_ptr) return out;
    vsl_filter_fn fn = (vsl_filter_fn)fn_ptr;
    for (int64_t i = 0; i < src->length; i++) {
        if (fn(env, src->data[i]))
            __visuall_list_push(out, src->data[i]);
    }
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: string
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Return a single-character GC-managed string for s[i].
   Negative indices wrap: s[-1] is the last character.
   Out-of-range indices print an error and call exit(1). */
char* __visuall_string_index(const char* s, int64_t i) {
    if (!s) {
        fprintf(stderr, "IndexError: string index on null string\n");
        exit(1);
    }
    int64_t len = (int64_t)strlen(s);
    if (i < 0) i = len + i;   /* normalise negative index */
    if (i < 0 || i >= len) {
        fprintf(stderr, "IndexError: string index %lld out of range [0, %lld)\n",
                (long long)(i < 0 ? i - len : i), (long long)len);
        exit(1);
    }
    char* out = (char*)__visuall_alloc(2, VSL_TAG_STRING);
    out[0] = s[i];
    out[1] = '\0';
    return out;
}

char* __visuall_string_upper(const char* s) {
    if (!s) return vsl_strdup("");
    size_t len = strlen(s);
    char* out = (char*)__visuall_alloc(len + 1, VSL_TAG_STRING);
    for (size_t i = 0; i < len; i++)
        out[i] = (char)toupper((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

char* __visuall_string_lower(const char* s) {
    if (!s) return vsl_strdup("");
    size_t len = strlen(s);
    char* out = (char*)__visuall_alloc(len + 1, VSL_TAG_STRING);
    for (size_t i = 0; i < len; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

char* __visuall_string_strip(const char* s) {
    if (!s) return vsl_strdup("");
    size_t len = strlen(s);
    size_t start = 0, end = len;
    while (start < len && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    size_t new_len = end - start;
    char* out = (char*)__visuall_alloc(new_len + 1, VSL_TAG_STRING);
    memcpy(out, s + start, new_len);
    out[new_len] = '\0';
    return out;
}

char* __visuall_string_lstrip(const char* s) {
    if (!s) return vsl_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    return vsl_strdup(s);
}

char* __visuall_string_rstrip(const char* s) {
    if (!s) return vsl_strdup("");
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    char* out = (char*)__visuall_alloc(len + 1, VSL_TAG_STRING);
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

VisualList* __visuall_string_split(const char* s, const char* sep) {
    VisualList* out = __visuall_list_new();
    if (!s) return out;
    if (!sep || *sep == '\0') sep = " ";
    size_t sep_len = strlen(sep);
    const char* p = s;
    while (1) {
        const char* found = strstr(p, sep);
        if (!found) {
            __visuall_list_push(out, (int64_t)vsl_strdup(p));
            break;
        }
        size_t part_len = (size_t)(found - p);
        char* part = (char*)__visuall_alloc(part_len + 1, VSL_TAG_STRING);
        memcpy(part, p, part_len);
        part[part_len] = '\0';
        __visuall_list_push(out, (int64_t)part);
        p = found + sep_len;
    }
    return out;
}

char* __visuall_string_join(const char* sep, VisualList* parts) {
    if (!parts || parts->length == 0) return vsl_strdup("");
    if (!sep) sep = "";
    size_t sep_len = strlen(sep);

    /* Calculate total length */
    size_t total = 0;
    for (int64_t i = 0; i < parts->length; i++) {
        const char* part = (const char*)parts->data[i];
        if (part) total += strlen(part);
        if (i < parts->length - 1) total += sep_len;
    }

    char* out = (char*)__visuall_alloc(total + 1, VSL_TAG_STRING);
    size_t pos = 0;
    for (int64_t i = 0; i < parts->length; i++) {
        const char* part = (const char*)parts->data[i];
        if (part) {
            size_t plen = strlen(part);
            memcpy(out + pos, part, plen);
            pos += plen;
        }
        if (i < parts->length - 1 && sep_len > 0) {
            memcpy(out + pos, sep, sep_len);
            pos += sep_len;
        }
    }
    out[pos] = '\0';
    return out;
}

char* __visuall_string_replace(const char* s, const char* old_str, const char* new_str) {
    if (!s) return vsl_strdup("");
    if (!old_str || *old_str == '\0') return vsl_strdup(s);
    if (!new_str) new_str = "";

    size_t s_len = strlen(s);
    size_t old_len = strlen(old_str);
    size_t new_len = strlen(new_str);

    /* Count occurrences */
    size_t count = 0;
    const char* p = s;
    while ((p = strstr(p, old_str)) != NULL) { count++; p += old_len; }

    size_t out_len = s_len + count * (new_len - old_len);
    char* out = (char*)__visuall_alloc(out_len + 1, VSL_TAG_STRING);
    char* dst = out;
    p = s;
    while (1) {
        const char* found = strstr(p, old_str);
        if (!found) {
            strcpy(dst, p);
            break;
        }
        size_t chunk = (size_t)(found - p);
        memcpy(dst, p, chunk);
        dst += chunk;
        memcpy(dst, new_str, new_len);
        dst += new_len;
        p = found + old_len;
    }
    return out;
}

int64_t __visuall_string_starts_with(const char* s, const char* prefix) {
    if (!s || !prefix) return 0;
    size_t sl = strlen(s), pl = strlen(prefix);
    if (pl > sl) return 0;
    return memcmp(s, prefix, pl) == 0 ? 1 : 0;
}

int64_t __visuall_string_ends_with(const char* s, const char* suffix) {
    if (!s || !suffix) return 0;
    size_t sl = strlen(s), xl = strlen(suffix);
    if (xl > sl) return 0;
    return memcmp(s + sl - xl, suffix, xl) == 0 ? 1 : 0;
}

int64_t __visuall_string_contains(const char* s, const char* sub) {
    if (!s || !sub) return 0;
    return strstr(s, sub) != NULL ? 1 : 0;
}

int64_t __visuall_string_find(const char* s, const char* sub) {
    if (!s || !sub) return -1;
    const char* p = strstr(s, sub);
    return p ? (int64_t)(p - s) : -1;
}

char* __visuall_string_repeat(const char* s, int64_t n) {
    if (!s || n <= 0) return vsl_strdup("");
    size_t len = strlen(s);
    size_t total = len * (size_t)n;
    char* out = (char*)__visuall_alloc(total + 1, VSL_TAG_STRING);
    for (int64_t i = 0; i < n; i++)
        memcpy(out + i * len, s, len);
    out[total] = '\0';
    return out;
}

char* __visuall_string_format(const char* template_str, VisualList* args) {
    /* Simple {} placeholder replacement */
    if (!template_str) return vsl_strdup("");

    size_t tlen = strlen(template_str);
    /* Generous buffer — use plain malloc for the working copy since we
       may need to realloc.  Copy into a GC string at the end. */
    size_t buf_cap = tlen + 256;
    char* buf = (char*)malloc(buf_cap);
    if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
    size_t out_pos = 0;
    int64_t arg_idx = 0;

    for (size_t i = 0; i < tlen; i++) {
        if (template_str[i] == '{' && i + 1 < tlen && template_str[i + 1] == '}') {
            /* Replace {} with next arg (as string via int_to_str) */
            if (args && arg_idx < args->length) {
                char* s = __visuall_int_to_str(args->data[arg_idx++]);
                size_t slen = strlen(s);
                while (out_pos + slen + 1 >= buf_cap) {
                    buf_cap *= 2;
                    buf = (char*)realloc(buf, buf_cap);
                    if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
                }
                memcpy(buf + out_pos, s, slen);
                out_pos += slen;
                /* s is GC-managed, don't free */
            }
            i++; /* skip '}' */
        } else {
            if (out_pos + 2 >= buf_cap) {
                buf_cap *= 2;
                buf = (char*)realloc(buf, buf_cap);
                if (!buf) { fprintf(stderr, "out of memory\n"); exit(1); }
            }
            buf[out_pos++] = template_str[i];
        }
    }
    buf[out_pos] = '\0';
    /* Copy into a GC-managed string. */
    char* result = (char*)__visuall_alloc(out_pos + 1, VSL_TAG_STRING);
    memcpy(result, buf, out_pos + 1);
    free(buf);
    return result;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: math
 * ═══════════════════════════════════════════════════════════════════════════ */

double __visuall_math_pi(void) { return 3.14159265358979323846; }
double __visuall_math_e(void)  { return 2.71828182845904523536; }

double __visuall_math_sqrt(double x)  { return sqrt(x); }
double __visuall_math_pow(double b, double e) { return pow(b, e); }
int64_t __visuall_math_floor(double x) { return (int64_t)floor(x); }
int64_t __visuall_math_ceil(double x)  { return (int64_t)ceil(x); }
double __visuall_math_abs(double x)    { return fabs(x); }
double __visuall_math_sin(double x)    { return sin(x); }
double __visuall_math_cos(double x)    { return cos(x); }
double __visuall_math_tan(double x)    { return tan(x); }
double __visuall_math_log(double x)    { return log(x); }
double __visuall_math_log2(double x)   { return log2(x); }
double __visuall_math_log10(double x)  { return log10(x); }

double __visuall_math_clamp(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

double __visuall_math_lerp(double a, double b, double t) {
    return a + (b - a) * t;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: io
 * ═══════════════════════════════════════════════════════════════════════════ */

char* __visuall_io_read_file(const char* path) {
    if (!path) return vsl_strdup("");
    FILE* f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "IOError: cannot open file '%s'\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)__visuall_alloc((size_t)size + 1, VSL_TAG_STRING);
    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

void __visuall_io_write_file(const char* path, const char* content) {
    if (!path) return;
    FILE* f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "IOError: cannot open file '%s' for writing\n", path);
        exit(1);
    }
    if (content) fwrite(content, 1, strlen(content), f);
    fclose(f);
}

void __visuall_io_append_file(const char* path, const char* content) {
    if (!path) return;
    FILE* f = fopen(path, "ab");
    if (!f) {
        fprintf(stderr, "IOError: cannot open file '%s' for appending\n", path);
        exit(1);
    }
    if (content) fwrite(content, 1, strlen(content), f);
    fclose(f);
}

int64_t __visuall_io_file_exists(const char* path) {
    if (!path) return 0;
    FILE* f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

void __visuall_io_delete_file(const char* path) {
    if (!path) return;
    if (remove(path) != 0) {
        fprintf(stderr, "IOError: cannot delete file '%s'\n", path);
    }
}

VisualList* __visuall_io_list_dir(const char* path) {
    VisualList* out = __visuall_list_new();
    if (!path) return out;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return out;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;
        __visuall_list_push(out, (int64_t)vsl_strdup(fd.cFileName));
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR* dir = opendir(path);
    if (!dir) return out;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        __visuall_list_push(out, (int64_t)vsl_strdup(entry->d_name));
    }
    closedir(dir);
#endif
    return out;
}

void __visuall_io_make_dir(const char* path) {
    if (!path) return;
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: sys
 * ═══════════════════════════════════════════════════════════════════════════ */

/* sys.args is populated by main — stored as a global list */
static VisualList* __visuall_sys_args = NULL;
static int    __visuall_sys_argc = 0;
static char** __visuall_sys_argv = NULL;

void __visuall_sys_init(int argc, char** argv) {
    __visuall_sys_argc = argc;
    __visuall_sys_argv = argv;
    __visuall_sys_args = __visuall_list_new();
    for (int i = 0; i < argc; i++) {
        __visuall_list_push(__visuall_sys_args, (int64_t)argv[i]);
    }
}

VisualList* __visuall_sys_get_args(void) {
    if (!__visuall_sys_args)
        __visuall_sys_args = __visuall_list_new();
    return __visuall_sys_args;
}

void __visuall_sys_exit(int64_t code) {
    exit((int)code);
}

char* __visuall_sys_env(const char* key) {
    if (!key) return NULL;
    const char* val = getenv(key);
    if (!val) return NULL;
    return vsl_strdup(val);
}

const char* __visuall_sys_platform(void) {
#ifdef _WIN32
    return "windows";
#elif __APPLE__
    return "macos";
#else
    return "linux";
#endif
}

double __visuall_sys_time(void) {
    struct timespec ts;
#ifdef _WIN32
    timespec_get(&ts, TIME_UTC);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: collections — Stack
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    VisualList* list;
} VisualStack;

VisualStack* __visuall_stack_new(void) {
    VisualStack* s = (VisualStack*)__visuall_alloc(sizeof(VisualStack),
                                                    VSL_TAG_OBJECT);
    s->list = __visuall_list_new();
    return s;
}

void __visuall_stack_push(VisualStack* s, int64_t val) {
    if (!s) return;
    __visuall_list_push(s->list, val);
}

int64_t __visuall_stack_pop(VisualStack* s) {
    if (!s || s->list->length == 0) {
        fprintf(stderr, "StackError: pop from empty stack\n");
        exit(1);
    }
    return s->list->data[--s->list->length];
}

int64_t __visuall_stack_peek(VisualStack* s) {
    if (!s || s->list->length == 0) {
        fprintf(stderr, "StackError: peek at empty stack\n");
        exit(1);
    }
    return s->list->data[s->list->length - 1];
}

int64_t __visuall_stack_is_empty(VisualStack* s) {
    return (!s || s->list->length == 0) ? 1 : 0;
}

int64_t __visuall_stack_size(VisualStack* s) {
    return s ? s->list->length : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: collections — Queue
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    VisualList* list;
    int64_t     front;
} VisualQueue;

VisualQueue* __visuall_queue_new(void) {
    VisualQueue* q = (VisualQueue*)__visuall_alloc(sizeof(VisualQueue),
                                                    VSL_TAG_OBJECT);
    q->list  = __visuall_list_new();
    q->front = 0;
    return q;
}

void __visuall_queue_enqueue(VisualQueue* q, int64_t val) {
    if (!q) return;
    __visuall_list_push(q->list, val);
}

int64_t __visuall_queue_dequeue(VisualQueue* q) {
    if (!q || q->front >= q->list->length) {
        fprintf(stderr, "QueueError: dequeue from empty queue\n");
        exit(1);
    }
    return q->list->data[q->front++];
}

int64_t __visuall_queue_is_empty(VisualQueue* q) {
    return (!q || q->front >= q->list->length) ? 1 : 0;
}

int64_t __visuall_queue_size(VisualQueue* q) {
    return q ? (q->list->length - q->front) : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Module: collections — Set
 * Uses a sorted-array approach for simplicity (O(n) insert, O(log n) lookup)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int64_t* data;
    int64_t  length;
    int64_t  capacity;
} VisualSet;

VisualSet* __visuall_set_new(void) {
    VisualSet* s = (VisualSet*)__visuall_alloc(sizeof(VisualSet),
                                                VSL_TAG_OBJECT);
    s->capacity = 8;
    s->length   = 0;
    s->data     = (int64_t*)malloc(sizeof(int64_t) * (size_t)s->capacity);
    if (!s->data) { fprintf(stderr, "out of memory\n"); exit(1); }
    return s;
}

static int64_t set_find_idx(VisualSet* s, int64_t val) {
    /* Linear search (simple; could be binary) */
    for (int64_t i = 0; i < s->length; i++) {
        if (s->data[i] == val) return i;
    }
    return -1;
}

void __visuall_set_add(VisualSet* s, int64_t val) {
    if (!s) return;
    if (set_find_idx(s, val) >= 0) return; /* already present */
    if (s->length >= s->capacity) {
        s->capacity *= 2;
        s->data = (int64_t*)realloc(s->data, sizeof(int64_t) * (size_t)s->capacity);
        if (!s->data) { fprintf(stderr, "out of memory\n"); exit(1); }
    }
    s->data[s->length++] = val;
}

void __visuall_set_remove(VisualSet* s, int64_t val) {
    if (!s) return;
    int64_t idx = set_find_idx(s, val);
    if (idx < 0) return;
    /* Swap with last */
    s->data[idx] = s->data[--s->length];
}

int64_t __visuall_set_contains(VisualSet* s, int64_t val) {
    if (!s) return 0;
    return set_find_idx(s, val) >= 0 ? 1 : 0;
}

int64_t __visuall_set_size(VisualSet* s) {
    return s ? s->length : 0;
}

VisualList* __visuall_set_to_list(VisualSet* s) {
    VisualList* out = __visuall_list_new();
    if (!s) return out;
    for (int64_t i = 0; i < s->length; i++)
        __visuall_list_push(out, s->data[i]);
    return out;
}

VisualSet* __visuall_set_union(VisualSet* a, VisualSet* b) {
    VisualSet* out = __visuall_set_new();
    if (a) {
        for (int64_t i = 0; i < a->length; i++)
            __visuall_set_add(out, a->data[i]);
    }
    if (b) {
        for (int64_t i = 0; i < b->length; i++)
            __visuall_set_add(out, b->data[i]);
    }
    return out;
}

VisualSet* __visuall_set_intersect(VisualSet* a, VisualSet* b) {
    VisualSet* out = __visuall_set_new();
    if (!a || !b) return out;
    for (int64_t i = 0; i < a->length; i++) {
        if (__visuall_set_contains(b, a->data[i]))
            __visuall_set_add(out, a->data[i]);
    }
    return out;
}
