
#pragma once

#include <stdint.h>
#include "ramdisk.h"
#include "driver/ata.h"

// ============================================================
// JACK OS - STORAGE MANAGER
// Sistem Snapshot A/B
// ============================================================

#define STORAGE_SECTOR_SIZE 512

#define STORAGE_SLOT_A_START 1
#define STORAGE_SLOT_B_START 65
#define STORAGE_SLOT_SECTORS 64

#define STORAGE_MAGIC 0x4A4B4F53
#define STORAGE_VERSION 1
#define STORAGE_COMMIT_MARKER 0xC04D17ED

// ============================================================
// HASIL OPERASI
// ============================================================

enum StorageResult
{
    STORAGE_OK = 0,
    STORAGE_ERR_NO_DISK,
    STORAGE_ERR_NO_DATA,
    STORAGE_ERR_READ,
    STORAGE_ERR_WRITE,
    STORAGE_ERR_CHECKSUM,
    STORAGE_ERR_FORMAT,
    STORAGE_ERR_INVALID_DATA
};

// ============================================================
// HEADER SNAPSHOT
// ============================================================

struct StorageHeader
{
    u32 magic;
    u32 version;
    u32 generation;
    u32 dataSize;
    u32 checksum;
    u32 commit;
    u32 reserved1;
    u32 reserved2;
};

static_assert(
    sizeof(StorageHeader) == 32,
    "StorageHeader harus berukuran 32 byte");

// ============================================================
// DATA SNAPSHOT
// ============================================================

struct StorageSnapshot
{
    RdItem items[RD_MAX_ITEMS];
    u32 nextId;
};

static_assert(
    sizeof(RdItem) == 32,
    "RdItem harus berukuran 32 byte");

#define STORAGE_DATA_SIZE \
    ((u32)sizeof(StorageSnapshot))

#define STORAGE_DATA_SECTORS \
    ((STORAGE_DATA_SIZE + STORAGE_SECTOR_SIZE - 1) / STORAGE_SECTOR_SIZE)

#define STORAGE_DATA_BUFFER_SIZE \
    (STORAGE_DATA_SECTORS * STORAGE_SECTOR_SIZE)

static_assert(
    STORAGE_DATA_SECTORS + 1 <= STORAGE_SLOT_SECTORS,
    "Snapshot melebihi kapasitas slot");

// ============================================================
// BUFFER SEMENTARA
// Menghindari penggunaan stack yang terlalu besar.
// ============================================================

static ata_u16 storageSectorBuffer[256];

static u8 storageDataBuffer[STORAGE_DATA_BUFFER_SIZE];

static StorageSnapshot storageSnapshotA;
static StorageSnapshot storageSnapshotB;
static StorageSnapshot storageProbe;

// ============================================================
// UTILITAS MEMORI
// ============================================================

static void storageCopy(
    u8 *destination,
    const u8 *source,
    u32 size)
{
    for (u32 i = 0; i < size; i++)
        destination[i] = source[i];
}

static void storageClear(u8 *buffer, u32 size)
{
    for (u32 i = 0; i < size; i++)
        buffer[i] = 0;
}

// ============================================================
// CHECKSUM FNV-1a
// ============================================================

static u32 storageChecksum(const u8 *data, u32 size)
{
    u32 hash = 2166136261u;

    for (u32 i = 0; i < size; i++)
    {
        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

// ============================================================
// PERBANDINGAN GENERASI
// Mendukung kondisi nomor generasi overflow.
// ============================================================

static bool storageIsNewer(u32 a, u32 b)
{
    return (int32_t)(a - b) > 0;
}

// ============================================================
// MEMBACA HEADER
// ============================================================

static bool storageReadHeader(
    u32 sectorLBA,
    StorageHeader *header)
{
    if (header == nullptr)
        return false;

    if (ata_read_sector(
            sectorLBA,
            storageSectorBuffer) != ATA_OK)
    {
        return false;
    }

    storageCopy(
        (u8 *)header,
        (const u8 *)storageSectorBuffer,
        sizeof(StorageHeader));

    return true;
}

// ============================================================
// MEMBACA DAN MEMVALIDASI SLOT
// ============================================================

static bool storageReadSlot(
    u32 slotStart,
    StorageSnapshot *snapshot,
    u32 *generation)
{
    if (snapshot == nullptr || generation == nullptr)
        return false;

    StorageHeader header;

    if (!storageReadHeader(slotStart, &header))
        return false;

    // Validasi identitas snapshot.
    if (header.magic != STORAGE_MAGIC ||
        header.version != STORAGE_VERSION ||
        header.commit != STORAGE_COMMIT_MARKER ||
        header.dataSize != STORAGE_DATA_SIZE)
    {
        return false;
    }

    // Bersihkan buffer sebelum membaca.
    storageClear(
        storageDataBuffer,
        STORAGE_DATA_BUFFER_SIZE);

    // Membaca seluruh sektor data.
    for (u32 i = 0; i < STORAGE_DATA_SECTORS; i++)
    {
        if (ata_read_sector(
                slotStart + 1 + i,
                storageSectorBuffer) != ATA_OK)
        {
            return false;
        }

        storageCopy(
            storageDataBuffer + (i * STORAGE_SECTOR_SIZE),
            (const u8 *)storageSectorBuffer,
            STORAGE_SECTOR_SIZE);
    }

    // Memeriksa checksum.
    u32 checksum = storageChecksum(
        storageDataBuffer,
        STORAGE_DATA_SIZE);

    if (checksum != header.checksum)
        return false;

    // Memulihkan data snapshot.
    storageCopy(
        (u8 *)snapshot,
        storageDataBuffer,
        STORAGE_DATA_SIZE);

    if (snapshot->nextId == 0)
        return false;

    *generation = header.generation;

    return true;
}

// ============================================================
// MEMBUAT SNAPSHOT DARI RAMDISK
// ============================================================

static void storageCreateSnapshot(StorageSnapshot *snapshot)
{
    for (u32 i = 0; i < RD_MAX_ITEMS; i++)
    {
        snapshot->items[i] = ramdisk.items[i];
    }

    snapshot->nextId = ramdisk.nextId;
}

// ============================================================
// MEMULIHKAN SNAPSHOT KE RAMDISK
// ============================================================

static void storageRestoreSnapshot(
    const StorageSnapshot *snapshot)
{
    for (u32 i = 0; i < RD_MAX_ITEMS; i++)
    {
        ramdisk.items[i] = snapshot->items[i];
    }

    ramdisk.nextId = snapshot->nextId;

    // Dirty tidak disimpan di disk.
    ramdisk.dirty = false;
}

// ============================================================
// INISIALISASI STORAGE
// ============================================================

static StorageResult storage_init()
{
    if (!ata_is_ready())
        return STORAGE_ERR_NO_DISK;

    if (ata_get_sector_count() <
        STORAGE_SLOT_B_START + STORAGE_SLOT_SECTORS)
    {
        return STORAGE_ERR_NO_DISK;
    }

    return STORAGE_OK;
}

// ============================================================
// LOAD SNAPSHOT TERBARU
// ============================================================

static StorageResult storage_load()
{
    if (!ata_is_ready())
        return STORAGE_ERR_NO_DISK;

    u32 generationA = 0;
    u32 generationB = 0;

    bool validA = storageReadSlot(
        STORAGE_SLOT_A_START,
        &storageSnapshotA,
        &generationA);

    bool validB = storageReadSlot(
        STORAGE_SLOT_B_START,
        &storageSnapshotB,
        &generationB);

    if (!validA && !validB)
        return STORAGE_ERR_NO_DATA;

    if (validA && validB)
    {
        if (storageIsNewer(generationB, generationA))
        {
            storageRestoreSnapshot(&storageSnapshotB);
        }
        else
        {
            storageRestoreSnapshot(&storageSnapshotA);
        }
    }
    else if (validA)
    {
        storageRestoreSnapshot(&storageSnapshotA);
    }
    else
    {
        storageRestoreSnapshot(&storageSnapshotB);
    }

    return STORAGE_OK;
}

// ============================================================
// MENULIS HEADER
// ============================================================

static StorageResult storageWriteHeader(
    u32 sectorLBA,
    const StorageHeader *header)
{
    if (header == nullptr)
        return STORAGE_ERR_INVALID_DATA;

    storageClear(
        (u8 *)storageSectorBuffer,
        STORAGE_SECTOR_SIZE);

    storageCopy(
        (u8 *)storageSectorBuffer,
        (const u8 *)header,
        sizeof(StorageHeader));

    if (ata_write_sector(
            sectorLBA,
            storageSectorBuffer) != ATA_OK)
    {
        return STORAGE_ERR_WRITE;
    }

    return STORAGE_OK;
}

// ============================================================
// SAVE SNAPSHOT
// ============================================================

static StorageResult storage_save()
{
    if (!ata_is_ready())
        return STORAGE_ERR_NO_DISK;

    // Membuat salinan data RAMDisk.
    StorageSnapshot currentSnapshot;

    storageCreateSnapshot(&currentSnapshot);

    storageClear(
        storageDataBuffer,
        STORAGE_DATA_BUFFER_SIZE);

    storageCopy(
        storageDataBuffer,
        (const u8 *)&currentSnapshot,
        STORAGE_DATA_SIZE);

    // --------------------------------------------------------
    // MEMERIKSA SLOT A DAN SLOT B
    // --------------------------------------------------------

    u32 generationA = 0;
    u32 generationB = 0;

    bool validA = storageReadSlot(
        STORAGE_SLOT_A_START,
        &storageProbe,
        &generationA);

    bool validB = storageReadSlot(
        STORAGE_SLOT_B_START,
        &storageProbe,
        &generationB);

    u32 targetSlot;
    u32 newestGeneration = 0;

    // --------------------------------------------------------
    // MENENTUKAN SLOT TUJUAN
    // --------------------------------------------------------

    if (validA && validB)
    {
        if (storageIsNewer(generationA, generationB))
        {
            newestGeneration = generationA;
            targetSlot = STORAGE_SLOT_B_START;
        }
        else
        {
            newestGeneration = generationB;
            targetSlot = STORAGE_SLOT_A_START;
        }
    }
    else if (validA)
    {
        newestGeneration = generationA;
        targetSlot = STORAGE_SLOT_B_START;
    }
    else if (validB)
    {
        newestGeneration = generationB;
        targetSlot = STORAGE_SLOT_A_START;
    }
    else
    {
        targetSlot = STORAGE_SLOT_A_START;
    }

    u32 newGeneration = newestGeneration + 1;

    if (newGeneration == 0)
        newGeneration = 1;

    // --------------------------------------------------------
    // 1. INVALIDASI HEADER SLOT TUJUAN
    // --------------------------------------------------------

    storageClear(
        (u8 *)storageSectorBuffer,
        STORAGE_SECTOR_SIZE);

    if (ata_write_sector(
            targetSlot,
            storageSectorBuffer) != ATA_OK)
    {
        return STORAGE_ERR_WRITE;
    }

    // --------------------------------------------------------
    // 2. MENULIS DATA SNAPSHOT
    // --------------------------------------------------------

    for (u32 i = 0; i < STORAGE_DATA_SECTORS; i++)
    {
        storageCopy(
            (u8 *)storageSectorBuffer,
            storageDataBuffer + (i * STORAGE_SECTOR_SIZE),
            STORAGE_SECTOR_SIZE);

        if (ata_write_sector(
                targetSlot + 1 + i,
                storageSectorBuffer) != ATA_OK)
        {
            return STORAGE_ERR_WRITE;
        }
    }

    // --------------------------------------------------------
    // 3. MEMBUAT HEADER BARU
    // --------------------------------------------------------

    StorageHeader header;

    header.magic = STORAGE_MAGIC;
    header.version = STORAGE_VERSION;
    header.generation = newGeneration;
    header.dataSize = STORAGE_DATA_SIZE;

    header.checksum = storageChecksum(
        storageDataBuffer,
        STORAGE_DATA_SIZE);

    header.commit = STORAGE_COMMIT_MARKER;
    header.reserved1 = 0;
    header.reserved2 = 0;

    // --------------------------------------------------------
    // 4. COMMIT HEADER TERAKHIR
    // --------------------------------------------------------

    StorageResult result = storageWriteHeader(
        targetSlot,
        &header);

    if (result != STORAGE_OK)
        return result;

    // --------------------------------------------------------
    // 5. PENYIMPANAN BERHASIL
    // --------------------------------------------------------

    ramdisk.dirty = false;

    return STORAGE_OK;
}
