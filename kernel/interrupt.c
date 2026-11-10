#include "interrupt.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t r;
    __asm__ __volatile__("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

// ==========================================
// GDT (Global Descriptor Table)
// ==========================================
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[3];
static struct gdt_ptr gp;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;

    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].access = access;
}

void gdt_init(void) {
    gp.limit = (sizeof(struct gdt_entry) * 3) - 1;
    gp.base = (uint32_t)(uintptr_t)&gdt;

    // 0: Null Descriptor
    gdt_set_gate(0, 0, 0, 0, 0);

    // 1: Kernel Code Segment (0x08): Base 0, Limit 4GB, 4KB gran, 32-bit, Ring 0, Read/Exec
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    // 2: Kernel Data Segment (0x10): Base 0, Limit 4GB, 4KB gran, 32-bit, Ring 0, Read/Write
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    __asm__ __volatile__(
        "lgdt (%0)\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "ljmp $0x08, $1f\n\t"
        "1:\n\t"
        :
        : "r"(&gp)
        : "eax", "memory"
    );
}

// ==========================================
// IDT (Interrupt Descriptor Table)
// ==========================================
struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_ptr ip;

static void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = (base & 0xFFFF);
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

// Default ISR handler jika ada interrupt tak terduga
extern void default_isr(void);
__asm__(
    ".text\n"
    ".globl default_isr\n"
    "default_isr:\n"
    "    iret\n"
);

void idt_init(void) {
    ip.limit = (sizeof(struct idt_entry) * 256) - 1;
    ip.base = (uint32_t)(uintptr_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate((uint8_t)i, (uint32_t)(uintptr_t)default_isr, 0x08, 0x8E);
    }

    // IRQ0 -> Interrupt 32 (0x20): Timer PIT
    idt_set_gate(32, (uint32_t)(uintptr_t)timer_isr, 0x08, 0x8E);

    __asm__ __volatile__("lidt (%0)" : : "r"(&ip));
}

// ==========================================
// PIC (8259A)
// ==========================================
void pic_init(void) {
    // ICW1: Memulai inisialisasi pada master & slave
    outb(0x20, 0x11);
    io_wait();
    outb(0xA0, 0x11);
    io_wait();

    // ICW2: Remap offset: Master -> 0x20 (32), Slave -> 0x28 (40)
    outb(0x21, 0x20);
    io_wait();
    outb(0xA1, 0x28);
    io_wait();

    // ICW3: Cascading: Master terhubung slave di IRQ2, Slave identity 2
    outb(0x21, 0x04);
    io_wait();
    outb(0xA1, 0x02);
    io_wait();

    // ICW4: Mode 8086/88
    outb(0x21, 0x01);
    io_wait();
    outb(0xA1, 0x01);
    io_wait();

    // Mask IRQ: Hanya aktifkan IRQ0 (Timer, bit 0 = 0)
    // IRQ1 (Keyboard) dibiarkan masked karena input keyboard dipolling pada port 0x64/0x60
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF);
}

// ==========================================
// PIT (8254 Programmable Interval Timer)
// ==========================================
void pit_init(uint32_t freq_hz) {
    if (freq_hz == 0) freq_hz = 50;
    uint32_t divisor = 1193182 / freq_hz;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;

    // Port 0x43: Channel 0, Lobyle/Hibyte, Mode 3 (Square wave generator)
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void interrupts_init(void) {
    gdt_init();
    pic_init();
    idt_init();
    pit_init(50); // 50 Hz = 20ms per tick
}
