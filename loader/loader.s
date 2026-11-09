.set MAGIC, 0x1badb002
.set FLAGS, (1<<0 | 1<<1)
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
    .long MAGIC
    .long FLAGS
    .long CHECKSUM

.section .text
.global loader
.extern kmain

loader:
    mov $stack_top, %esp
    call kmain

_halt:
    cli
    hlt
    jmp _halt

.section .bss
.space 2 * 1024 * 1024 # 2 MB stack
stack_top: