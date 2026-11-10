#include "process.h"

static uint32_t next_tid = 1;
static thread_t thread_pool[MAX_THREADS];

int thread_count(void)
{
    return MAX_THREADS;
}

thread_t *thread_at(int index)
{
    if (index < 0 || index >= MAX_THREADS)
        return 0;
    return &thread_pool[index];
}

const char *thread_state_name(thread_state_t s)
{
    switch (s)
    {
    case THREAD_NEW:
        return "NEW";
    case THREAD_READY:
        return "READY";
    case THREAD_RUNNING:
        return "RUNNING";
    case THREAD_WAITING:
        return "WAITING";
    case THREAD_TERMINATED:
        return "TERMINATED";
    }
    return "?";
}

// Membuat thread baru: status NEW, lalu masuk Ready Queue (READY).
// Slot kosong dipakai lebih dulu supaya riwayat thread yang sudah selesai
// tetap terlihat di perintah "threads". Slot TERMINATED baru dipakai ulang
// kalau semua slot sudah pernah terpakai.
thread_t *create_thread(const char *name, void (*entry_point)(void))
{
    thread_t *t = 0;
    for (int i = 0; i < MAX_THREADS; i++)
    {
        if (!thread_pool[i].in_use)
        {
            t = &thread_pool[i];
            break;
        }
    }
    if (!t)
    {
        for (int i = 0; i < MAX_THREADS; i++)
        {
            if (thread_pool[i].state == THREAD_TERMINATED)
            {
                t = &thread_pool[i];
                break;
            }
        }
    }
    if (!t)
        return 0; // semua slot terpakai

    t->in_use = 1;
    t->tid = next_tid++;
    t->name = name;
    t->entry = entry_point;
    t->next = 0;
    t->wake_tick = 0;
    t->state = THREAD_NEW;
    thread_set_state(t, THREAD_NEW); // catat status awal: NEW

    // Siapkan stack teratas:
    //   [alamat thread_exit]  <- tempat kembali kalau entry_point selesai
    //   [entry_point]         <- esp menunjuk ke sini (dipakai oleh "ret")
    uint32_t *stack_top = (uint32_t *)(t->stack + STACK_SIZE);
    *(--stack_top) = (uint32_t)(uintptr_t)thread_exit;
    *(--stack_top) = (uint32_t)(uintptr_t)entry_point;

    t->context.esp = (uint32_t)(uintptr_t)stack_top;
    t->context.eip = (uint32_t)(uintptr_t)entry_point;
    t->context.ebx = 0;
    t->context.esi = 0;
    t->context.edi = 0;
    t->context.ebp = 0;

    scheduler_add(t); // NEW -> READY
    return t;
}