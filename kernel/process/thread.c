#include "process.h"

static uint32_t next_tid = 1;

// Fungsi untuk membuat thread baru (misal: Thread Menu, Thread Data, Thread Monitor)
thread_t* create_thread(void (*entry_point)()) {
    // Alokasi statis sederhana (atau nanti menggunakan kmalloc dari Memory Management)
    static thread_t thread_pool[10];
    static int pool_index = 0;

    if (pool_index >= 10) return 0; // Maksimal 10 thread sederhana

    thread_t *t = &thread_pool[pool_index++];
    t->tid = next_tid++;
    t->state = THREAD_NEW;
    t->next = 0;

    // Menyiapkan stack teratas
    uint32_t *stack_top = (uint32_t *)(t->stack + STACK_SIZE);

    // Memasukkan entry point fungsi ke stack (seolah-olah dipanggil lewat fungsi)
    *(--stack_top) = (uint32_t)entry_point;

    // Set register awal
    t->context.esp = (uint32_t)stack_top;
    t->context.eip = (uint32_t)entry_point;
    t->state = THREAD_READY;

    return t;
}