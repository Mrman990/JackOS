#include "process.h"

static thread_t *ready_queue_head = 0;
static thread_t *ready_queue_tail = 0;
static thread_t *current_thread = 0;

// Menambahkan thread ke antrean Ready Queue (Linked List Circular/Queue)
void enqueue_thread(thread_t *t) {
    t->next = 0;
    if (!ready_queue_head) {
        ready_queue_head = t;
        ready_queue_tail = t;
    } else {
        ready_queue_tail->next = t;
        ready_queue_tail = t;
    }
}

// Inisialisasi Scheduler
void scheduler_init(void) {
    ready_queue_head = 0;
    ready_queue_tail = 0;
    current_thread = 0;
}

// Algoritma Round Robin: Memilih thread berikutnya yang siap jalan
void schedule(void) {
    if (!ready_queue_head) return;

    // Jika ada thread yang sedang jalan, kembalikan ke antrean READY
    if (current_thread && current_thread->state == THREAD_RUNNING) {
        current_thread->state = THREAD_READY;
        enqueue_thread(current_thread);
    }

    // Ambil thread paling depan dari antrean READY
    current_thread = ready_queue_head;
    ready_queue_head = ready_queue_head->next;
    if (!ready_queue_head) {
        ready_queue_tail = 0;
    }

    // Ubah status thread menjadi RUNNING
    current_thread->state = THREAD_RUNNING;

    // Di sini nantinya context_switch(&previous_context, &current_thread->context)
    // dipanggil via Assembly (.asm)
}

// Meminta giliran pindah thread secara sukarela (voluntary yield)
void yield(void) {
    schedule();
}