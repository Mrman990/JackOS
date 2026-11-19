
#pragma once
// Aplikasi Stok Barang (mode USER).
// Include setelah ramdisk_cmd.h dan sys_io.h.

static bool stokAppAktif = false;
static int stokRow = 2;
static int stokCol = 2;

// ================= OUTPUT PANEL KIRI =================

static void stokPrint(const char *str, u8 clr = CLR_WHITE)
{
    if (str == nullptr)
        return;

    for (int i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            stokRow++;
            stokCol = 2;
            continue;
        }

        if (stokCol >= 39)
        {
            stokRow++;
            stokCol = 2;
        }

        if (stokRow >= 25)
            return;

        char temp[2] = {str[i], '\0'};
        sys_write_at(stokRow, stokCol, temp, clr);
        stokCol++;
    }
}

static void stokPrintNumber(u32 number, u8 clr)
{
    char digits[12];
    int count = 0;

    if (number == 0)
    {
        stokPrint("0", clr);
        return;
    }

    while (number > 0 && count < 12)
    {
        digits[count++] = '0' + (number % 10);
        number /= 10;
    }

    while (count > 0)
    {
        char temp[2] = {digits[--count], '\0'};
        stokPrint(temp, clr);
    }
}

// ================= INPUT KEYBOARD =================

static void stokAsk(const char *label, char *buf, int maxLen)
{
    stokPrint(label, CLR_WHITE);

    if (buf == nullptr || maxLen <= 0)
        return;

    int len = 0;
    buf[0] = '\0';

    while (true)
    {
        char ch;

        if (!sys_try_getchar(&ch))
        {
            yield();
            continue;
        }

        if (ch == '\n')
        {
            buf[len] = '\0';
            stokPrint("\n", CLR_WHITE);
            return;
        }

        if (ch == '\b')
        {
            if (len > 0)
            {
                len--;
                buf[len] = '\0';

                if (stokCol > 2)
                    stokCol--;
                else if (stokRow > 2)
                {
                    stokRow--;
                    stokCol = 38;
                }

                sys_write_at(stokRow, stokCol, " ", CLR_WHITE);
            }
            continue;
        }

        if (ch >= 32 && ch < 127 && len < maxLen - 1)
        {
            buf[len++] = ch;
            buf[len] = '\0';

            char temp[2] = {ch, '\0'};
            stokPrint(temp, CLR_WHITE);
        }
    }
}

// ================= PESAN ERROR =================

static void stokShowError(RdResult r)
{
    if (r == RD_ACCESS_DENIED)
        return;

    stokPrint(rdMessage(r), CLR_RED);
    stokPrint("\n", CLR_RED);
}

// ================= CRUD =================

static void stokMenuLihat()
{
    sys_list_items_panel();
}

static void stokMenuTambah()
{
    char nama[40], stokTxt[24];

    stokAsk("Nama barang : ", nama, sizeof(nama));
    stokAsk("Jumlah stok : ", stokTxt, sizeof(stokTxt));

    long stok;

    if (!rdParseLong(stokTxt, stok))
    {
        stokPrint("Stok harus berupa angka.\n", CLR_RED);
        return;
    }

    u32 id = 0;
    RdResult r = sys_add_item(nama, stok, &id);

    if (r != RD_OK)
    {
        stokShowError(r);
        return;
    }

    stokPrint("Barang ditambahkan: ID ", CLR_WHITE);
    stokPrintNumber(id, CLR_WHITE);
    stokPrint(".\n", CLR_WHITE);
}

static void stokMenuUbah()
{
    char idTxt[24], stokTxt[24];

    sys_list_items_panel();

    stokAsk("ID barang        : ", idTxt, sizeof(idTxt));
    stokAsk("Jumlah stok baru : ", stokTxt, sizeof(stokTxt));

    long id, stok;

    if (!rdParseLong(idTxt, id) || id <= 0 ||
        !rdParseLong(stokTxt, stok))
    {
        stokPrint("ID dan stok harus berupa angka.\n", CLR_RED);
        return;
    }

    RdResult r = sys_update_stock((u32)id, stok);

    if (r != RD_OK)
    {
        stokShowError(r);
        return;
    }

    stokPrint("Stok barang ID ", CLR_WHITE);
    stokPrintNumber((u32)id, CLR_WHITE);
    stokPrint(" diubah menjadi ", CLR_WHITE);
    stokPrintNumber((u32)stok, CLR_WHITE);
    stokPrint(".\n", CLR_WHITE);
}

static void stokMenuHapus()
{
    char idTxt[24], yakin[8];

    sys_list_items_panel();

    stokAsk("ID barang yang dihapus : ", idTxt, sizeof(idTxt));

    long id;

    if (!rdParseLong(idTxt, id) || id <= 0)
    {
        stokPrint("ID harus berupa angka.\n", CLR_RED);
        return;
    }

    stokAsk("Yakin hapus? (Y/n) : ", yakin, sizeof(yakin));

    if (!(yakin[0] == 'Y' && yakin[1] == '\0'))
    {
        stokPrint("Dibatalkan.\n", CLR_WHITE);
        return;
    }

    RdResult r = sys_delete_item((u32)id);

    if (r != RD_OK)
    {
        stokShowError(r);
        return;
    }

    stokPrint("Barang ID ", CLR_WHITE);
    stokPrintNumber((u32)id, CLR_WHITE);
    stokPrint(" dihapus.\n", CLR_WHITE);
}

// ================= MENU APLIKASI =================

static void runStokApp()
{
    stokRow = 2;
    stokCol = 2;

    stokPrint("=== APLIKASI STOK BARANG ===\n", CLR_RED);

    char pilih[8];

    while (true)
    {
        // Bersihkan area menu dan input.
        char kosong[40];
        for (int i = 0; i < 39; i++)
            kosong[i] = ' ';
        kosong[39] = '\0';

        for (int r = 14; r < 25; r++)
            sys_write_at(r, 0, kosong, CLR_WHITE);

        // Kembalikan posisi menu ke area tetap.
        stokRow = 14;
        stokCol = 2;

        stokPrint("1. Lihat daftar barang\n", CLR_WHITE);
        stokPrint("2. Tambah barang\n", CLR_WHITE);
        stokPrint("3. Ubah jumlah stok\n", CLR_WHITE);
        stokPrint("4. Hapus barang\n", CLR_WHITE);
        stokPrint("0. Keluar\n", CLR_WHITE);

        stokPrint("Pilih> ", CLR_WHITE);
        stokAsk("", pilih, sizeof(pilih));

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
            stokPrint("Keluar dari aplikasi stok.\n", CLR_RED);
            return;
        default:
            stokPrint("Pilihan tidak dikenal.\n", CLR_RED);
            break;
        }
    }
}


// ================= THREAD APLIKASI =================

static void taskStokApp()
{
    stokAppAktif = true;
    runStokApp();
    stokAppAktif = false;
}
