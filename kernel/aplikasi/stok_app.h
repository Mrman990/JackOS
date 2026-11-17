#pragma once
// Aplikasi Stok Barang (berjalan di mode USER).
// Include SETELAH ramdisk_cmd.h (memakai rdParseLong) dan sys_io.h.

static void stokShowError(RdResult r)
{
    if (r == RD_ACCESS_DENIED)
        return; // alasan sudah dicetak oleh gerbang
    sys_print(rdMessage(r), CLR_RED);
    sys_print("\n", CLR_RED);
}

static void stokAsk(const char *label, char *buf, int maxLen)
{
    sys_print(label, CLR_WHITE);
    sys_read_line(buf, maxLen);
}

static void stokMenuLihat()
{
    sys_list_items();
}

static void stokMenuTambah()
{
    char nama[40], stokTxt[24];
    stokAsk("Nama barang : ", nama, sizeof(nama));
    stokAsk("Jumlah stok : ", stokTxt, sizeof(stokTxt));
    long stok;
    if (!rdParseLong(stokTxt, stok))
    {
        sys_print("Stok harus berupa angka.\n", CLR_RED);
        return;
    }
    u32 id = 0;
    RdResult r = sys_add_item(nama, stok, &id);
    if (r != RD_OK)
    {
        stokShowError(r);
        return;
    }

    // Catat perubahan TAMBAH yang berhasil ke buku log
    invLogger.recordChange("TAMBAH", (int)id, nama, 0, (int)stok);

    sys_print("Barang ditambahkan: ID ", CLR_WHITE);
    sys_print_num(id, CLR_WHITE);
    sys_print(".\n", CLR_WHITE);
}

static void stokMenuUbah()
{
    char idTxt[24], stokTxt[24];
    sys_list_items();
    stokAsk("ID barang        : ", idTxt, sizeof(idTxt));
    stokAsk("Jumlah stok baru : ", stokTxt, sizeof(stokTxt));
    long id, stok;
    if (!rdParseLong(idTxt, id) || id <= 0 || !rdParseLong(stokTxt, stok))
    {
        sys_print("ID dan stok harus berupa angka.\n", CLR_RED);
        return;
    }
    RdResult r = sys_update_stock((u32)id, stok);
    if (r != RD_OK)
    {
        stokShowError(r);
        return;
    }

    // Catat perubahan UBAH yang berhasil ke buku log
    invLogger.recordChange("UBAH", (int)id, "Barang", 0, (int)stok);

    sys_print("Stok barang ID ", CLR_WHITE);
    sys_print_num((u32)id, CLR_WHITE);
    sys_print(" diubah menjadi ", CLR_WHITE);
    sys_print_num((u32)stok, CLR_WHITE);
    sys_print(".\n", CLR_WHITE);
}

static void stokMenuHapus()
{
    char idTxt[24], yakin[8];
    sys_list_items();
    stokAsk("ID barang yang dihapus : ", idTxt, sizeof(idTxt));
    long id;
    if (!rdParseLong(idTxt, id) || id <= 0)
    {
        sys_print("ID harus berupa angka.\n", CLR_RED);
        return;
    }
    stokAsk("Yakin hapus? (Y/n) : ", yakin, sizeof(yakin));
    if (!(yakin[0] == 'Y' && yakin[1] == '\0'))
    {
        sys_print("Dibatalkan.\n", CLR_WHITE);
        return;
    }
    RdResult r = sys_delete_item((u32)id);
    if (r != RD_OK)
    {
        stokShowError(r);
        return;
    }

    // Catat perubahan HAPUS yang berhasil ke buku log
    invLogger.recordChange("HAPUS", (int)id, "Barang", 0, 0);

    sys_print("Barang ID ", CLR_WHITE);
    sys_print_num((u32)id, CLR_WHITE);
    sys_print(" dihapus.\n", CLR_WHITE);
}

static void runStokApp()
{
    // Buka sesi log saat masuk menu inventory
    invLogger.openSession();

    sys_print("=== MANAJEMEN INVENTORY (mode ", CLR_RED);
    sys_print(modeName(g_mode), CLR_RED);
    sys_print(") ===\n", CLR_RED);

    char pilih[8];
    while (true)
    {
        sys_print("\n  1. Lihat daftar barang\n", CLR_WHITE);
        sys_print("  2. Tambah barang\n", CLR_WHITE);
        sys_print("  3. Ubah jumlah stok\n", CLR_WHITE);
        sys_print("  4. Hapus barang\n", CLR_WHITE);
        sys_print("  0. Keluar\n", CLR_WHITE);
        stokAsk("Pilih> ", pilih, sizeof(pilih));

        if (pilih[1] != '\0')
            pilih[0] = '?';
        switch (pilih[0])
        {
        case '1':
            stokMenuLihat();
            break;
        case '2':
            stokMenuTambah();
            break;
        case '3':
            stokMenuUbah();
            break;
        case '4':
            stokMenuHapus();
            break;
        case '0':
            // Tutup sesi log saat keluar dari aplikasi
            invLogger.closeSession();
            sys_print("Keluar dari aplikasi stok.\n", CLR_RED);
            return;
        default:
            sys_print("Pilihan tidak dikenal. Ketik 0-4.\n", CLR_RED);
            break;
        }
    }
}