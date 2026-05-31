/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall Scheduler — M:N goroutine runtime implementation
 *
 * v1: single-threaded cooperative scheduler with continuation-passing channels.
 * Multi-threaded work-stealing is deferred to v2.
 * ═══════════════════════════════════════════════════════════════════════════ */

#include "scheduler.h"
#include "gc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Scheduler state (v1: single ready queue)
 * ═══════════════════════════════════════════════════════════════════════════ */

VisuallTask* ready_head = NULL;
VisuallTask* ready_tail = NULL;
static bool sched_initialised = false;
static bool sched_running = true;

#define WORKER_COUNT 2

static void sched_enqueue(VisuallTask* task);
static VisuallTask* sched_dequeue(void);

/* Enqueue a task at the tail of the ready queue. */
static void sched_enqueue(VisuallTask* task) {
    task->next = NULL;
    if (!ready_head) {
        ready_head = ready_tail = task;
    } else {
        ready_tail->next = task;
        ready_tail = task;
    }
}

/* Dequeue a task from the head of the ready queue.  Returns NULL if empty. */
static VisuallTask* sched_dequeue(void) {
    if (!ready_head) return NULL;
    VisuallTask* t = ready_head;
    ready_head = t->next;
    if (!ready_head) ready_tail = NULL;
    t->next = NULL;
    return t;
}

/* Worker thread loop — dequeues and executes goroutine tasks. */
#ifdef _WIN32
static DWORD WINAPI worker_loop(LPVOID arg) {
#else
static void* worker_loop(void* arg) {
#endif
    VisuallTask* t;
    __visuall_gc_register_thread();
    while (sched_running) {
        t = sched_dequeue();
        if (!t) {
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1000);
#endif
            continue;
        }
        if (t->fn_ptr) {
            ((void(*)(void*))t->fn_ptr)(t->env);
        }
    }
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

static void sched_spawn_workers(void) {
    int i;
#ifdef _WIN32
    for (i = 0; i < WORKER_COUNT; i++) {
        HANDLE h = CreateThread(NULL, 0, worker_loop, NULL, 0, NULL);
        if (h) CloseHandle(h);
    }
#else
    for (i = 0; i < WORKER_COUNT; i++) {
        pthread_t t;
        pthread_create(&t, NULL, worker_loop, NULL);
        pthread_detach(t);
    }
#endif
}

void __visuall_sched_init(void) {
    if (sched_initialised) return;
    sched_initialised = true;
    sched_spawn_workers();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Goroutine lifecycle
 * ═══════════════════════════════════════════════════════════════════════════ */

void __visuall_go_create(VisuallTask* task) {
    if (!task) return;
    if (!sched_initialised) __visuall_sched_init();
    task->state = 0;
    task->next = NULL;
    sched_enqueue(task);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Channel implementation (continuation-passing, v1 cooperative)
 * ═══════════════════════════════════════════════════════════════════════════ */

VisuallChannel* __visuall_chan_create(int64_t capacity) {
    VisuallChannel* ch = (VisuallChannel*)__visuall_alloc(
        sizeof(VisuallChannel), VSL_TAG_CHANNEL);
    if (!ch) return NULL;
    memset(ch, 0, sizeof(VisuallChannel));

    if (capacity > 0) {
        ch->buffer = (int64_t*)malloc((size_t)(capacity * sizeof(int64_t)));
        memset(ch->buffer, 0, (size_t)(capacity * sizeof(int64_t)));
    }
    ch->capacity = capacity;
    ch->closed   = false;
    ch->blocked_senders_head   = NULL;
    ch->blocked_receivers_head = NULL;

#ifdef _WIN32
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(cs);
    ch->lock = cs;
#else
    pthread_mutex_t* mtx = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mtx, NULL);
    ch->lock = mtx;
#endif
    return ch;
}

/* Helper: enqueue a task on a blocked queue. */
static void blocked_enqueue(VisuallTask** head, VisuallTask* task) {
    task->next = NULL;
    if (!*head) {
        *head = task;
    } else {
        VisuallTask* cur = *head;
        while (cur->next) cur = cur->next;
        cur->next = task;
    }
}

/* Helper: dequeue from a blocked queue. */
static VisuallTask* blocked_dequeue(VisuallTask** head) {
    if (!*head) return NULL;
    VisuallTask* t = *head;
    *head = t->next;
    t->next = NULL;
    return t;
}

void __visuall_chan_send(VisuallChannel* ch, int64_t value,
                          void* cont, void* cont_env) {
    if (!ch || ch->closed) return;

    /* Try to pair with a waiting receiver. */
    VisuallTask* receiver = blocked_dequeue(&ch->blocked_receivers_head);
    if (receiver) {
        /* Direct handoff: store value in receiver's env slot and wake it. */
        /* For v1: resume the receiver immediately (single-threaded). */
        sched_enqueue(receiver);
        return;
    }

    if (ch->capacity > 0) {
        /* Buffered channel: store in ring buffer. */
        if (ch->count < ch->capacity) {
            ch->buffer[ch->tail] = value;
            ch->tail = (ch->tail + 1) % ch->capacity;
            ch->count++;
            return;
        }
    }

    /* Block: register continuation and yield (v1: just enqueue self). */
    if (cont) {
        VisuallTask* self = (VisuallTask*)cont_env;
        if (self) {
            blocked_enqueue(&ch->blocked_senders_head, self);
        }
    }
}

int64_t __visuall_chan_recv(VisuallChannel* ch,
                             void* cont, void* cont_env) {
    if (!ch) return 0;

    /* Try to pair with a waiting sender. */
    VisuallTask* sender = blocked_dequeue(&ch->blocked_senders_head);
    if (sender) {
        /* Wake the sender. */
        sched_enqueue(sender);
        if (ch->count > 0) {
            /* Buffered: read from buffer */
            int64_t val = ch->buffer[ch->head];
            ch->head = (ch->head + 1) % ch->capacity;
            ch->count--;
            return val;
        }
        return 0;
    }

    if (ch->count > 0) {
        int64_t val = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        return val;
    }

    /* Block: register continuation. */
    if (cont && cont_env) {
        VisuallTask* self = (VisuallTask*)cont_env;
        blocked_enqueue(&ch->blocked_receivers_head, self);
    }
    return 0;
}

void __visuall_chan_close(VisuallChannel* ch) {
    if (!ch) return;
    ch->closed = true;
    /* Wake all blocked tasks — they will see closed flag on resume. */
    while (blocked_dequeue(&ch->blocked_receivers_head)) {}
    while (blocked_dequeue(&ch->blocked_senders_head)) {}
}
