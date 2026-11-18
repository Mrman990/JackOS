#pragma once
// RAMDisk: tabel barang di RAM.
// Berisi struktur data, aturan validasi, dan 4 fungsi kernel:
//   ramdisk_add, ramdisk_list, ramdisk_update, ramdisk_delete
// Include SETELAH typedef u8/u16/u32, CLR_*, `console`, dan protection.h.
// Fungsi ramdisk_* adalah fungsi INTERNAL KERNEL: ditolak jika dipanggil dari mode USER.
// Aplikasi/shell harus memakai sys_* (syscall.h).

#define RD_MAX_ITEMS 64
#define RD_NAME_LEN 20 // 19 huruf + penutup '\0'
#define RD_MAX_STOCK 1000000

// Satu catatan barang: tepat 32 byte, jadi 16 catatan muat dalam 1 sektor disk (512 byte).
struct RdItem
{
    u32 id;
    char name[RD_NAME_LEN];
    u32 stock;
    u32 used; // 1 = slot berisi barang, 0 = kosong
};
static_assert(sizeof(RdItem) == 32, "RdItem harus 32 byte");

enum RdResult
{
    RD_OK = 0,
    RD_FULL,
    RD_NAME_EMPTY,
    RD_NAME_TOO_LONG,
    RD_NAME_DUPLICATE,
    RD_STOCK_NEGATIVE,
    RD_STOCK_TOO_BIG,
    RD_NOT_FOUND,
    RD_ACCESS_DENIED, // ditolak gerbang/proteksi (pesan sudah dicetak)
    RD_BAD_ARG        // argumen system call tidak sah
};

static const char *rdMessage(RdResult r)
{
    switch (r)
    {
    case RD_OK:
        return "OK";
    case RD_FULL:
        return "Penyimpanan penuh (maksimal 64 barang).";
    case RD_NAME_EMPTY:
        return "Nama tidak boleh kosong.";
    case RD_NAME_TOO_LONG:
        return "Nama terlalu panjang (maksimal 19 karakter).";
    case RD_NAME_DUPLICATE:
        return "Nama barang sudah ada.";
    case RD_STOCK_NEGATIVE:
        return "Stok tidak boleh negatif.";
    case RD_STOCK_TOO_BIG:
        return "Stok terlalu besar (maksimal 1000000).";
    case RD_NOT_FOUND:
        return "ID barang tidak ditemukan.";
    case RD_ACCESS_DENIED:
        return "Akses ditolak.";
    case RD_BAD_ARG:
        return "Argumen tidak sah.";
    }
    return "Kesalahan tidak dikenal.";
}

struct RamDisk
{
    RdItem items[RD_MAX_ITEMS];
    u32 nextId;
    bool dirty; // true = ada perubahan yang belum disimpan ke flashdisk

    // Dipanggil sekali di kmain. (Tidak memakai constructor, karena constructor
    // global belum tentu dijalankan oleh loader.)
    void init()
    {
        reset();
    }

    void reset()
    {
        for (int i = 0; i < RD_MAX_ITEMS; i++)
        {
            items[i].id = 0;
            for (int c = 0; c < RD_NAME_LEN; c++)
                items[i].name[c] = '\0';
            items[i].stock = 0;
            items[i].used = 0;
        }
        nextId = 1;
        dirty = false;
    }

    u32 count() const
    {
        u32 n = 0;
        for (int i = 0; i < RD_MAX_ITEMS; i++)
            if (items[i].used)
                n++;
        return n;
    }

    RdItem *find(u32 id)
    {
        if (id == 0)
            return nullptr;
        for (int i = 0; i < RD_MAX_ITEMS; i++)
            if (items[i].used && items[i].id == id)
                return &items[i];
        return nullptr;
    }

    RdResult add(const char *rawName, long stock, u32 *newId = nullptr)
    {
        // Buang spasi di awal dan akhir nama.
        while (*rawName == ' ')
            rawName++;
        int len = 0;
        while (rawName[len] != '\0')
            len++;
        while (len > 0 && rawName[len - 1] == ' ')
            len--;

        if (len == 0)
            return RD_NAME_EMPTY;
        if (len > RD_NAME_LEN - 1)
            return RD_NAME_TOO_LONG;
        if (stock < 0)
            return RD_STOCK_NEGATIVE;
        if (stock > RD_MAX_STOCK)
            return RD_STOCK_TOO_BIG;

        for (int i = 0; i < RD_MAX_ITEMS; i++)
        {
            if (!items[i].used)
                continue;
            bool same = true;
            for (int c = 0; c < len; c++)
                if (items[i].name[c] != rawName[c])
                {
                    same = false;
                    break;
                }
            if (same && items[i].name[len] == '\0')
                return RD_NAME_DUPLICATE;
        }

        for (int i = 0; i < RD_MAX_ITEMS; i++)
        {
            if (items[i].used)
                continue;
            // Slot selalu rapat (tanpa lubang), jadi slot kosong pertama = barang ke-(jumlah+1).
            items[i].id = (u32)(i + 1);
            nextId = (u32)(i + 2);
            for (int c = 0; c < RD_NAME_LEN; c++)
                items[i].name[c] = '\0';
            for (int c = 0; c < len; c++)
                items[i].name[c] = rawName[c];
            items[i].stock = (u32)stock;
            items[i].used = 1;
            dirty = true;
            if (newId)
                *newId = items[i].id;
            return RD_OK;
        }
        return RD_FULL;
    }

    RdResult update(u32 id, long stock)
    {
        RdItem *it = find(id);
        if (!it)
            return RD_NOT_FOUND;
        if (stock < 0)
            return RD_STOCK_NEGATIVE;
        if (stock > RD_MAX_STOCK)
            return RD_STOCK_TOO_BIG;
        it->stock = (u32)stock;
        dirty = true;
        return RD_OK;
    }

    // Hapus barang, lalu rapatkan: barang di bawahnya naik satu slot dan ID-nya turun satu,
    // sehingga ID selalu berurutan 1..N tanpa lompatan (ID = nomor slot + 1).
    RdResult remove(u32 id)
    {
        RdItem *it = find(id);
        if (!it)
            return RD_NOT_FOUND;
        int idx = (int)(it - items);
        // Geser per field (tanpa menyalin struct utuh agar tidak memanggil memcpy).
        for (int j = idx; j < RD_MAX_ITEMS - 1; j++)
        {
            items[j].used = items[j + 1].used;
            items[j].id = items[j + 1].used ? (u32)(j + 1) : 0;
            items[j].stock = items[j + 1].stock;
            for (int c = 0; c < RD_NAME_LEN; c++)
                items[j].name[c] = items[j + 1].name[c];
        }
        // Slot paling akhir dikosongkan.
        RdItem &last = items[RD_MAX_ITEMS - 1];
        last.used = 0;
        last.id = 0;
        last.stock = 0;
        for (int c = 0; c < RD_NAME_LEN; c++)
            last.name[c] = '\0';
        nextId = count() + 1;
        dirty = true;
        return RD_OK;
    }
};

static RamDisk ramdisk;


static RdResult ramdisk_add(const char *name, long stock, u32 *newId = nullptr)
{
    if (!requireKernel("ramdisk_add"))
        return RD_ACCESS_DENIED;
    return ramdisk.add(name, stock, newId);
}

static RdResult ramdisk_update(u32 id, long stock)
{
    if (!requireKernel("ramdisk_update"))
        return RD_ACCESS_DENIED;
    return ramdisk.update(id, stock);
}

static RdResult ramdisk_delete(u32 id)
{
    if (!requireKernel("ramdisk_delete"))
        return RD_ACCESS_DENIED;
    return ramdisk.remove(id);
}

static void ramdiskPrintNum(u32 v, int width)
{
    char buf[12];
    int i = 11;
    buf[i] = '\0';
    if (v == 0)
        buf[--i] = '0';
    while (v > 0)
    {
        buf[--i] = (char)('0' + v % 10);
        v /= 10;
    }
    int digits = 11 - i;
    console.print(&buf[i], CLR_WHITE);
    for (int k = digits; k < width; k++)
        console.print(" ", CLR_WHITE);
}

// Tampilkan semua barang dalam bentuk tabel.
static void ramdisk_list()
{
    if (!requireKernel("ramdisk_list"))
        return;
    console.print("ID   NAMA                 STOK\n", CLR_RED);
    for (int i = 0; i < RD_MAX_ITEMS; i++)
    {
        const RdItem &it = ramdisk.items[i];
        if (!it.used)
            continue;
        ramdiskPrintNum(it.id, 5);
        console.print(it.name, CLR_WHITE);
        int len = 0;
        while (it.name[len])
            len++;
        for (int c = len; c < 21; c++)
            console.print(" ", CLR_WHITE);
        ramdiskPrintNum(it.stock, 0);
        console.print("\n", CLR_WHITE);
    }
    console.print("Total: ", CLR_WHITE);
    ramdiskPrintNum(ramdisk.count(), 0);
    console.print(" barang\n", CLR_WHITE);
}
