.text
.globl timer_isr
.globl scheduler_start
.globl main_stack_ptr
.extern timer_handler

.data
.align 4
main_stack_ptr:
    .long 0

.text
# -------------------------------------------------------------
# timer_isr: Dipanggil oleh hardware saat IRQ0 atau via "int $0x20"
# -------------------------------------------------------------
timer_isr:
    # CPU otomatis push: EFLAGS, CS, EIP
    pushal

    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    # Pastikan data segment menunjuk ke kernel data (0x10)
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # Kirim pointer frame saat ini (%esp) ke timer_handler di C
    pushl %esp
    call timer_handler
    # timer_handler mengembalikan ESP thread yang akan dieksekusi di %eax

    # Ganti stack pointer ke ESP tujuan
    movl %eax, %esp

    # Restore segment dan register umum
    popl %gs
    popl %fs
    popl %es
    popl %ds

    popal
    iret

# -------------------------------------------------------------
# scheduler_start: Memulai eksekusi thread pertama dari shell/main
# Prototype C: void scheduler_start(uint32_t next_esp);
# -------------------------------------------------------------
scheduler_start:
    # Ambil next_esp dari 4(%esp)
    movl 4(%esp), %edx

    # Ambil alamat kembali caller dari (%esp)
    popl %ecx

    # Susun stack frame seolah-olah terjadi interrupt:
    pushfl              # EFLAGS
    pushl %cs           # CS
    pushl %ecx          # EIP (alamat kembali)

    pushal              # EDI, ESI, EBP, ESP, EBX, EDX, ECX, EAX

    pushl %ds           # Segment registers
    pushl %es
    pushl %fs
    pushl %gs

    # Simpan stack pointer main kernel ke main_stack_ptr
    movl %esp, main_stack_ptr

    # Pindah stack pointer ke thread pertama
    movl %edx, %esp

    # Restore context thread pertama dan jalankan dengan iret
    popl %gs
    popl %fs
    popl %es
    popl %ds

    popal
    iret
