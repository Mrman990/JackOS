@echo off
setlocal
set "PATH=C:\msys64\ucrt64\bin;%PATH%"

if "%1"=="JACK_OS" (
    echo [1/7] Mengompilasi kernel.cpp...
    clang++ --target=i686-elf -ffreestanding -fno-exceptions -fno-rtti -fno-asynchronous-unwind-tables -c kernel/kernel.cpp -o kernel.o
    if errorlevel 1 goto error

    echo [2/7] Mengompilasi interrupt.c...
    clang --target=i686-elf -ffreestanding -fno-asynchronous-unwind-tables -fno-stack-protector -c kernel/interrupt.c -o interrupt.o
    if errorlevel 1 goto error

    echo [3/7] Mengompilasi thread.c...
    clang --target=i686-elf -ffreestanding -fno-asynchronous-unwind-tables -fno-stack-protector -c kernel/process/thread.c -o thread.o
    if errorlevel 1 goto error

    echo [4/7] Mengompilasi scheduler.c...
    clang --target=i686-elf -ffreestanding -fno-asynchronous-unwind-tables -fno-stack-protector -c kernel/process/scheduler.c -o scheduler.o
    if errorlevel 1 goto error

    echo [5/7] Mengompilasi switch.s & loader.s...
    clang --target=i686-elf -c kernel/process/switch.s -o switch.o
    if errorlevel 1 goto error
    clang --target=i686-elf -c loader/loader.s -o loader.o
    if errorlevel 1 goto error

    echo [6/7] Melakukan linking ke kernel.bin...
    ld.lld -m elf_i386 -T linker/linker.ld -o kernel.bin loader.o switch.o interrupt.o kernel.o thread.o scheduler.o
    if errorlevel 1 goto error

    echo [7/7] Menjalankan QEMU...
    qemu-system-i386 -kernel kernel.bin
    goto end
)

echo Perintah tidak dikenali. Gunakan: run.bat JACK_OS
goto end

:error
echo Terjadi error saat kompilasi/linking!
exit /b 1

:end
