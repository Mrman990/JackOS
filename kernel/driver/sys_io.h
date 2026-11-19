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

// System call untuk mengatur posisi kursor.
static bool sys_set_cursor(int row, int col)
{
    CpuMode prev = kernel_enter();
    bool ok = drv_screen_set_cursor(row, col);
    kernel_leave(prev);

    return ok;
}

// Menampilkan teks pada posisi tertentu melalui system call.
static bool sys_write_at(
    int row, int col,
    const char *str,
    u8 clr)
{
    if (str == nullptr)
        return false;

    CpuMode prev = kernel_enter();
    bool ok = drv_screen_write_at(row, col, str, clr);
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


static bool sys_try_getchar(char *out)
{
    if (out == nullptr)
        return false;

    CpuMode prev = kernel_enter();

    bool available = drv_keyboard_try_getchar(out);

    kernel_leave(prev);

    return available;
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

// Demo perintah `io`: driver langsung dari USER ditolak, lewat syscall diterima.
static void ioDemo()
{
    sys_print("=== DEMO PERANGKAT (layar & keyboard) ===\n", CLR_RED);
    sys_print("Mode sekarang: ", CLR_WHITE);
    sys_print(modeName(g_mode), CLR_WHITE);
    sys_print("\n1) Panggil drv_screen_print() langsung dari USER:\n", CLR_WHITE);
    bool direct = drv_screen_print("(teks ini tidak boleh tampil)\n", CLR_WHITE);
    sys_print(direct ? "  hasil: LOLOS (salah!)\n" : "  hasil: ditolak: OK\n", direct ? CLR_RED : CLR_WHITE);
    sys_print("2) Lewat sys_print() dan sys_read_line():\n", CLR_WHITE);
    sys_print("  ketik sesuatu: ", CLR_WHITE);
    char buf[40];
    int n = sys_read_line(buf, sizeof(buf));
    sys_print("  kamu mengetik ", CLR_WHITE);
    sys_print_num((u32)(n < 0 ? 0 : n), CLR_WHITE);
    sys_print(" karakter: ", CLR_WHITE);
    sys_print(n < 0 ? "" : buf, CLR_WHITE);
    sys_print("\n=== DEMO SELESAI ===\n", CLR_RED);
}