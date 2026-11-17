#pragma once
// System call input/output: jalan resmi aplikasi User ke layar dan keyboard.
// Include SETELAH protection.h dan driver.h.
//
// Tiap sys_* memeriksa argumen, naik ke mode KERNEL, memanggil driver,
// lalu kembali ke mode semula.

// Cetak teks ke layar. Return false jika argumen tidak sah.
static bool sys_print(const char *str, u8 clr)
{
    if (str == nullptr)
        return false;
    CpuMode prev = kernel_enter();
    bool ok = drv_screen_print(str, clr);
    kernel_leave(prev);
    return ok;
}

static bool sys_print_num(u32 v, u8 clr)
{
    CpuMode prev = kernel_enter();
    bool ok = drv_screen_print_num(v, clr);
    kernel_leave(prev);
    return ok;
}

// Tunggu satu tombol dari keyboard.
static inline char sys_getchar()
{
    CpuMode prev = kernel_enter();
    char ch = drv_keyboard_getchar();
    kernel_leave(prev);
    return ch;
}

// Baca satu baris ke buf (maksimal maxLen termasuk '\0'). Return panjang, atau -1 jika argumen tidak sah.
static int sys_read_line(char *buf, int maxLen)
{
    if (buf == nullptr || maxLen < 2)
        return -1;
    CpuMode prev = kernel_enter();
    int n = drv_keyboard_readline(buf, maxLen);
    kernel_leave(prev);
    return n;
}
