
#pragma once

// Gerbang system call: satu-satunya jalan resmi mode USER
// untuk mengakses RAMDisk dan penyimpanan permanen.
//
// Include SETELAH:
// 1. protection.h
// 2. ramdisk.h
// 3. logbook.h
// 4. storage.h

#define PERM_ADD 1
#define PERM_LIST 2
#define PERM_UPDATE 4
#define PERM_DELETE 8
#define PERM_SAVE 16

#define PERM_ALL 31

static u32 userPerms = PERM_ALL;

// Memeriksa izin pemanggil.
static bool sysPermit(u32 bit, const char *op)
{
    if (g_mode == MODE_KERNEL || (userPerms & bit))
        return true;

    console.print("akses ditolak: izin '", CLR_RED);
    console.print(op, CLR_RED);
    console.print("' tidak dimiliki mode USER\n", CLR_RED);

    return false;
}

// Menyalin nama barang dengan batas panjang.
static void sysCopyName(char *dst, const char *src)
{
    int i = 0;

    while (src[i] != '\0' && i < RD_NAME_LEN - 1)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

// ============================================================
// TAMBAH BARANG
// ============================================================

static RdResult sys_add_item(
    const char *name,
    long stock,
    u32 *newId = nullptr)
{
    if (!sysPermit(PERM_ADD, "tambah"))
        return RD_ACCESS_DENIED;

    if (name == nullptr)
        return RD_BAD_ARG;

    u32 id = 0;

    CpuMode prev = kernel_enter();
    RdResult r = ramdisk_add(name, stock, &id);
    kernel_leave(prev);

    if (r == RD_OK)
    {
        if (newId)
            *newId = id;

        RdItem *it = ramdisk.find(id);

        logbook_record(
            LOG_TAMBAH,
            id,
            it ? it->name : name,
            0,
            (u32)stock);
    }

    return r;
}

// ============================================================
// LIHAT DAFTAR BARANG
// ============================================================

static RdResult sys_list_items()
{
    if (!sysPermit(PERM_LIST, "barang"))
        return RD_ACCESS_DENIED;

    CpuMode prev = kernel_enter();
    ramdisk_list();
    kernel_leave(prev);

    return RD_OK;
}

// ============================================================
// UBAH STOK BARANG
// ============================================================

static RdResult sys_update_stock(u32 id, long stock)
{
    if (!sysPermit(PERM_UPDATE, "ubah"))
        return RD_ACCESS_DENIED;

    if (id == 0)
        return RD_BAD_ARG;

    char nm[RD_NAME_LEN];
    nm[0] = '\0';

    u32 oldStock = 0;

    RdItem *it = ramdisk.find(id);

    if (it)
    {
        sysCopyName(nm, it->name);
        oldStock = it->stock;
    }

    CpuMode prev = kernel_enter();
    RdResult r = ramdisk_update(id, stock);
    kernel_leave(prev);

    if (r == RD_OK)
    {
        logbook_record(
            LOG_UBAH,
            id,
            nm,
            oldStock,
            (u32)stock);
    }

    return r;
}

// ============================================================
// HAPUS BARANG
// ============================================================

static RdResult sys_delete_item(u32 id)
{
    if (!sysPermit(PERM_DELETE, "hapus"))
        return RD_ACCESS_DENIED;

    if (id == 0)
        return RD_BAD_ARG;

    char nm[RD_NAME_LEN];
    nm[0] = '\0';

    u32 oldStock = 0;

    RdItem *it = ramdisk.find(id);

    if (it)
    {
        sysCopyName(nm, it->name);
        oldStock = it->stock;
    }

    CpuMode prev = kernel_enter();
    RdResult r = ramdisk_delete(id);
    kernel_leave(prev);

    if (r == RD_OK)
    {
        logbook_record(
            LOG_HAPUS,
            id,
            nm,
            oldStock,
            0);
    }

    return r;
}

// ============================================================
// SIMPAN RAMDISK DAN LOGBOOK KE DISK
// ============================================================

static bool sys_save_storage()
{
    if (!sysPermit(PERM_SAVE, "simpan"))
        return false;

    // Naik ke mode KERNEL sebelum mengakses penyimpanan.
    CpuMode prev = kernel_enter();

    // Menyimpan data barang dan LogBook sekaligus.
    storage::Result result = storage::saveSystem();

    // Kembali ke mode sebelumnya.
    kernel_leave(prev);

    return result == storage::STORAGE_OK;
}

// ============================================================
// MANAJEMEN SESI INVENTORY
// ============================================================

static void sys_session_open()
{
    logbook_open();
}

static void sys_session_close()
{
    logbook_close();
}
