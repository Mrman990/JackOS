#include "process.h"

static thread_t *queue_head = 0;
static thread_t *queue_tail = 0;
static thread_t *current_thread = 0;
static uint32_t ticks = 0;
static uint32_t preempt_count = 0;
static int scheduler_running = 0;
static volatile int yield_requested = 0;
static thread_log_fn logger = 0;
static preempt_log_fn preempt_logger = 0;

// Idle thread TCB jika semua thread sedang WAITING
static thread_t idle_tcb;
static uint8_t idle_stack[1024];

extern void scheduler_start(uint32_t next_esp);
extern uint32_t main_stack_ptr;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

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

static void idle_task(void) {
    while (1) {
        __asm__ __volatile__("sti; hlt");
    }
}

static void init_idle_thread(void) {
    idle_tcb.tid = 999;
    idle_tcb.name = "IDLE";
    idle_tcb.state = THREAD_READY;
    idle_tcb.in_use = 1;
    idle_tcb.time_slice = 1;

    uint8_t *stk = idle_stack + sizeof(idle_stack);
    stk = (uint8_t *)((uintptr_t)stk & ~0xF);
    stk -= sizeof(interrupt_frame_t);
    interrupt_frame_t *frame = (interrupt_frame_t *)stk;

    frame->gs = frame->fs = frame->es = frame->ds = 0x10;
    frame->edi = frame->esi = frame->ebp = frame->ebx = frame->edx = frame->ecx = frame->eax = 0;
    frame->esp = (uint32_t)(uintptr_t)stk;
    frame->eip = (uint32_t)(uintptr_t)idle_task;
    frame->cs = 0x08;
    frame->eflags = 0x0202;

    idle_tcb.saved_esp = (uint32_t)(uintptr_t)stk;
}

static int count_alive_threads(void) {
    int alive = 0;
    for (int i = 0; i < thread_count(); i++) {
        thread_t *t = thread_at(i);
        if (!t || !t->in_use) continue;
        if (t->state == THREAD_READY || t->state == THREAD_RUNNING || t->state == THREAD_WAITING) {
            alive++;
        }
    }
    return alive;
}

void scheduler_init(void) {
    queue_head = 0;
    queue_tail = 0;
    current_thread = 0;
    ticks = 0;
    preempt_count = 0;
    scheduler_running = 0;
    yield_requested = 0;
}

void scheduler_set_logger(thread_log_fn fn) {
    logger = fn;
}

void scheduler_set_preempt_logger(preempt_log_fn fn) {
    preempt_logger = fn;
}

thread_t *get_current_thread(void) {
    return current_thread;
}

uint32_t scheduler_ticks(void) {
    return ticks;
}

int scheduler_is_running(void) {
    return scheduler_running;
}

uint32_t scheduler_preempt_count(void) {
    return preempt_count;
}

// Ubah status thread dan laporkan perubahannya melalui logger.
void thread_set_state(thread_t *t, thread_state_t s) {
    if (!t) return;
    thread_state_t old = t->state;
    t->state = s;
    if (logger) logger(t, old, s);
}

// NEW -> READY, lalu masuk Ready Queue.
void scheduler_add(thread_t *t) {
    thread_set_state(t, THREAD_READY);
    enqueue_thread(t);
}

// ----------------------------------------------------------------------
// timer_handler: Dipanggil dari timer_isr pada switch.s setiap tick IRQ0
// ataupun saat software interrupt int $0x20 dieksekusi oleh yield/sleep/exit.
// ----------------------------------------------------------------------
void *timer_handler(interrupt_frame_t *frame) {
    // 1. Kirim sinyal End-Of-Interrupt (EOI) ke Master PIC (port 0x20)
    outb(0x20, 0x20);

    // 2. Tambah tick sistem
    ticks++;

    // 3. Jika scheduler sedang tidak aktif (misal saat mengetik di shell CLI),
    // jangan ubah stack atau lakukan context switch.
    if (!scheduler_running) {
        return frame;
    }

    // 4. Periksa thread WAITING: bangunkan jika waktu tidurnya sudah selesai
    for (int i = 0; i < thread_count(); i++) {
        thread_t *t = thread_at(i);
        if (!t || !t->in_use) continue;
        if (t->state == THREAD_WAITING) {
            if (ticks >= t->wake_tick) {
                thread_set_state(t, THREAD_READY);
                enqueue_thread(t);
            }
        }
    }

    // 5. Periksa apakah masih ada thread pekerja yang hidup
    int alive = count_alive_threads();
    if (alive == 0) {
        // Semua thread pekerja sudah selesai (TERMINATED).
        // Kembalikan eksekusi ke shell CLI (main_stack_ptr).
        scheduler_running = 0;
        current_thread = 0;
        return (void*)(uintptr_t)main_stack_ptr;
    }

    // 6. Simpan stack frame saat ini ke TCB thread yang sedang berjalan
    if (current_thread == &idle_tcb) {
        idle_tcb.saved_esp = (uint32_t)(uintptr_t)frame;
    } else if (current_thread) {
        current_thread->saved_esp = (uint32_t)(uintptr_t)frame;
    }

    // 7. Kasus A: Thread memanggil yield() secara sukarela
    if (yield_requested) {
        yield_requested = 0;
        if (current_thread && current_thread != &idle_tcb && current_thread->state == THREAD_RUNNING) {
            thread_set_state(current_thread, THREAD_READY);
            enqueue_thread(current_thread);
        }

        thread_t *next = dequeue_thread();
        if (!next) {
            next = &idle_tcb;
        }
        current_thread = next;
        next->time_slice = DEFAULT_QUANTUM;
        if (next != &idle_tcb) {
            thread_set_state(next, THREAD_RUNNING);
        }
        return (void*)(uintptr_t)next->saved_esp;
    }

    // 8. Kasus B: Thread baru saja selesai (TERMINATED) melalui thread_exit()
    if (current_thread && current_thread->state == THREAD_TERMINATED) {
        thread_t *next = dequeue_thread();
        if (!next) {
            next = &idle_tcb;
        }
        current_thread = next;
        next->time_slice = DEFAULT_QUANTUM;
        if (next != &idle_tcb) {
            thread_set_state(next, THREAD_RUNNING);
        }
        return (void*)(uintptr_t)next->saved_esp;
    }

    // 9. Kasus C: Thread sedang menunggu (WAITING) melalui thread_sleep()
    if (current_thread && current_thread->state == THREAD_WAITING) {
        thread_t *next = dequeue_thread();
        if (!next) {
            next = &idle_tcb;
        }
        current_thread = next;
        next->time_slice = DEFAULT_QUANTUM;
        if (next != &idle_tcb) {
            thread_set_state(next, THREAD_RUNNING);
        }
        return (void*)(uintptr_t)next->saved_esp;
    }

    // 10. Kasus D: Preemptive Round Robin Scheduling
    if (current_thread == &idle_tcb) {
        // Jika sedang di IDLE dan sudah ada thread di antrian ready, segera switch
        thread_t *next = dequeue_thread();
        if (next) {
            current_thread = next;
            next->time_slice = DEFAULT_QUANTUM;
            thread_set_state(next, THREAD_RUNNING);
            return (void*)(uintptr_t)next->saved_esp;
        }
        return frame; // tetap di idle
    }

    if (current_thread && current_thread->state == THREAD_RUNNING) {
        if (current_thread->time_slice > 0) {
            current_thread->time_slice--;
        }

        // Jika jatah waktu (time quantum) habis: PEMOTONGAN PREEMPTIVE
        if (current_thread->time_slice == 0) {
            thread_t *next = dequeue_thread();
            if (next) {
                // Beritahu logger bahwa terjadi pemotongan giliran CPU secara preemptive
                preempt_count++;
                if (preempt_logger) {
                    preempt_logger(current_thread, next);
                }

                thread_set_state(current_thread, THREAD_READY);
                enqueue_thread(current_thread);

                current_thread = next;
                next->time_slice = DEFAULT_QUANTUM;
                thread_set_state(next, THREAD_RUNNING);
                return (void*)(uintptr_t)next->saved_esp;
            } else {
                // Tidak ada thread lain di antrian, perbarui jatah waktu thread ini
                current_thread->time_slice = DEFAULT_QUANTUM;
            }
        }
    }

    // Lanjutkan eksekusi thread yang sama jika belum habis jatah waktu
    return frame;
}

// Menjalankan scheduler sampai semua thread selesai, lalu kembali ke caller (CLI)
void scheduler_run(void) {
    int alive = count_alive_threads();
    if (alive == 0) return;

    init_idle_thread();

    thread_t *first = dequeue_thread();
    if (!first) return;

    current_thread = first;
    first->time_slice = DEFAULT_QUANTUM;
    thread_set_state(first, THREAD_RUNNING);

    scheduler_running = 1;

    // Aktifkan interrupt
    __asm__ __volatile__("sti");

    // Mulai eksekusi thread pertama (akan kembali ke sini saat semua thread TERMINATED)
    scheduler_start(first->saved_esp);

    // Kembali ke CLI
    scheduler_running = 0;
    current_thread = 0;
}

// Melepas giliran CPU secara sukarela: RUNNING -> READY
void yield(void) {
    if (!scheduler_running) return;
    yield_requested = 1;
    __asm__ __volatile__("int $0x20");
}

// Menunggu beberapa tick: RUNNING -> WAITING
void thread_sleep(uint32_t n) {
    if (!scheduler_running) return;
    if (current_thread) {
        current_thread->wake_tick = ticks + n;
        thread_set_state(current_thread, THREAD_WAITING);
    }
    __asm__ __volatile__("int $0x20");
}
