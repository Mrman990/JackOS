#pragma once
// Gerbang system call: satu-satunya jalan resmi mode USER ke RAMDisk.
// Include SETELAH protection.h dan ramdisk.h.
//
// Tiap sys_* melakukan 4 langkah:
//   1. periksa IZIN pemanggil,
//   2. periksa DATA (pointer kosong, ID 0),
//   3. naik ke mode KERNEL,
//   4. panggil fungsi RAMDisk, lalu kembali ke mode semula.
// Aturan isi data (nama kosong, stok negatif, penuh, dst.) tetap diperiksa RAMDisk.

#define PERM_ADD 1
#define PERM_LIST 2
#define PERM_UPDATE 4
#define PERM_DELETE 8
#define PERM_ALL 15

static u32 userPerms = PERM_ALL; // izin yang dimiliki mode USER

static bool sysPermit(u32 bit, const char *op)
{
    if (g_mode == MODE_KERNEL || (userPerms & bit))
        return true;
    console.print("akses ditolak: izin '", CLR_RED);
    console.print(op, CLR_RED);
    console.print("' tidak dimiliki mode USER\n", CLR_RED);
    return false;
}

static RdResult sys_add_item(const char *name, long stock, u32 *newId = nullptr)
{
    if (!sysPermit(PERM_ADD, "tambah"))
        return RD_ACCESS_DENIED;
    if (name == nullptr)
        return RD_BAD_ARG;
    CpuMode prev = kernel_enter();
    RdResult r = ramdisk_add(name, stock, newId);
    kernel_leave(prev);
    return r;
}

static RdResult sys_list_items()
{
    if (!sysPermit(PERM_LIST, "barang"))
        return RD_ACCESS_DENIED;
    CpuMode prev = kernel_enter();
    ramdisk_list();
    kernel_leave(prev);
    return RD_OK;
}

static RdResult sys_update_stock(u32 id, long stock)
{
    if (!sysPermit(PERM_UPDATE, "ubah"))
        return RD_ACCESS_DENIED;
    if (id == 0)
        return RD_BAD_ARG;
    CpuMode prev = kernel_enter();
    RdResult r = ramdisk_update(id, stock);
    kernel_leave(prev);
    return r;
}

static RdResult sys_delete_item(u32 id)
{
    if (!sysPermit(PERM_DELETE, "hapus"))
        return RD_ACCESS_DENIED;
    if (id == 0)
        return RD_BAD_ARG;
    CpuMode prev = kernel_enter();
    RdResult r = ramdisk_delete(id);
    kernel_leave(prev);
    return r;
}
