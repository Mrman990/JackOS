
#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stddef.h>

// File ini di-include setelah:
// ramdisk.h
// logbook.h
// driver/ata.h

namespace storage {

// ==================================================
// 1. KONFIGURASI STORAGE
// ==================================================

static const uint32_t STORAGE_SECTOR_SIZE = 512;

static const uint32_t SLOT_SIZE = 64;

static const uint32_t SLOT_A_START = 1;
static const uint32_t SLOT_B_START = 65;

static const uint32_t STORAGE_VERSION = 1;

static const uint32_t MAGIC_NUMBER = 0x4A41434B;
static const uint32_t COMMIT_MARKER = 0x434F4D54;

// Ukuran satu snapshot:
// RAMDisk:
//   nextId                  = 4 byte
//   64 * RdItem             = 2048 byte
//
// LogBook:
//   logBookCount            = 4 byte
//   logBookNext             = 4 byte
//   logSeq                  = 4 byte
//   100 * LogEntry          = 5200 byte
//
// Total                     = 7264 byte

static const uint32_t SYSTEM_DATA_SIZE =
    4 + (RD_MAX_ITEMS * 32) + 12 + (LOG_MAX * 52);

static const uint32_t SLOT_CAPACITY =
    (SLOT_SIZE - 1) * STORAGE_SECTOR_SIZE;

// ==================================================
// 2. STATUS STORAGE
// ==================================================

enum Result {
    STORAGE_OK,
    STORAGE_NOT_FOUND,
    STORAGE_DISK_ERROR,
    STORAGE_INVALID_HEADER,
    STORAGE_INVALID_CHECKSUM,
    STORAGE_DATA_TOO_LARGE,
    STORAGE_INVALID_ARGUMENT,
    STORAGE_NO_VALID_SLOT
};

// ==================================================
// 3. HEADER PENYIMPANAN
// ==================================================

struct Header {
    uint32_t magic;
    uint32_t version;
    uint32_t generation;
    uint32_t dataSize;
    uint32_t checksum;
    uint32_t commit;
    uint32_t reserved1;
    uint32_t reserved2;
};

static_assert(sizeof(Header) == 32,
              "Storage Header harus 32 byte");

// ==================================================
// 4. INFORMASI SLOT
// ==================================================

struct SlotInfo {
    bool valid;
    uint32_t startSector;
    uint32_t generation;
    uint32_t dataSize;
};

// ==================================================
// 5. BUFFER INTERNAL
// ==================================================

static uint8_t sectorBuffer[STORAGE_SECTOR_SIZE];

static uint8_t systemBuffer[SYSTEM_DATA_SIZE];

// Buffer sementara agar tidak menggunakan stack besar.
static RdItem tempItems[RD_MAX_ITEMS];
static LogEntry tempLogs[LOG_MAX];

// ==================================================
// 6. FUNGSI MEMORI
// ==================================================

static void clearMemory(uint8_t* destination, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        destination[i] = 0;
}

static void copyMemory(
    uint8_t* destination,
    const uint8_t* source,
    uint32_t size
) {
    for (uint32_t i = 0; i < size; i++)
        destination[i] = source[i];
}

// ==================================================
// 7. CHECKSUM FNV-1a
// ==================================================

static uint32_t calculateChecksum(
    const uint8_t* data,
    uint32_t size
) {
    uint32_t hash = 2166136261u;

    for (uint32_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }

    return hash;
}

// ==================================================
// 8. SERIALISASI INTEGER
// ==================================================

static void put32(
    uint8_t* buffer,
    uint32_t& offset,
    uint32_t value
) {
    buffer[offset++] = (uint8_t)value;
    buffer[offset++] = (uint8_t)(value >> 8);
    buffer[offset++] = (uint8_t)(value >> 16);
    buffer[offset++] = (uint8_t)(value >> 24);
}

static uint32_t get32(
    const uint8_t* buffer,
    uint32_t& offset
) {
    uint32_t value =
        (uint32_t)buffer[offset] |
        ((uint32_t)buffer[offset + 1] << 8) |
        ((uint32_t)buffer[offset + 2] << 16) |
        ((uint32_t)buffer[offset + 3] << 24);

    offset += 4;

    return value;
}

// ==================================================
// 9. ENCODE DAN DECODE HEADER
// ==================================================

static void encodeHeader(
    const Header& header,
    uint8_t* buffer
) {
    clearMemory(buffer, STORAGE_SECTOR_SIZE);

    uint32_t offset = 0;

    put32(buffer, offset, header.magic);
    put32(buffer, offset, header.version);
    put32(buffer, offset, header.generation);
    put32(buffer, offset, header.dataSize);
    put32(buffer, offset, header.checksum);
    put32(buffer, offset, header.commit);
    put32(buffer, offset, header.reserved1);
    put32(buffer, offset, header.reserved2);
}

static void decodeHeader(
    const uint8_t* buffer,
    Header& header
) {
    uint32_t offset = 0;

    header.magic = get32(buffer, offset);
    header.version = get32(buffer, offset);
    header.generation = get32(buffer, offset);
    header.dataSize = get32(buffer, offset);
    header.checksum = get32(buffer, offset);
    header.commit = get32(buffer, offset);
    header.reserved1 = get32(buffer, offset);
    header.reserved2 = get32(buffer, offset);
}

// ==================================================
// 10. BACA HEADER
// ==================================================

static Result readHeader(
    uint32_t startSector,
    Header& header
) {
    ata::Result result =
        ata::readSector(startSector, sectorBuffer);

    if (result != ata::ATA_OK)
        return STORAGE_DISK_ERROR;

    decodeHeader(sectorBuffer, header);

    if (header.magic != MAGIC_NUMBER ||
        header.version != STORAGE_VERSION ||
        header.commit != COMMIT_MARKER) {
        return STORAGE_INVALID_HEADER;
    }

    if (header.dataSize == 0 ||
        header.dataSize > SLOT_CAPACITY) {
        return STORAGE_INVALID_HEADER;
    }

    return STORAGE_OK;
}

// ==================================================
// 11. TULIS HEADER
// ==================================================

static Result writeHeader(
    uint32_t startSector,
    const Header& header
) {
    encodeHeader(header, sectorBuffer);

    ata::Result result =
        ata::writeSector(startSector, sectorBuffer);

    if (result != ata::ATA_OK)
        return STORAGE_DISK_ERROR;

    return STORAGE_OK;
}

// ==================================================
// 12. PEMERIKSAAN SLOT
// ==================================================

static SlotInfo inspectSlot(uint32_t startSector)
{
    SlotInfo info;

    info.valid = false;
    info.startSector = startSector;
    info.generation = 0;
    info.dataSize = 0;

    Header header;

    if (readHeader(startSector, header) != STORAGE_OK)
        return info;

    info.valid = true;
    info.generation = header.generation;
    info.dataSize = header.dataSize;

    return info;
}

// ==================================================
// 13. MEMILIH SLOT TERBARU
// ==================================================

static Result findLatestSlot(SlotInfo& latest)
{
    SlotInfo slotA = inspectSlot(SLOT_A_START);
    SlotInfo slotB = inspectSlot(SLOT_B_START);

    if (!slotA.valid && !slotB.valid)
        return STORAGE_NO_VALID_SLOT;

    if (slotA.valid && !slotB.valid) {
        latest = slotA;
        return STORAGE_OK;
    }

    if (!slotA.valid && slotB.valid) {
        latest = slotB;
        return STORAGE_OK;
    }

    if ((int32_t)(slotA.generation - slotB.generation) > 0)
        latest = slotA;
    else
        latest = slotB;

    return STORAGE_OK;
}

// ==================================================
// 14. MEMBACA DATA SLOT
// ==================================================

static Result readSlotData(
    const SlotInfo& slot,
    uint8_t* destination,
    uint32_t destinationSize
) {
    if (!slot.valid || destination == nullptr)
        return STORAGE_INVALID_ARGUMENT;

    if (slot.dataSize > destinationSize)
        return STORAGE_DATA_TOO_LARGE;

    uint32_t remaining = slot.dataSize;
    uint32_t offset = 0;
    uint32_t sector = slot.startSector + 1;

    while (remaining > 0) {
        ata::Result result =
            ata::readSector(sector, sectorBuffer);

        if (result != ata::ATA_OK)
            return STORAGE_DISK_ERROR;

        uint32_t amount = remaining;

        if (amount > STORAGE_SECTOR_SIZE)
            amount = STORAGE_SECTOR_SIZE;

        copyMemory(
            destination + offset,
            sectorBuffer,
            amount
        );

        offset += amount;
        remaining -= amount;
        sector++;
    }

    return STORAGE_OK;
}

// ==================================================
// 15. MENULIS DATA SLOT
// ==================================================

static Result writeSlotData(
    uint32_t startSector,
    const uint8_t* data,
    uint32_t dataSize
) {
    if (data == nullptr || dataSize == 0)
        return STORAGE_INVALID_ARGUMENT;

    if (dataSize > SLOT_CAPACITY)
        return STORAGE_DATA_TOO_LARGE;

    uint32_t remaining = dataSize;
    uint32_t offset = 0;
    uint32_t sector = startSector + 1;

    while (remaining > 0) {
        clearMemory(sectorBuffer, STORAGE_SECTOR_SIZE);

        uint32_t amount = remaining;

        if (amount > STORAGE_SECTOR_SIZE)
            amount = STORAGE_SECTOR_SIZE;

        copyMemory(
            sectorBuffer,
            data + offset,
            amount
        );

        ata::Result result =
            ata::writeSector(sector, sectorBuffer);

        if (result != ata::ATA_OK)
            return STORAGE_DISK_ERROR;

        offset += amount;
        remaining -= amount;
        sector++;
    }

    return STORAGE_OK;
}

// ==================================================
// 16. LOAD BUFFER
// ==================================================

static Result load(
    uint8_t* destination,
    uint32_t destinationSize,
    uint32_t& loadedSize
) {
    loadedSize = 0;

    if (destination == nullptr)
        return STORAGE_INVALID_ARGUMENT;

    SlotInfo latest;

    Result result = findLatestSlot(latest);

    if (result != STORAGE_OK)
        return result;

    result = readSlotData(
        latest,
        destination,
        destinationSize
    );

    if (result != STORAGE_OK)
        return result;

    uint32_t checksum = calculateChecksum(
        destination,
        latest.dataSize
    );

    Header header;

    result = readHeader(latest.startSector, header);

    if (result != STORAGE_OK)
        return result;

    if (checksum != header.checksum)
        return STORAGE_INVALID_CHECKSUM;

    loadedSize = latest.dataSize;

    return STORAGE_OK;
}

// ==================================================
// 17. SAVE BUFFER
// ==================================================

static Result save(
    const uint8_t* data,
    uint32_t dataSize
) {
    if (data == nullptr || dataSize == 0)
        return STORAGE_INVALID_ARGUMENT;

    if (dataSize > SLOT_CAPACITY)
        return STORAGE_DATA_TOO_LARGE;

    SlotInfo latest;

    Result latestResult = findLatestSlot(latest);

    uint32_t targetSector;
    uint32_t generation;

    if (latestResult == STORAGE_OK) {
        targetSector =
            latest.startSector == SLOT_A_START
            ? SLOT_B_START
            : SLOT_A_START;

        generation = latest.generation + 1;
    }
    else if (latestResult == STORAGE_NO_VALID_SLOT) {
        targetSector = SLOT_A_START;
        generation = 1;
    }
    else {
        return latestResult;
    }

    // Kosongkan header slot tujuan terlebih dahulu.
    Header invalidHeader = {};

    Result result = writeHeader(
        targetSector,
        invalidHeader
    );

    if (result != STORAGE_OK)
        return result;

    // Tulis seluruh data.
    result = writeSlotData(
        targetSector,
        data,
        dataSize
    );

    if (result != STORAGE_OK)
        return result;

    // Siapkan header baru.
    Header header = {};

    header.magic = MAGIC_NUMBER;
    header.version = STORAGE_VERSION;
    header.generation = generation;
    header.dataSize = dataSize;
    header.checksum = calculateChecksum(data, dataSize);
    header.commit = COMMIT_MARKER;

    // Commit dilakukan paling akhir.
    result = writeHeader(targetSector, header);

    if (result != STORAGE_OK)
        return result;

    // Periksa header yang baru ditulis.
    SlotInfo saved = inspectSlot(targetSector);

    if (!saved.valid ||
        saved.generation != generation ||
        saved.dataSize != dataSize) {
        return STORAGE_INVALID_HEADER;
    }

    return STORAGE_OK;
}

// ==================================================
// 18. SERIALISASI SATU LOGENTRY
// ==================================================

static void serializeLog(
    uint8_t* buffer,
    uint32_t& offset,
    const LogEntry& entry
) {
    put32(buffer, offset, entry.no);
    put32(buffer, offset, entry.jam);
    put32(buffer, offset, entry.menit);
    put32(buffer, offset, entry.detik);
    put32(buffer, offset, entry.kind);
    put32(buffer, offset, entry.itemId);

    for (int i = 0; i < RD_NAME_LEN; i++)
        buffer[offset++] = (uint8_t)entry.name[i];

    put32(buffer, offset, entry.oldStock);
    put32(buffer, offset, entry.newStock);
}

// ==================================================
// 19. DESERIALISASI SATU LOGENTRY
// ==================================================

static void deserializeLog(
    const uint8_t* buffer,
    uint32_t& offset,
    LogEntry& entry
) {
    entry.no = get32(buffer, offset);
    entry.jam = get32(buffer, offset);
    entry.menit = get32(buffer, offset);
    entry.detik = get32(buffer, offset);
    entry.kind = get32(buffer, offset);
    entry.itemId = get32(buffer, offset);

    for (int i = 0; i < RD_NAME_LEN; i++)
        entry.name[i] = (char)buffer[offset++];

    entry.oldStock = get32(buffer, offset);
    entry.newStock = get32(buffer, offset);
}

// ==================================================
// 20. SERIALISASI SISTEM
// ==================================================

static Result serializeSystem()
{
    uint32_t offset = 0;

    put32(systemBuffer, offset, ramdisk.nextId);

    // RAMDisk.
    for (int i = 0; i < RD_MAX_ITEMS; i++) {
        const RdItem& item = ramdisk.items[i];

        put32(systemBuffer, offset, item.id);

        for (int c = 0; c < RD_NAME_LEN; c++)
            systemBuffer[offset++] = (uint8_t)item.name[c];

        put32(systemBuffer, offset, item.stock);
        put32(systemBuffer, offset, item.used);
    }

    // Metadata LogBook.
    put32(systemBuffer, offset, (uint32_t)logBookCount);
    put32(systemBuffer, offset, (uint32_t)logBookNext);
    put32(systemBuffer, offset, logSeq);

    // Seluruh LogBook.
    for (int i = 0; i < LOG_MAX; i++)
        serializeLog(systemBuffer, offset, logBook[i]);

    if (offset != SYSTEM_DATA_SIZE)
        return STORAGE_INVALID_ARGUMENT;

    return STORAGE_OK;
}

// ==================================================
// 21. VALIDASI DAN DESERIALISASI SISTEM
// ==================================================

static Result deserializeSystem()
{
    uint32_t offset = 0;

    uint32_t nextId = get32(systemBuffer, offset);

    if (nextId == 0 || nextId > RD_MAX_ITEMS + 1)
        return STORAGE_INVALID_ARGUMENT;

    // Baca RAMDisk ke buffer sementara.
    uint32_t usedCount = 0;

    for (int i = 0; i < RD_MAX_ITEMS; i++) {
        RdItem& item = tempItems[i];

        item.id = get32(systemBuffer, offset);

        for (int c = 0; c < RD_NAME_LEN; c++)
            item.name[c] = (char)systemBuffer[offset++];

        item.stock = get32(systemBuffer, offset);
        item.used = get32(systemBuffer, offset);

        if (item.used > 1 || item.stock > RD_MAX_STOCK)
            return STORAGE_INVALID_ARGUMENT;

        if (item.used) {
            usedCount++;

            if (item.id != (uint32_t)(i + 1))
                return STORAGE_INVALID_ARGUMENT;

            if (item.name[RD_NAME_LEN - 1] != '\0')
                return STORAGE_INVALID_ARGUMENT;
        }
        else {
            if (item.id != 0 || item.stock != 0)
                return STORAGE_INVALID_ARGUMENT;
        }
    }

    if (nextId != usedCount + 1)
        return STORAGE_INVALID_ARGUMENT;

    // Baca metadata LogBook.
    uint32_t tempLogCount = get32(systemBuffer, offset);
    uint32_t tempLogNext = get32(systemBuffer, offset);
    uint32_t tempLogSeq = get32(systemBuffer, offset);

    if (tempLogCount > LOG_MAX ||
        tempLogNext >= LOG_MAX) {
        return STORAGE_INVALID_ARGUMENT;
    }

    // Baca LogBook ke buffer sementara.
    for (int i = 0; i < LOG_MAX; i++) {
        deserializeLog(systemBuffer, offset, tempLogs[i]);

        if (tempLogs[i].kind > LOG_KELUAR)
            return STORAGE_INVALID_ARGUMENT;
    }

    if (offset != SYSTEM_DATA_SIZE)
        return STORAGE_INVALID_ARGUMENT;

    // ======================================
    // TERAPKAN DATA SETELAH VALIDASI
    // ======================================

    for (int i = 0; i < RD_MAX_ITEMS; i++) {
        ramdisk.items[i].id = tempItems[i].id;
        ramdisk.items[i].stock = tempItems[i].stock;
        ramdisk.items[i].used = tempItems[i].used;

        for (int c = 0; c < RD_NAME_LEN; c++)
            ramdisk.items[i].name[c] = tempItems[i].name[c];
    }

    ramdisk.nextId = nextId;
    ramdisk.dirty = false;

    for (int i = 0; i < LOG_MAX; i++)
        logCopy(logBook[i], tempLogs[i]);

    logBookCount = (int)tempLogCount;
    logBookNext = (int)tempLogNext;
    logSeq = tempLogSeq;

    // Antrean log lama tidak dipulihkan.
    logPendCount = 0;

    logDirty = false;

    return STORAGE_OK;
}

// ==================================================
// 22. SIMPAN SELURUH DATA JACK OS
// ==================================================

static Result saveSystem()
{
    // Pastikan antrean LogBook diproses.
    logFlush();

    Result result = serializeSystem();

    if (result != STORAGE_OK)
        return result;

    result = save(systemBuffer, SYSTEM_DATA_SIZE);

    if (result != STORAGE_OK)
        return result;

    ramdisk.dirty = false;
    logDirty = false;

    return STORAGE_OK;
}

// ==================================================
// 23. MUAT SELURUH DATA JACK OS
// ==================================================

static Result loadSystem()
{
    uint32_t loadedSize = 0;

    Result result = load(
        systemBuffer,
        SYSTEM_DATA_SIZE,
        loadedSize
    );

    if (result != STORAGE_OK)
        return result;

    if (loadedSize != SYSTEM_DATA_SIZE)
        return STORAGE_INVALID_ARGUMENT;

    return deserializeSystem();
}

// ==================================================
// 24. INISIALISASI STORAGE
// ==================================================

static Result init()
{
    SlotInfo latest;

    Result result = findLatestSlot(latest);

    if (result == STORAGE_OK)
        return STORAGE_OK;

    if (result == STORAGE_NO_VALID_SLOT)
        return STORAGE_NO_VALID_SLOT;

    return result;
}

// ==================================================
// 25. STATUS PERUBAHAN DATA
// ==================================================

static bool hasUnsavedChanges()
{
    return ramdisk.dirty || logDirty;
}

} // namespace storage

#endif
