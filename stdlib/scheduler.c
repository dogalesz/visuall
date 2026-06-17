/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall Scheduler — M:N goroutine runtime implementation
 *
 * v1: multi-threaded cooperative scheduler with continuation-passing channels.
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
 * Scheduler state
 * ═══════════════════════════════════════════════════════════════════════════ */

VisuallTask* ready_head = NULL;
VisuallTask* ready_tail = NULL;
static bool sched_initialised = false;
static bool sched_running = true;

#define WORKER_COUNT 2

/* ── Scheduler mutex (V02 fix) ─────────────────────────────────────────── */
#ifdef _WIN32
/* SRWLOCK supports static initialization (unlike CRITICAL_SECTION),
   so it's always safe to acquire even before __visuall_sched_init(). */
static SRWLOCK sched_lock = SRWLOCK_INIT;
#define SCHED_LOCK()   AcquireSRWLockExclusive(&sched_lock)
#define SCHED_UNLOCK() ReleaseSRWLockExclusive(&sched_lock)
#else
static pthread_mutex_t sched_lock = PTHREAD_MUTEX_INITIALIZER;
#define SCHED_LOCK()   pthread_mutex_lock(&sched_lock)
#define SCHED_UNLOCK() pthread_mutex_unlock(&sched_lock)
#endif

/* ── Channel lock helpers (V03 fix) ────────────────────────────────────── */
#ifdef _WIN32
#define CHAN_LOCK(ch)   EnterCriticalSection((CRITICAL_SECTION*)(ch)->lock)
#define CHAN_UNLOCK(ch) LeaveCriticalSection((CRITICAL_SECTION*)(ch)->lock)
#else
#define CHAN_LOCK(ch)   pthread_mutex_lock((pthread_mutex_t*)(ch)->lock)
#define CHAN_UNLOCK(ch) pthread_mutex_unlock((pthread_mutex_t*)(ch)->lock)
#endif

static void sched_enqueue(VisuallTask* task);
static VisuallTask* sched_dequeue(void);

/* Enqueue a task at the tail of the ready queue.  Caller must hold sched_lock. */
static void sched_enqueue_locked(VisuallTask* task) {
    task->next = NULL;
    if (!ready_head) {
        ready_head = ready_tail = task;
    } else {
        ready_tail->next = task;
        ready_tail = task;
    }
}

/* Dequeue a task from the head of the ready queue.  Caller must hold sched_lock. */
static VisuallTask* sched_dequeue_locked(void) {
    if (!ready_head) return NULL;
    VisuallTask* t = ready_head;
    ready_head = t->next;
    if (!ready_head) ready_tail = NULL;
    t->next = NULL;
    return t;
}

/* ── Public wrappers that take the lock ────────────────────────────────── */

static void sched_enqueue(VisuallTask* task) {
    SCHED_LOCK();
    sched_enqueue_locked(task);
    SCHED_UNLOCK();
}

static VisuallTask* sched_dequeue(void) {
    SCHED_LOCK();
    VisuallTask* t = sched_dequeue_locked();
    SCHED_UNLOCK();
    return t;
}

/* ── Channel helpers (caller must hold channel lock) ───────────────────── */

/* Enqueue a task on a blocked queue. */
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

/* Dequeue from a blocked queue. */
static VisuallTask* blocked_dequeue(VisuallTask** head) {
    if (!*head) return NULL;
    VisuallTask* t = *head;
    *head = t->next;
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
    task->has_recv = false;
    task->recv_value = 0;
    task->next = NULL;
    sched_enqueue(task);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Channel implementation (V03/V09/V10 fixes applied)
 * ═══════════════════════════════════════════════════════════════════════════ */

VisuallChannel* __visuall_chan_create(int64_t capacity) {
    VisuallChannel* ch = (VisuallChannel*)__visuall_alloc(
        sizeof(VisuallChannel), VSL_TAG_CHANNEL);
    if (!ch) return NULL;
    memset(ch, 0, sizeof(VisuallChannel));

    if (capacity > 0) {
        ch->buffer = (int64_t*)malloc((size_t)(capacity * sizeof(int64_t)));
        if (ch->buffer)
            memset(ch->buffer, 0, (size_t)(capacity * sizeof(int64_t)));
    }
    ch->capacity = capacity;
    ch->closed   = false;
    ch->blocked_senders_head   = NULL;
    ch->blocked_receivers_head = NULL;

#ifdef _WIN32
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION));
    if (cs) InitializeCriticalSection(cs);
    ch->lock = cs;
#else
    pthread_mutex_t* mtx = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (mtx) pthread_mutex_init(mtx, NULL);
    ch->lock = mtx;
#endif
    return ch;
}

void __visuall_chan_send(VisuallChannel* ch, int64_t value,
                          void* cont, void* cont_env) {
    if (!ch) return;

    CHAN_LOCK(ch);

    if (ch->closed) {
        CHAN_UNLOCK(ch);
        return;
    }

    /* Try to pair with a waiting receiver — direct handoff (V09 fix). */
    VisuallTask* receiver = blocked_dequeue(&ch->blocked_receivers_head);
    if (receiver) {
        /* Direct handoff: store value in receiver and wake it. */
        receiver->recv_value = value;
        receiver->has_recv   = true;
        CHAN_UNLOCK(ch);
        /* Enqueue OUTSIDE channel lock to avoid deadlock with sched_lock. */
        sched_enqueue(receiver);
        return;
    }

    if (ch->capacity > 0) {
        /* Buffered channel: store in ring buffer. */
        if (ch->count < ch->capacity) {
            ch->buffer[ch->tail] = value;
            ch->tail = (ch->tail + 1) % ch->capacity;
            ch->count++;
            CHAN_UNLOCK(ch);
            return;
        }
    }

    /* Block: register continuation. */
    if (cont) {
        VisuallTask* self = (VisuallTask*)cont_env;
        if (self) {
            blocked_enqueue(&ch->blocked_senders_head, self);
        }
    }

    CHAN_UNLOCK(ch);
}

int64_t __visuall_chan_recv(VisuallChannel* ch,
                             void* cont, void* cont_env) {
    if (!ch) return 0;

    CHAN_LOCK(ch);

    /* Check for value from direct handoff (V09 fix). */
    /* The caller's task may have been woken by a sender that stored a value. */
    if (cont_env) {
        VisuallTask* self = (VisuallTask*)cont_env;
        if (self && self->has_recv) {
            int64_t val = self->recv_value;
            self->has_recv = false;
            self->recv_value = 0;
            CHAN_UNLOCK(ch);
            return val;
        }
    }

    /* Try to pair with a waiting sender. */
    VisuallTask* sender = blocked_dequeue(&ch->blocked_senders_head);
    if (sender) {
        if (ch->count > 0) {
            /* Buffered: read from buffer, wake sender so it can enqueue more. */
            int64_t val = ch->buffer[ch->head];
            ch->head = (ch->head + 1) % ch->capacity;
            ch->count--;
            CHAN_UNLOCK(ch);
            sched_enqueue(sender);
            return val;
        }
        /* Unbuffered: no value available yet, wake sender to retry.
           The sender will retry chan_send and find this receiver gone,
           and a new receiver may pick up the value. */
        CHAN_UNLOCK(ch);
        sched_enqueue(sender);
        /* Fall through to check buffer in case sender wrote meanwhile. */
        CHAN_LOCK(ch);
    }

    if (ch->count > 0) {
        int64_t val = ch->buffer[ch->head];
        ch->head = (ch->head + 1) % ch->capacity;
        ch->count--;
        CHAN_UNLOCK(ch);
        return val;
    }

    /* Block: register continuation. */
    if (cont && cont_env) {
        VisuallTask* self = (VisuallTask*)cont_env;
        blocked_enqueue(&ch->blocked_receivers_head, self);
    }

    CHAN_UNLOCK(ch);
    return 0;
}

void __visuall_chan_close(VisuallChannel* ch) {
    if (!ch) return;

    CHAN_LOCK(ch);
    ch->closed = true;

    /* Wake all blocked tasks — re-enqueue so they can observe closed (V10 fix). */
    VisuallTask* t;
    while ((t = blocked_dequeue(&ch->blocked_receivers_head))) {
        sched_enqueue(t);
    }
    while ((t = blocked_dequeue(&ch->blocked_senders_head))) {
        sched_enqueue(t);
    }

    CHAN_UNLOCK(ch);
}
