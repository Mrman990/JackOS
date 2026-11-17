#pragma once
// Penanda mode eksekusi: USER dan KERNEL.
// Include SETELAH `console` dan CLR_* di kernel.cpp, SEBELUM ramdisk.h.
//
// Aturan JACK OS:
//   - Shell dan aplikasi stok berjalan di mode USER.
//   - Fungsi internal kernel (ramdisk_*) hanya boleh jalan di mode KERNEL.
//   - Mode USER hanya bisa masuk ke KERNEL lewat gerbang system call (sys_*).

enum CpuMode
{
    MODE_USER = 0,
    MODE_KERNEL = 1
};

static CpuMode g_mode = MODE_USER; // setelah boot, shell berjalan sebagai USER

static const char *modeName(CpuMode m)
{
    return m == MODE_KERNEL ? "KERNEL" : "USER";
}

// Naik ke mode KERNEL. Mengembalikan mode sebelumnya, untuk dipakai kernel_leave.
static CpuMode kernel_enter()
{
    CpuMode prev = g_mode;
    g_mode = MODE_KERNEL;
    return prev;
}

static void kernel_leave(CpuMode prev)
{
    g_mode = prev;
}

// Dipanggil di awal setiap fungsi internal kernel.
// Jika pemanggil masih USER: cetak "akses ditolak" dan return false.
static bool requireKernel(const char *fn)
{
    if (g_mode == MODE_KERNEL)
        return true;
    console.print("akses ditolak: ", CLR_RED);
    console.print(fn, CLR_RED);
    console.print(" hanya boleh dipanggil dari mode KERNEL (sekarang: USER)\n", CLR_RED);
    return false;
}
