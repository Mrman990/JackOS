
#ifndef ATA_H
#define ATA_H

#include <stdint.h>

namespace ata {

// ==========================================
// KONFIGURASI PRIMARY IDE MASTER
// ==========================================

static const uint16_t DATA_PORT    = 0x1F0;
static const uint16_t ERROR_PORT   = 0x1F1;
static const uint16_t SECTOR_COUNT = 0x1F2;
static const uint16_t LBA_LOW      = 0x1F3;
static const uint16_t LBA_MID      = 0x1F4;
static const uint16_t LBA_HIGH     = 0x1F5;
static const uint16_t DRIVE_PORT   = 0x1F6;
static const uint16_t STATUS_PORT  = 0x1F7;
static const uint16_t COMMAND_PORT = 0x1F7;
static const uint16_t ALT_STATUS   = 0x3F6;

static const uint8_t CMD_IDENTIFY = 0xEC;
static const uint8_t CMD_READ     = 0x20;
static const uint8_t CMD_WRITE    = 0x30;
static const uint8_t CMD_FLUSH    = 0xE7;

static const uint8_t STATUS_ERR = 0x01;
static const uint8_t STATUS_DRQ = 0x08;
static const uint8_t STATUS_DF  = 0x20;
static const uint8_t STATUS_BSY = 0x80;

static const uint32_t SECTOR_SIZE = 512;
static const uint32_t MAX_LBA28   = 0x0FFFFFFF;

// ==========================================
// HASIL OPERASI ATA
// ==========================================

enum Result {
    ATA_OK,
    ATA_NOT_FOUND,
    ATA_ERROR,
    ATA_TIMEOUT,
    ATA_INVALID_ARGUMENT
};

// ==========================================
// I/O PORT
// ==========================================

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;

    asm volatile (
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;

    asm volatile (
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port)
    );

    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

// ==========================================
// DELAY 400 NANOSECOND
// ==========================================

static inline void delay400ns() {
    inb(ALT_STATUS);
    inb(ALT_STATUS);
    inb(ALT_STATUS);
    inb(ALT_STATUS);
}

// ==========================================
// MENUNGGU STATUS PERANGKAT
// ==========================================

static inline Result waitNotBusy(uint32_t timeout = 1000000) {
    while (timeout--) {
        uint8_t status = inb(STATUS_PORT);

        if (status == 0xFF || status == 0x00) {
            return ATA_NOT_FOUND;
        }

        if (!(status & STATUS_BSY)) {
            return ATA_OK;
        }
    }

    return ATA_TIMEOUT;
}

static inline Result waitDataRequest(uint32_t timeout = 1000000) {
    while (timeout--) {
        uint8_t status = inb(STATUS_PORT);

        if (status == 0xFF || status == 0x00) {
            return ATA_NOT_FOUND;
        }

        if (status & STATUS_BSY) {
            continue;
        }

        if (status & STATUS_ERR || status & STATUS_DF) {
            return ATA_ERROR;
        }

        if (status & STATUS_DRQ) {
            return ATA_OK;
        }
    }

    return ATA_TIMEOUT;
}

// ==========================================
// PEMILIHAN DRIVE
// ==========================================

static inline Result selectDrive() {
    outb(DRIVE_PORT, 0xE0);
    delay400ns();

    return waitNotBusy();
}

// ==========================================
// INISIALISASI DAN IDENTIFIKASI ATA
// ==========================================

static inline Result init() {
    Result result = selectDrive();

    if (result != ATA_OK) {
        return result;
    }

    outb(SECTOR_COUNT, 0);
    outb(LBA_LOW, 0);
    outb(LBA_MID, 0);
    outb(LBA_HIGH, 0);

    outb(COMMAND_PORT, CMD_IDENTIFY);
    delay400ns();

    uint8_t status = inb(STATUS_PORT);

    if (status == 0x00 || status == 0xFF) {
        return ATA_NOT_FOUND;
    }

    result = waitNotBusy();

    if (result != ATA_OK) {
        return result;
    }

    if (inb(LBA_MID) != 0 || inb(LBA_HIGH) != 0) {
        return ATA_NOT_FOUND;
    }

    result = waitDataRequest();

    if (result != ATA_OK) {
        return result;
    }

    // Membaca 256 word IDENTIFY.
    for (uint32_t i = 0; i < 256; i++) {
        (void)inw(DATA_PORT);
    }

    return ATA_OK;
}

// ==========================================
// VALIDASI LBA
// ==========================================

static inline bool validLBA(uint32_t lba) {
    return lba <= MAX_LBA28;
}

// ==========================================
// MEMBACA SATU SEKTOR
// ==========================================

static inline Result readSector(
    uint32_t lba,
    uint8_t* buffer
) {
    if (buffer == nullptr || !validLBA(lba)) {
        return ATA_INVALID_ARGUMENT;
    }

    Result result = selectDrive();

    if (result != ATA_OK) {
        return result;
    }

    outb(DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));

    outb(SECTOR_COUNT, 1);
    outb(LBA_LOW, (uint8_t)(lba));
    outb(LBA_MID, (uint8_t)(lba >> 8));
    outb(LBA_HIGH, (uint8_t)(lba >> 16));

    outb(COMMAND_PORT, CMD_READ);
    delay400ns();

    result = waitDataRequest();

    if (result != ATA_OK) {
        return result;
    }

    for (uint32_t i = 0; i < 256; i++) {
        uint16_t data = inw(DATA_PORT);

        buffer[i * 2] =
            (uint8_t)(data & 0xFF);

        buffer[i * 2 + 1] =
            (uint8_t)(data >> 8);
    }

    return ATA_OK;
}

// ==========================================
// MENULIS SATU SEKTOR
// ==========================================

static inline Result writeSector(
    uint32_t lba,
    const uint8_t* buffer
) {
    if (buffer == nullptr || !validLBA(lba)) {
        return ATA_INVALID_ARGUMENT;
    }

    Result result = selectDrive();

    if (result != ATA_OK) {
        return result;
    }

    outb(DRIVE_PORT, 0xE0 | ((lba >> 24) & 0x0F));

    outb(SECTOR_COUNT, 1);
    outb(LBA_LOW, (uint8_t)(lba));
    outb(LBA_MID, (uint8_t)(lba >> 8));
    outb(LBA_HIGH, (uint8_t)(lba >> 16));

    outb(COMMAND_PORT, CMD_WRITE);
    delay400ns();

    result = waitDataRequest();

    if (result != ATA_OK) {
        return result;
    }

    for (uint32_t i = 0; i < 256; i++) {
        uint16_t data =
            (uint16_t)buffer[i * 2] |
            ((uint16_t)buffer[i * 2 + 1] << 8);

        outw(DATA_PORT, data);
    }

    outb(COMMAND_PORT, CMD_FLUSH);

    result = waitNotBusy();

    if (result != ATA_OK) {
        return result;
    }

    uint8_t status = inb(STATUS_PORT);

    if (status & STATUS_ERR || status & STATUS_DF) {
        return ATA_ERROR;
    }

    return ATA_OK;
}

// ==========================================
// PENGUJIAN BACA DAN TULIS SEKTOR
// ==========================================

static inline Result testReadWrite() {
    uint8_t writeBuffer[SECTOR_SIZE];
    uint8_t readBuffer[SECTOR_SIZE];

    // Sektor 10 digunakan khusus pengujian.
    const uint32_t testLBA = 10;

    // Membuat pola data pengujian.
    for (uint32_t i = 0; i < SECTOR_SIZE; i++) {
        writeBuffer[i] = (uint8_t)(i & 0xFF);
        readBuffer[i] = 0;
    }

    // Menulis data.
    Result result = writeSector(testLBA, writeBuffer);

    if (result != ATA_OK) {
        return result;
    }

    // Membaca kembali data.
    result = readSector(testLBA, readBuffer);

    if (result != ATA_OK) {
        return result;
    }

    // Membandingkan hasil.
    for (uint32_t i = 0; i < SECTOR_SIZE; i++) {
        if (writeBuffer[i] != readBuffer[i]) {
            return ATA_ERROR;
        }
    }

    return ATA_OK;
}

} // namespace ata

#endif
