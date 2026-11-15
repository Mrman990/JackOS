#pragma once
// Aplikasi Stok Barang (berjalan di mode USER).
// Include SETELAH ramdisk_cmd.h (memakai rdParseLong, rdShowError, rdEq).
//
// ATURAN: aplikasi ini TIDAK BOLEH menyentuh RAMDisk langsung.
// Semua operasi hanya lewat system call: sys_add_item, sys_list_items,
// sys_update_stock, sys_delete_item.

// Baca satu baris dari keyboard (dengan echo dan backspace).
static int stokReadLine(char *buf, int maxLen)
{
    int len = 0;
    while (true)
    {
        char ch = keyboard.getChar();
        if (ch == '\n')
        {
            console.putChar('\n');
            buf[len] = '\0';
            return len;
        }
        else if (ch == '\b')
        {
            if (len > 0)
            {
                len--;
                console.putChar('\b');
            }
        }
        else if (ch >= 32 && ch < 127 && len < maxLen - 1)
        {
            buf[len++] = ch;
            console.setColor(CLR_WHITE);
            console.putChar(ch);
        }
    }
}

static void stokAsk(const char *label, char *buf, int maxLen)
{
    console.print(label, CLR_WHITE);
    stokReadLine(buf, maxLen);
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
        console.print("Stok harus berupa angka.\n", CLR_RED);
        return;
    }
    u32 id = 0;
    RdResult r = sys_add_item(nama, stok, &id);
    if (r != RD_OK)
    {
        rdShowError(r);
        return;
    }
    console.print("Barang ditambahkan: ID ", CLR_WHITE);
    ramdiskPrintNum(id, 0);
    console.print(".\n", CLR_WHITE);
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
        console.print("ID dan stok harus berupa angka.\n", CLR_RED);
        return;
    }
    RdResult r = sys_update_stock((u32)id, stok);
    if (r != RD_OK)
    {
        rdShowError(r);
        return;
    }
    console.print("Stok barang ID ", CLR_WHITE);
    ramdiskPrintNum((u32)id, 0);
    console.print(" diubah menjadi ", CLR_WHITE);
    ramdiskPrintNum((u32)stok, 0);
    console.print(".\n", CLR_WHITE);
}

static void stokMenuHapus()
{
    char idTxt[24], yakin[8];
    sys_list_items();
    stokAsk("ID barang yang dihapus : ", idTxt, sizeof(idTxt));
    long id;
    if (!rdParseLong(idTxt, id) || id <= 0)
    {
        console.print("ID harus berupa angka.\n", CLR_RED);
        return;
    }
    stokAsk("Yakin hapus? (Y/n) : ", yakin, sizeof(yakin));
    if (!(yakin[0] == 'Y' && yakin[1] == '\0'))
    {
        console.print("Dibatalkan.\n", CLR_WHITE);
        return;
    }
    RdResult r = sys_delete_item((u32)id);
    if (r != RD_OK)
    {
        rdShowError(r);
        return;
    }
    console.print("Barang ID ", CLR_WHITE);
    ramdiskPrintNum((u32)id, 0);
    console.print(" dihapus.\n", CLR_WHITE);
}

static void runStokApp()
{
    console.print("=== APLIKASI STOK BARANG (mode ", CLR_RED);
    console.print(modeName(g_mode), CLR_RED);
    console.print(") ===\n", CLR_RED);

    char pilih[8];
    while (true)
    {
        console.print("\n  1. Lihat daftar barang\n", CLR_WHITE);
        console.print("  2. Tambah barang\n", CLR_WHITE);
        console.print("  3. Ubah jumlah stok\n", CLR_WHITE);
        console.print("  4. Hapus barang\n", CLR_WHITE);
        console.print("  0. Keluar\n", CLR_WHITE);
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
            console.print("Keluar dari aplikasi stok.\n", CLR_RED);
            return;
        default:
            console.print("Pilihan tidak dikenal. Ketik 0-4.\n", CLR_RED);
            break;
        }
    }
}