#include "process.h"

static thread_t *queue_head = 0;
static thread_t *queue_tail = 0;
static thread_t *current_thread = 0;
static cpu_context_t scheduler_context; // konteks "dispatcher" (shell)
static uint32_t ticks = 0;
static thread_log_fn logger = 0;

// Pindah konteks: simpan register ke old_ctx, muat register dari new_ctx.
void context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx);
__asm__(
    ".text\n"
    ".globl context_switch\n"
    "context_switch:\n"
    "    movl 4(%esp), %eax\n"  // old_ctx
    "    movl 8(%esp), %edx\n"  // new_ctx
    "    movl %ebx, 4(%eax)\n"
    "    movl %esi, 8(%eax)\n"
    "    movl %edi, 12(%eax)\n"
    "    movl %ebp, 16(%eax)\n"
    "    movl %esp, 0(%eax)\n"  // esp menunjuk ke alamat kembali
    "    movl 0(%edx), %esp\n"
    "    movl 4(%edx), %ebx\n"
    "    movl 8(%edx), %esi\n"
    "    movl 12(%edx), %edi\n"
    "    movl 16(%edx), %ebp\n"
    "    ret\n"                 // lanjut ke alamat kembali thread tujuan
);

static void enqueue_thread(thread_t *t) {
    t->next = 0;
    if (!queue_head) {
        queue_head = t;
        queue_tail = t;
    } else {
        queue_tail->next = t;
        queue_tail = t;
    }
}

static thread_t *dequeue_thread(void) {
    thread_t *t = queue_head;
    if (!t) return 0;
    queue_head = t->next;
    if (!queue_head) queue_tail = 0;
    t->next = 0;
    return t;
}

void scheduler_init(void) {
    queue_head = 0;
    queue_tail = 0;
    current_thread = 0;
    ticks = 0;
}

void scheduler_set_logger(thread_log_fn fn) {
    logger = fn;
}

thread_t *get_current_thread(void) {
    return current_thread;
}

uint32_t scheduler_ticks(void) {
    return ticks;
}

// Ubah status thread dan laporkan perubahannya.
void thread_set_state(thread_t *t, thread_state_t s) {
    thread_state_t old = t->state;
    t->state = s;
    if (logger) logger(t, old, s);
}

// NEW -> READY, lalu masuk Ready Queue.
void scheduler_add(thread_t *t) {
    thread_set_state(t, THREAD_READY);
    enqueue_thread(t);
}

// Dispatcher Round Robin. Dipanggil dari shell; kembali kalau semua thread selesai.
void scheduler_run(void) {
    for (;;) {
        // Bangunkan thread WAITING yang waktunya sudah tiba, hitung thread yang masih hidup.
        int alive = 0;
        for (int i = 0; i < thread_count(); i++) {
            thread_t *t = thread_at(i);
            if (!t->in_use) continue;
            if (t->state == THREAD_WAITING) {
                alive++;
                if (ticks >= t->wake_tick) {
                    thread_set_state(t, THREAD_READY);
                    enqueue_thread(t);
                }
            } else if (t->state == THREAD_READY) {
                alive++;
            }
        }
        if (alive == 0) break;

        thread_t *next = dequeue_thread();
        ticks++;
        if (!next) continue; // semua thread menunggu: waktu terus berjalan

        current_thread = next;
        thread_set_state(next, THREAD_RUNNING);
        context_switch(&scheduler_context, &next->context);
        current_thread = 0;

        // Thread kembali ke sini lewat yield / sleep / selesai.
        if (next->state == THREAD_READY) enqueue_thread(next);
    }
}

// Melepas giliran secara sukarela: RUNNING -> READY.
void yield(void) {
    thread_t *t = current_thread;
    if (!t) return;
    thread_set_state(t, THREAD_READY);
    context_switch(&t->context, &scheduler_context);
}

// Menunggu beberapa tick: RUNNING -> WAITING.
void thread_sleep(uint32_t n) {
    thread_t *t = current_thread;
    if (!t) return;
    t->wake_tick = ticks + n;
    thread_set_state(t, THREAD_WAITING);
    context_switch(&t->context, &scheduler_context);
}

// Dipanggil otomatis kalau fungsi thread selesai (return): RUNNING -> TERMINATED.
void thread_exit(void) {
    thread_t *t = current_thread;
    if (t) {
        thread_set_state(t, THREAD_TERMINATED);
        context_switch(&t->context, &scheduler_context);
    }
    for (;;) { }
}