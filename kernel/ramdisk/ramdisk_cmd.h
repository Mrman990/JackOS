#pragma once
// Perintah shell untuk RAMDisk: barang, tambah, ubah, hapus.
// Include SETELAH syscall.h. Shell berjalan di mode USER, jadi semua operasi
// lewat gerbang sys_* (bukan ramdisk_* langsung). Di runCommand cukup tambah:
//     else if (ramdiskCommand(cmd)) { }

static bool rdParseLong(const char *s, long &out)
{
    bool neg = false;
    if (*s == '-')
    {
        neg = true;
        s++;
    }
    if (*s == '\0')
        return false;
    long v = 0;
    for (; *s; s++)
    {
        if (*s < '0' || *s > '9')
            return false;
        v = v * 10 + (*s - '0');
        if (v > 100000000L)
            v = 100000000L; // cukup besar agar ditolak validasi
    }
    out = neg ? -v : v;
    return true;
}

static bool rdStarts(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++)
            return false;
    return true;
}

static bool rdEq(const char *a, const char *b)
{
    while (*a && *b)
        if (*a++ != *b++)
            return false;
    return *a == *b;
}

static void rdShowError(RdResult r)
{
    if (r == RD_ACCESS_DENIED)
        return; // alasan sudah dicetak oleh gerbang
    console.print(rdMessage(r), CLR_RED);
    console.print("\n", CLR_RED);
}

// Pecah "a b" jadi dua kata; return false kalau bukan tepat dua kata.
static bool rdTwoWords(const char *args, char *w1, char *w2)
{
    int t = 0, c = 0;
    char *dst[2] = {w1, w2};
    for (const char *p = args;; p++)
    {
        if (*p == ' ' || *p == '\0')
        {
            if (c > 0)
            {
                if (t >= 2)
                    return false;
                dst[t][c] = '\0';
                t++;
                c = 0;
            }
            if (*p == '\0')
                break;
        }
        else
        {
            if (t >= 2 || c >= 15)
                return false;
            dst[t][c++] = *p;
        }
    }
    return t == 2;
}

// tambah <nama> <stok>  (nama boleh berisi spasi; stok = kata terakhir)
static void rdCmdTambah(const char *args)
{
    char buf[128];
    int n = 0;
    while (args[n] && n < 127)
    {
        buf[n] = args[n];
        n++;
    }
    buf[n] = '\0';
    while (n > 0 && buf[n - 1] == ' ')
        buf[--n] = '\0';

    int last = n - 1;
    while (last >= 0 && buf[last] != ' ')
        last--;
    if (last < 0)
    {
        console.print("Pemakaian: tambah <nama> <stok>\n", CLR_RED);
        return;
    }
    long stock;
    if (!rdParseLong(&buf[last + 1], stock))
    {
        console.print("Stok harus berupa angka.\n", CLR_RED);
        return;
    }
    buf[last] = '\0';

    u32 id = 0;
    RdResult r = sys_add_item(buf, stock, &id);
    if (r != RD_OK)
    {
        rdShowError(r);
        return;
    }
    console.print("Barang ditambahkan: ID ", CLR_WHITE);
    ramdiskPrintNum(id, 0);
    console.print(", stok ", CLR_WHITE);
    ramdiskPrintNum((u32)stock, 0);
    console.print(".\n", CLR_WHITE);
}

// ubah <id> <stok>
static void rdCmdUbah(const char *args)
{
    char a[16], b[16];
    long id, stock;
    if (!rdTwoWords(args, a, b) || !rdParseLong(a, id) || !rdParseLong(b, stock) || id < 0)
    {
        console.print("Pemakaian: ubah <id> <stok>\n", CLR_RED);
        return;
    }
    RdResult r = sys_update_stock((u32)id, stock);
    if (r != RD_OK)
    {
        rdShowError(r);
        return;
    }
    console.print("Stok barang ID ", CLR_WHITE);
    ramdiskPrintNum((u32)id, 0);
    console.print(" diubah menjadi ", CLR_WHITE);
    ramdiskPrintNum((u32)stock, 0);
    console.print(".\n", CLR_WHITE);
}

// hapus <id>
static void rdCmdHapus(const char *args)
{
    long id;
    while (*args == ' ')
        args++;
    if (!rdParseLong(args, id) || id < 0)
    {
        console.print("Pemakaian: hapus <id>\n", CLR_RED);
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

// Return true kalau cmd adalah perintah RAMDisk (sudah dijalankan).
static bool ramdiskCommand(const char *cmd)
{
    if (rdEq(cmd, "barang"))
        sys_list_items();
    else if (rdStarts(cmd, "tambah "))
        rdCmdTambah(cmd + 7);
    else if (rdStarts(cmd, "ubah "))
        rdCmdUbah(cmd + 5);
    else if (rdStarts(cmd, "hapus "))
        rdCmdHapus(cmd + 6);
    else if (rdEq(cmd, "tambah") || rdEq(cmd, "ubah") || rdEq(cmd, "hapus"))
        console.print("Argumen kurang. Contoh: tambah Beras 50 | ubah 1 70 | hapus 1\n", CLR_RED);
    else
        return false;
    return true;
}
