/* ═══════════════════════════════════════════════════════════════════════════
 * Visuall Scheduler — M:N goroutine runtime
 *
 * Implements lightweight cooperative goroutines with continuation-passing
 * channels.  Goroutines yield when a channel operation blocks; the OS thread
 * immediately picks up the next runnable task (Trap 6 fix).
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifndef VISUALL_SCHEDULER_H
#define VISUALL_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Task descriptor (GC-managed, VSL_TAG_TASK) ───────────────────────── */
typedef struct VisuallTask {
    void*             fn_ptr;       /* goroutine entry / resume function        */
    void*             env;          /* environment (closure + saved variables)  */
    int64_t           state;        /* state-machine position (0 = start)       */
    int64_t           recv_value;   /* value delivered by direct channel handoff */
    bool              has_recv;     /* true when recv_value is valid             */
    struct VisuallTask* next;       /* intrusive linked-list for scheduler queues */
} VisuallTask;

/* ── Channel (GC-managed, VSL_TAG_CHANNEL) ────────────────────────────── */
typedef struct VisuallChannel {
    int64_t*  buffer;               /* ring buffer (NULL for unbuffered)   */
    int64_t   capacity;
    int64_t   count;
    int64_t   head;
    int64_t   tail;
    bool      closed;

    /* Blocked-task wait queues (Trap 9: scanned by GC mark phase). */
    VisuallTask* blocked_senders_head;
    VisuallTask* blocked_receivers_head;

    /* Platform synchronisation */
#ifdef _WIN32
    void* lock;     /* CRITICAL_SECTION* */
    void* send_cv;  /* CONDITION_VARIABLE* */
    void* recv_cv;  /* CONDITION_VARIABLE* */
#else
    void* lock;     /* pthread_mutex_t* */
    void* send_cv;  /* pthread_cond_t* */
    void* recv_cv;  /* pthread_cond_t* */
#endif
} VisuallChannel;

/* ── Scheduler lifecycle ──────────────────────────────────────────────── */

/* Create and enqueue a goroutine.  The task descriptor and its env/arg
   array must be GC-allocated (VSL_TAG_TASK).  The scheduler's ready queue
   is registered as a GC root. */
void __visuall_go_create(VisuallTask* task);

/* Initialise the scheduler (called once, lazily on first go_create). */
void __visuall_sched_init(void);

/* Scheduler ready queue — exposed for GC root tracing (Trap 5). */
extern VisuallTask* ready_head;
extern VisuallTask* ready_tail;

/* ── Channel API (continuation-based — Trap 6 fix) ────────────────────── */

/* Create a channel.  capacity = 0 means unbuffered. */
VisuallChannel* __visuall_chan_create(int64_t capacity);

/* Send to channel.  If blocked, registers (cont, cont_env) and yields.
   Pass NULL for cont/cont_env for fire-and-forget sends (blocks thread). */
void __visuall_chan_send(VisuallChannel* ch, int64_t value,
                          void* cont, void* cont_env);

/* Receive from channel.  If blocked, registers (cont, cont_env) and yields.
   Returns the received value. */
int64_t __visuall_chan_recv(VisuallChannel* ch,
                             void* cont, void* cont_env);

/* Close the channel — wakes all blocked tasks. */
void __visuall_chan_close(VisuallChannel* ch);

#ifdef __cplusplus
}
#endif

#endif /* VISUALL_SCHEDULER_H */
