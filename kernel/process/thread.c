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

// Trampoline dipanggil pertama kali saat thread dijalankan via iret
static void thread_trampoline(void)
{
    __asm__ __volatile__("sti");

    thread_t *cur = get_current_thread();
    if (cur && cur->entry)
    {
        cur->entry();
    }
    thread_exit();
}

// Membuat thread baru: status NEW, lalu masuk Ready Queue (READY).
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
    t->time_slice = DEFAULT_QUANTUM;
    t->priority = 1;
    t->state = THREAD_NEW;
    thread_set_state(t, THREAD_NEW); // catat status awal: NEW

    // Siapkan stack frame seolah-olah thread sedang di-interrupt.
    // Saat scheduler melakukan iret ke thread ini, CPU akan memuat register ini.
    uint8_t *stk = t->stack + STACK_SIZE;
    stk = (uint8_t *)((uintptr_t)stk & ~0xF); // align 16 byte

    stk -= sizeof(interrupt_frame_t);
    interrupt_frame_t *frame = (interrupt_frame_t *)stk;

    // Segment registers (Kernel Data = 0x10)
    frame->gs = 0x10;
    frame->fs = 0x10;
    frame->es = 0x10;
    frame->ds = 0x10;

    // General registers
    frame->edi = 0;
    frame->esi = 0;
    frame->ebp = 0;
    frame->esp = (uint32_t)(uintptr_t)stk;
    frame->ebx = 0;
    frame->edx = 0;
    frame->ecx = 0;
    frame->eax = 0;

    // Hardware interrupt return context
    frame->eip = (uint32_t)(uintptr_t)thread_trampoline;
    frame->cs = 0x08;   // Kernel Code selector
    frame->eflags = 0x0202; // IF = 1 (Interrupt Flag diaktifkan)

    t->saved_esp = (uint32_t)(uintptr_t)stk;

    scheduler_add(t); // NEW -> READY
    return t;
}

// Dipanggil otomatis atau manual saat fungsi thread selesai: RUNNING -> TERMINATED
void thread_exit(void)
{
    __asm__ __volatile__("cli");
    thread_t *t = get_current_thread();
    if (t)
    {
        thread_set_state(t, THREAD_TERMINATED);
    }
    // Paksa context switch keluar dari thread ini via software interrupt
    __asm__ __volatile__("int $0x20");
    for (;;)
    {
        __asm__ __volatile__("hlt");
    }
}
