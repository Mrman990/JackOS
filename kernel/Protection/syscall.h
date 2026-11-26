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
#define PERM_SAVE 16
#define PERM_ALL 31

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

// Menampilkan daftar barang khusus pada panel aplikasi.
// Menampilkan daftar barang khusus pada panel aplikasi.
// Membuat angka menjadi teks dengan lebar minimal tertentu.
static void sysFormatItemNumber(u32 value, int minWidth, char *out)
{
    char temp[12];
    int count = 0;

    do
    {
        temp[count++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0 && count < 11);

    while (count < minWidth)
        temp[count++] = '0';

    int pos = 0;

    while (count > 0)
        out[pos++] = temp[--count];

    out[pos] = '\0';
}

static RdResult sys_list_items_panel()
{
    if (!sysPermit(PERM_LIST, "barang"))
        return RD_ACCESS_DENIED;

    CpuMode prev = kernel_enter();

    // Bersihkan area daftar sebelumnya.
    const char *blank = "                                     ";

    for (int r = 3; r < 13; r++)
        console.writeAt(r, 2, blank, CLR_WHITE);

    // Header daftar.
    console.writeAt(
        3, 2,
        "NO ID       NAMA             STOK",
        CLR_RED);

    int row = 4;
    int nomorUrut = 1;

    for (int i = 0; i < RD_MAX_ITEMS; i++)
    {
        RdItem *item = ramdisk.items[i];

        if (item == nullptr)
            continue;
        const RdItem &it = *item;

        if (!it.used)
            continue;

        char line[64];
        int pos = 0;

        // Kolom nomor urut.
        char noText[12];

        sysFormatItemNumber(
            (u32)nomorUrut,
            1,
            noText);

        for (int c = 0; noText[c] != '\0'; c++)
            line[pos++] = noText[c];

        while (pos < 3)
            line[pos++] = ' ';

        // Kolom ID: minimal 4 digit.
        char idText[12];

        sysFormatItemNumber(
            it.id,
            4,
            idText);

        for (int c = 0; idText[c] != '\0'; c++)
            line[pos++] = idText[c];

        // Posisi awal kolom nama.
        while (pos < 13)
            line[pos++] = ' ';

        // Kolom nama barang.
        int nameStart = pos;

        for (int c = 0;
             it.name[c] != '\0' && pos - nameStart < 18;
             c++)
        {
            line[pos++] = it.name[c];
        }

        while (pos - nameStart < 18)
            line[pos++] = ' ';

        // Kolom jumlah stok.
        char stockText[12];

        sysFormatItemNumber(
            it.stock,
            1,
            stockText);

        for (int c = 0; stockText[c] != '\0'; c++)
            line[pos++] = stockText[c];

        line[pos] = '\0';

        console.writeAt(row, 2, line, CLR_WHITE);

        nomorUrut++;
        row++;

        // Batas area daftar.
        if (row >= 13)
            break;
    }

    // Tampilkan pesan jika tidak ada barang.
    if (nomorUrut == 1)
    {
        console.writeAt(
            4, 2,
            "Belum ada barang.",
            CLR_WHITE);
    }

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
static RdResult sys_save_storage()
{
    if (!sysPermit(PERM_SAVE, "simpan"))
        return RD_ACCESS_DENIED;

    storageMonitorState = STORAGE_MONITOR_RUNNING;
    componentMonitorRefresh();

    CpuMode prev = kernel_enter();

    StorageResult result = storage_save();

    kernel_leave(prev);
    componentMonitorVisualDelay();
    storageMonitorState = STORAGE_MONITOR_READY;
    componentMonitorRefresh();

    if (result == STORAGE_OK)
        return RD_OK;

    return RD_BAD_ARG;
}