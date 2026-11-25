
#pragma once

#include <stdint.h>

// ============================================================
// JACK OS - ATA PIO DRIVER
// Primary IDE Master
// 28-bit LBA
// Sector size: 512 bytes
// ============================================================

typedef unsigned char ata_u8;
typedef unsigned short ata_u16;
typedef unsigned int ata_u32;

// ============================================================
// ATA PORT
// ============================================================

#define ATA_PRIMARY_IO 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6

#define ATA_DATA (ATA_PRIMARY_IO + 0)
#define ATA_ERROR (ATA_PRIMARY_IO + 1)
#define ATA_SECTOR_COUNT (ATA_PRIMARY_IO + 2)
#define ATA_LBA_LOW (ATA_PRIMARY_IO + 3)
#define ATA_LBA_MID (ATA_PRIMARY_IO + 4)
#define ATA_LBA_HIGH (ATA_PRIMARY_IO + 5)
#define ATA_DRIVE (ATA_PRIMARY_IO + 6)
#define ATA_STATUS (ATA_PRIMARY_IO + 7)
#define ATA_COMMAND (ATA_PRIMARY_IO + 7)

// ============================================================
// ATA COMMAND
// ============================================================

#define ATA_CMD_READ 0x20
#define ATA_CMD_WRITE 0x30
#define ATA_CMD_FLUSH 0xE7
#define ATA_CMD_IDENTIFY 0xEC

// ============================================================
// ATA STATUS
// ============================================================

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF 0x20
#define ATA_SR_BSY 0x80

// ============================================================
// ATA RESULT
// ============================================================

enum ATAResult
{
    ATA_OK = 0,
    ATA_ERR_NO_DEVICE,
    ATA_ERR_BUSY,
    ATA_ERR,
    ATA_ERR_DEVICE_FAULT,
    ATA_ERR_TIMEOUT,
    ATA_ERR_INVALID_ARGUMENT,
    ATA_ERR_LBA_RANGE
};

// ============================================================
// ATA DEVICE INFORMATION
// ============================================================

struct ATADevice
{
    bool detected;
    bool initialized;
    ata_u32 sectorCount;
};

static ATADevice ataDevice = {false, false, 0};

// ============================================================
// PORT I/O
// ============================================================

static inline void ata_outb(ata_u16 port, ata_u8 value)
{
    __asm__ __volatile__(
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port));
}

static inline ata_u8 ata_inb(ata_u16 port)
{
    ata_u8 value;

    __asm__ __volatile__(
        "inb %1, %0"
        : "=a"(value)
        : "Nd"(port));

    return value;
}

static inline void ata_outw(ata_u16 port, ata_u16 value)
{
    __asm__ __volatile__(
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port));
}

static inline ata_u16 ata_inw(ata_u16 port)
{
    ata_u16 value;

    __asm__ __volatile__(
        "inw %1, %0"
        : "=a"(value)
        : "Nd"(port));

    return value;
}

// ============================================================
// ATA DELAY
// 400ns delay using alternate status register.
// ============================================================

static inline void ata_delay_400ns()
{
    ata_inb(ATA_PRIMARY_CTRL);
    ata_inb(ATA_PRIMARY_CTRL);
    ata_inb(ATA_PRIMARY_CTRL);
    ata_inb(ATA_PRIMARY_CTRL);
}

// ============================================================
// WAIT FOR DEVICE
// ============================================================

static ATAResult ata_wait(bool checkError = true)
{
    ata_u32 timeout = 1000000;

    ata_u8 status = 0;

    while (timeout--)
    {
        status = ata_inb(ATA_STATUS);

        if (status == 0xFF || status == 0x00)
            return ATA_ERR_NO_DEVICE;

        if (!(status & ATA_SR_BSY))
        {
            if (status & ATA_SR_DF)
                return ATA_ERR_DEVICE_FAULT;

            if (checkError && (status & ATA_SR_ERR))
                return ATA_ERR;

            return ATA_OK;
        }
    }

    return ATA_ERR_TIMEOUT;
}

// ============================================================
// WAIT FOR DATA REQUEST
// ============================================================

static ATAResult ata_wait_drq()
{
    ata_u32 timeout = 1000000;

    while (timeout--)
    {
        ata_u8 status = ata_inb(ATA_STATUS);

        if (status == 0xFF || status == 0x00)
            return ATA_ERR_NO_DEVICE;

        if (status & ATA_SR_BSY)
            continue;

        if (status & ATA_SR_DF)
            return ATA_ERR_DEVICE_FAULT;

        if (status & ATA_SR_ERR)
            return ATA_ERR;

        if (status & ATA_SR_DRQ)
            return ATA_OK;
    }

    return ATA_ERR_TIMEOUT;
}

// ============================================================
// SELECT PRIMARY MASTER
// ============================================================

static void ata_select_master()
{
    ata_outb(ATA_DRIVE, 0xE0);
    ata_delay_400ns();
}

// ============================================================
// INITIALIZE ATA DEVICE
// ============================================================

static ATAResult ata_init()
{
    ataDevice.detected = false;
    ataDevice.initialized = false;
    ataDevice.sectorCount = 0;

    // Select Primary Master.
    ata_select_master();

    // Check whether the device is present.
    ata_outb(ATA_SECTOR_COUNT, 0);
    ata_outb(ATA_LBA_LOW, 0);
    ata_outb(ATA_LBA_MID, 0);
    ata_outb(ATA_LBA_HIGH, 0);

    ata_outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    ata_delay_400ns();

    ata_u8 status = ata_inb(ATA_STATUS);

    if (status == 0 || status == 0xFF)
        return ATA_ERR_NO_DEVICE;

    ATAResult result = ata_wait(false);

    if (result != ATA_OK)
        return result;

    // Reject non-ATA devices.
    if (ata_inb(ATA_LBA_MID) != 0 ||
        ata_inb(ATA_LBA_HIGH) != 0)
    {
        return ATA_ERR_NO_DEVICE;
    }

    result = ata_wait_drq();

    if (result != ATA_OK)
        return result;

    // Read IDENTIFY DEVICE data.
    ata_u16 identify[256];

    for (int i = 0; i < 256; i++)
        identify[i] = ata_inw(ATA_DATA);

    // LBA28 sector count: words 60 and 61.
    ata_u32 sectors =
        ((ata_u32)identify[61] << 16) |
        identify[60];

    if (sectors == 0)
        return ATA_ERR;

    ataDevice.detected = true;
    ataDevice.initialized = true;
    ataDevice.sectorCount = sectors;

    return ATA_OK;
}

// ============================================================
// READ ONE SECTOR
// Buffer size must be at least 512 bytes.
// ============================================================

static ATAResult ata_read_sector(
    ata_u32 lba,
    ata_u16 *buffer)
{
    if (buffer == 0)
        return ATA_ERR_INVALID_ARGUMENT;

    if (!ataDevice.initialized)
        return ATA_ERR_NO_DEVICE;

    if (lba >= ataDevice.sectorCount || lba >= 0x10000000)
        return ATA_ERR_LBA_RANGE;

    ATAResult result = ata_wait();

    if (result != ATA_OK)
        return result;

    ata_select_master();

    ata_outb(ATA_SECTOR_COUNT, 1);
    ata_outb(ATA_LBA_LOW, (ata_u8)(lba));
    ata_outb(ATA_LBA_MID, (ata_u8)(lba >> 8));
    ata_outb(ATA_LBA_HIGH, (ata_u8)(lba >> 16));
    ata_outb(ATA_DRIVE, (ata_u8)(0xE0 | ((lba >> 24) & 0x0F)));

    ata_outb(ATA_COMMAND, ATA_CMD_READ);

    ata_delay_400ns();

    result = ata_wait_drq();

    if (result != ATA_OK)
        return result;

    // 256 words = 512 bytes.
    for (int i = 0; i < 256; i++)
        buffer[i] = ata_inw(ATA_DATA);

    return ata_wait();
}

// ============================================================
// WRITE ONE SECTOR
// Buffer size must be at least 512 bytes.
// ============================================================

static ATAResult ata_write_sector(
    ata_u32 lba,
    const ata_u16 *buffer)
{
    if (buffer == 0)
        return ATA_ERR_INVALID_ARGUMENT;

    if (!ataDevice.initialized)
        return ATA_ERR_NO_DEVICE;

    if (lba >= ataDevice.sectorCount || lba >= 0x10000000)
        return ATA_ERR_LBA_RANGE;

    ATAResult result = ata_wait();

    if (result != ATA_OK)
        return result;

    ata_select_master();

    ata_outb(ATA_SECTOR_COUNT, 1);
    ata_outb(ATA_LBA_LOW, (ata_u8)(lba));
    ata_outb(ATA_LBA_MID, (ata_u8)(lba >> 8));
    ata_outb(ATA_LBA_HIGH, (ata_u8)(lba >> 16));
    ata_outb(ATA_DRIVE, (ata_u8)(0xE0 | ((lba >> 24) & 0x0F)));

    ata_outb(ATA_COMMAND, ATA_CMD_WRITE);

    ata_delay_400ns();

    result = ata_wait_drq();

    if (result != ATA_OK)
        return result;

    // 256 words = 512 bytes.
    for (int i = 0; i < 256; i++)
        ata_outw(ATA_DATA, buffer[i]);

    // Wait for write completion.
    result = ata_wait();

    if (result != ATA_OK)
        return result;

    // Flush write cache.
    ata_outb(ATA_COMMAND, ATA_CMD_FLUSH);

    return ata_wait();
}

// ============================================================
// DEVICE STATUS
// ============================================================

static bool ata_is_ready()
{
    return ataDevice.initialized;
}

static ata_u32 ata_get_sector_count()
{
    return ataDevice.sectorCount;
}
