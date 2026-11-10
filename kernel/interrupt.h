#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Struktur register yang disimpan di stack saat interrupt terjadi.
// Urutan ini cocok dengan: pushl gs, fs, es, ds; pushal; CPU pushed eip, cs, eflags.
typedef struct {
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp; // Nilai esp saat pushal (diabaikan oleh popal)
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;

    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} __attribute__((packed)) interrupt_frame_t;

void gdt_init(void);
void idt_init(void);
void pic_init(void);
void pit_init(uint32_t freq_hz);
void interrupts_init(void);

uint32_t get_system_ticks(void);

// Assembly ISR untuk timer IRQ0
void timer_isr(void);

#ifdef __cplusplus
}
#endif

#endif // INTERRUPT_H
