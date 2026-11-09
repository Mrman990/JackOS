#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define STACK_SIZE 4096 // 4 KB stack per thread

// Enum untuk status thread (State Transition)
typedef enum {
    THREAD_NEW,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
} thread_state_t;

// Context Register CPU (x86 architecture)
typedef struct {
    uint32_t esp; // Stack Pointer
    uint32_t ebx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eip; // Instruction Pointer
} cpu_context_t;

// Thread Control Block (TCB)
typedef struct thread {
    uint32_t tid;                 // ID Unique Thread
    thread_state_t state;         // Status Thread saat ini
    uint8_t stack[STACK_SIZE];    // Stack memori lokal milik thread
    cpu_context_t context;        // Penyimpanan register CPU untuk context switch
    struct thread *next;          // Pointer ke thread berikutnya di Ready Queue
} thread_t;

// Prototype fungsi manajemen thread & scheduler
thread_t* create_thread(void (*entry_point)());
void scheduler_init(void);
void schedule(void);
void yield(void);

#endif // PROCESS_H