#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef STACK_SIZE
#define STACK_SIZE 2048 // stack per thread (byte)
#endif
#define MAX_THREADS 10

    // Status thread (State Transition)
    typedef enum
    {
        THREAD_NEW,
        THREAD_READY,
        THREAD_RUNNING,
        THREAD_WAITING,
        THREAD_TERMINATED
    } thread_state_t;

    // Register CPU (x86). Urutan field dipakai langsung oleh context_switch (assembly).
    typedef struct
    {
        uint32_t esp; // offset 0  : Stack Pointer
        uint32_t ebx; // offset 4
        uint32_t esi; // offset 8
        uint32_t edi; // offset 12
        uint32_t ebp; // offset 16
        uint32_t eip; // offset 20 : Instruction Pointer
    } cpu_context_t;

    // Thread Control Block (TCB)
    typedef struct thread
    {
        uint32_t tid;              // ID unik thread
        const char *name;          // Nama thread
        thread_state_t state;      // Status thread saat ini
        uint8_t stack[STACK_SIZE]; // Stack lokal milik thread
        cpu_context_t context;     // Register CPU untuk context switch
        struct thread *next;       // Penghubung di Ready Queue
        void (*entry)(void);       // Fungsi tugas thread
        uint32_t wake_tick;        // Kapan thread WAITING dibangunkan
        int in_use;                // 1 jika slot dipakai
    } thread_t;

    // Dipanggil setiap status thread berubah (untuk ditampilkan oleh kernel).
    typedef void (*thread_log_fn)(const thread_t *t, thread_state_t from, thread_state_t to);

    // thread.c
    thread_t *create_thread(const char *name, void (*entry_point)(void));
    void thread_exit(void);
    int thread_count(void);
    thread_t *thread_at(int index);
    const char *thread_state_name(thread_state_t s);

    // scheduler.c
    void scheduler_init(void);
    void scheduler_set_logger(thread_log_fn fn);
    void scheduler_add(thread_t *t);                      // NEW -> READY
    void thread_set_state(thread_t *t, thread_state_t s); // ubah status + catat
    void scheduler_run(void);                             // jalankan sampai semua thread selesai
    void yield(void);                                     // RUNNING -> READY
    void thread_sleep(uint32_t ticks);                    // RUNNING -> WAITING
    thread_t *get_current_thread(void);
    uint32_t scheduler_ticks(void);

#ifdef __cplusplus
}
#endif

#endif // PROCESS_H