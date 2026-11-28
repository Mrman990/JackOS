#include <stdint.h>
#include "memori_menejeman/kmalloc.hpp"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// ============================================================
// PORT I/O
// ============================================================

static inline u8 inb(u16 port)
{
    u8 r;
    __asm__ __volatile__("inb %1, %0" : "=a"(r) : "Nd"(port));
    return r;
}

static inline void outb(u16 port, u8 val)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

// ============================================================
// STRING HELPER
// ============================================================

static bool strEqual(const char *a, const char *b)
{
    while (*a && *b)
    {
        if (*a != *b)
            return false;

        a++;
        b++;
    }

    return *a == *b;
}

static bool startsWith(const char *s, const char *prefix)
{
    while (*prefix)
    {
        if (*s != *prefix)
            return false;

        s++;
        prefix++;
    }

    return true;
}

// ============================================================
// COLOR
// ============================================================

#define CLR_RED 0x0C
#define CLR_WHITE 0x0F
#define CLR_POLE 0x08
#define CH_BLOCK 0xDB

// ============================================================
// VGA CONSOLE
// ============================================================

class VGAConsole
{
private:
    int row = 0;
    int col = 0;
    u8 color;

    static volatile u8 *vga()
    {
        return (volatile u8 *)0xb8000;
    }

    void updateCursor()
    {
        u16 pos = row * 80 + col;

        outb(0x3D4, 0x0F);
        outb(0x3D5, (u8)(pos & 0xFF));

        outb(0x3D4, 0x0E);
        outb(0x3D5, (u8)((pos >> 8) & 0xFF));
    }

    void scroll()
    {
        for (int r = 1; r < 25; r++)
        {
            for (int c = 0; c < 80 * 2; c++)
            {
                vga()[(r - 1) * 80 * 2 + c] =
                    vga()[r * 80 * 2 + c];
            }
        }

        for (int c = 0; c < 80; c++)
        {
            vga()[24 * 80 * 2 + c * 2] = ' ';
            vga()[24 * 80 * 2 + c * 2 + 1] = 0x07;
        }

        row = 24;
    }

public:
    VGAConsole() : color(0x07) {}

    void setColor(u8 c)
    {
        color = c;
    }

    void clear()
    {
        for (int i = 0; i < 80 * 25 * 2; i += 2)
        {
            vga()[i] = ' ';
            vga()[i + 1] = 0x07;
        }

        row = 0;
        col = 0;

        updateCursor();
    }

    void setCursor(int r, int c)
    {
        row = r;
        col = c;

        updateCursor();
    }

    void writeAt(int r, int c, const char *str, u8 clr)
    {
        if (r < 0 || r >= 25 || c < 0 || c >= 80)
            return;

        int offset = (r * 80 + c) * 2;

        for (int i = 0; str[i] != '\0' && c + i < 80; i++)
        {
            vga()[offset] = str[i];
            vga()[offset + 1] = clr;
            offset += 2;
        }
    }

    void putCell(int r, int c, u8 ch, u8 clr)
    {
        if (r < 0 || r >= 25 || c < 0 || c >= 80)
            return;

        int offset = (r * 80 + c) * 2;

        vga()[offset] = ch;
        vga()[offset + 1] = clr;
    }

    void putChar(char ch)
    {
        if (ch == '\n')
        {
            col = 0;
            row++;
        }
        else if (ch == '\b')
        {
            if (col > 0)
            {
                col--;

                int off = (row * 80 + col) * 2;

                vga()[off] = ' ';
                vga()[off + 1] = color;
            }
        }
        else
        {
            int off = (row * 80 + col) * 2;

            vga()[off] = ch;
            vga()[off + 1] = color;

            col++;

            if (col >= 80)
            {
                col = 0;
                row++;
            }
        }

        if (row >= 25)
            scroll();

        updateCursor();
    }

    void print(const char *str, u8 clr)
    {
        u8 old = color;
        color = clr;

        for (int i = 0; str[i] != '\0'; i++)
            putChar(str[i]);

        color = old;
    }

    void print(const char *str)
    {
        print(str, color);
    }
};

// ============================================================
// KEYBOARD
// ============================================================

static const char scanNormal[58] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};

static const char scanShift[58] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};

class Keyboard
{
private:
    bool shift;
    bool caps;
    bool extended;

public:
    Keyboard() : shift(false), caps(false), extended(false) {}

    bool tryGetChar(char *out)
    {
        if (out == nullptr)
            return false;

        while (inb(0x64) & 1)
        {
            u8 sc = inb(0x60);

            if (sc == 0xE0)
            {
                extended = true;
                continue;
            }

            if (extended)
            {
                extended = false;
                continue;
            }

            if (sc == 0x2A || sc == 0x36)
            {
                shift = true;
                continue;
            }

            if (sc == 0xAA || sc == 0xB6)
            {
                shift = false;
                continue;
            }

            if (sc == 0x3A)
            {
                caps = !caps;
                continue;
            }

            if (sc & 0x80)
                continue;

            if (sc >= 58)
                continue;

            char ch = shift ? scanShift[sc] : scanNormal[sc];

            if (caps)
            {
                if (ch >= 'a' && ch <= 'z')
                    ch = ch - 'a' + 'A';
                else if (ch >= 'A' && ch <= 'Z')
                    ch = ch - 'A' + 'a';
            }

            if (ch != 0)
            {
                *out = ch;
                return true;
            }
        }

        return false;
    }

    char getChar()
    {
        char ch;

        while (!tryGetChar(&ch))
        {
        }

        return ch;
    }
};

// ============================================================
// GLOBAL OBJECT
// ============================================================

static VGAConsole console;
static Keyboard keyboard;

enum StorageMonitorState
{
    STORAGE_MONITOR_TERMINATED,
    STORAGE_MONITOR_READY,
    STORAGE_MONITOR_RUNNING
};

static StorageMonitorState storageMonitorState =
    STORAGE_MONITOR_TERMINATED;

static void componentMonitorRefresh();
static void componentMonitorVisualDelay();
// ============================================================
// KERNEL MODULES
// ============================================================

#include "process/process.h"
#include "process/mutex.h"

#include "Protection/protection.h"
#include "ramdisk/ramdisk.h"
#include "driver/ata.h"
#include "ramdisk/storage.h"
#include "Protection/syscall.h"
#include "ramdisk/ramdisk_cmd.h"

static void printResult(const char *message, bool success);

#include "Protection/protection_cmd.h"
#include "driver/driver.h"
#include "driver/sys_io.h"
#include "aplikasi/stok_app.h"

static void componentMonitorWriteStatus(int row, const char *status)
{
    // Bersihkan area status terlebih dahulu.
    for (int col = 62; col < 79; col++)
    {
        console.putCell(row, col, ' ', CLR_WHITE);
    }

    console.writeAt(row, 62, status, CLR_WHITE);
}

static void componentMonitorRefresh()
{
    // Status Kernel
    console.writeAt(3, 42, "KERNEL", CLR_WHITE);
    componentMonitorWriteStatus(3, "RUNNING");

    // Status Inventory
    console.writeAt(4, 42, "INVENTORY", CLR_WHITE);

    if (stokAppAktif)
        componentMonitorWriteStatus(4, "RUNNING");
    else
        componentMonitorWriteStatus(4, "TERMINATED");

    // Status Storage
    console.writeAt(5, 42, "STORAGE", CLR_WHITE);

    switch (storageMonitorState)
    {
    case STORAGE_MONITOR_TERMINATED:
        componentMonitorWriteStatus(5, "TERMINATED");
        break;

    case STORAGE_MONITOR_READY:
        componentMonitorWriteStatus(5, "READY");
        break;

    case STORAGE_MONITOR_RUNNING:
        componentMonitorWriteStatus(5, "RUNNING");
        break;
    }
}
static void componentMonitorVisualDelay()
{
    volatile unsigned int i;

    for (i = 0; i < 100000000U; i++)
    {
        // Visual delay
    }
}
// ============================================================
// PRINT NUMBER
// ============================================================

static void printNum(u32 v, u8 clr)
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

    console.print(&buf[i], clr);
}

static void printResult(const char *message, bool success)
{
    console.print(message, CLR_WHITE);

    console.print(
        success ? " : OK\n" : " : GAGAL\n",
        success ? CLR_WHITE : CLR_RED);
}

// ============================================================
// THREAD AND SYNCHRONIZATION LOGGER
// ============================================================

static bool syncMonitorAktif = false;
static int syncLogRow = 15;
static int threadLogRow = 3;

static void writeNumberAt(int row, int col, int number, u8 clr);

static void logThreadState(
    const thread_t *t,
    thread_state_t from,
    thread_state_t to)
{
    if (t == nullptr)
        return;

    if (threadLogRow >= 12)
        threadLogRow = 3;

    for (int c = 42; c < 80; c++)
        console.putCell(threadLogRow, c, ' ', CLR_WHITE);

    console.writeAt(threadLogRow, 42, "T", CLR_WHITE);
    writeNumberAt(threadLogRow, 43, t->tid, CLR_WHITE);
    console.writeAt(threadLogRow, 47, t->name, CLR_WHITE);

    if (from == to)
    {
        console.writeAt(
            threadLogRow, 68,
            thread_state_name(to), CLR_WHITE);
    }
    else
    {
        console.writeAt(
            threadLogRow, 60,
            thread_state_name(from), CLR_WHITE);

        console.writeAt(threadLogRow, 68, ">", CLR_WHITE);

        console.writeAt(
            threadLogRow, 70,
            thread_state_name(to), CLR_WHITE);
    }

    threadLogRow++;
}

static void writeNumberAt(int row, int col, int number, u8 clr)
{
    char digits[10];
    int count = 0;

    if (number == 0)
    {
        console.putCell(row, col, '0', clr);
        return;
    }

    while (number > 0 && count < 10)
    {
        digits[count++] = '0' + (number % 10);
        number /= 10;
    }

    for (int i = 0; i < count; i++)
    {
        console.putCell(
            row,
            col + i,
            digits[count - 1 - i],
            clr);
    }
}

static void logSyncThreadState(
    const thread_t *t,
    thread_state_t from,
    thread_state_t to)
{
    if (!syncMonitorAktif || syncLogRow >= 24)
        return;

    for (int c = 42; c < 80; c++)
        console.putCell(syncLogRow, c, ' ', CLR_WHITE);

    console.writeAt(syncLogRow, 42, "T", CLR_WHITE);
    writeNumberAt(syncLogRow, 43, t->tid, CLR_WHITE);
    console.writeAt(syncLogRow, 47, t->name, CLR_WHITE);

    if (from == to)
    {
        console.writeAt(
            syncLogRow, 70,
            thread_state_name(to), CLR_WHITE);
    }
    else
    {
        console.writeAt(
            syncLogRow, 60,
            thread_state_name(from), CLR_WHITE);

        console.putCell(syncLogRow, 68, '>', CLR_WHITE);

        console.writeAt(
            syncLogRow, 70,
            thread_state_name(to), CLR_WHITE);
    }

    syncLogRow++;
}

static void logMutexEvent(mutex_event_t event)
{
    if (!syncMonitorAktif || syncLogRow >= 24)
        return;

    for (int c = 42; c < 80; c++)
        console.putCell(syncLogRow, c, ' ', CLR_WHITE);

    console.writeAt(syncLogRow, 42, "[MUTEX]", CLR_RED);

    if (event == MUTEX_EVENT_WAIT)
        console.writeAt(syncLogRow, 51, "WAIT", CLR_WHITE);
    else if (event == MUTEX_EVENT_LOCK)
        console.writeAt(syncLogRow, 51, "LOCK", CLR_WHITE);
    else if (event == MUTEX_EVENT_UNLOCK)
        console.writeAt(syncLogRow, 51, "UNLOCK", CLR_WHITE);

    syncLogRow++;
}

// ============================================================
// MEMORY MANAGEMENT
// ============================================================

static void showMem()
{
    u32 total = (u32)mem::total();
    u32 used = (u32)mem::used();
    u32 avail = (total >= used) ? (total - used) : 0;
    u32 pct = (total > 0) ? (used * 100) / total : 0;

    console.print("\n=== JACKOS RAM MONITOR ===\n", CLR_RED);

    int filled = (int)((pct * 20) / 100);
    if (used > 0 && filled == 0)
        filled = 1;
    console.print("Penggunaan: [", CLR_WHITE);
    for (int i = 0; i < 20; i++)
        console.print(i < filled ? "#" : "-", i < filled ? CLR_RED : CLR_WHITE);
    console.print("] ", CLR_WHITE);
    printNum(pct, CLR_WHITE);
    console.print("%\n", CLR_WHITE);

    console.print("Total Heap      : ", CLR_WHITE);
    printNum(total, CLR_WHITE);
    console.print(" bytes\n", CLR_WHITE);

    console.print("Memori Terpakai : ", CLR_WHITE);
    printNum(used, CLR_WHITE);
    console.print(" bytes\n", CLR_WHITE);

    console.print("Memori Tersedia : ", CLR_WHITE);
    printNum(avail, CLR_WHITE);
    console.print(" bytes\n", CLR_WHITE);

    console.print("Objek Barang    : ", CLR_WHITE);
    printNum(ramdisk.count(), CLR_WHITE);
    console.print(" item aktif\n", CLR_WHITE);
    console.print("==========================\n\n", CLR_RED);
}

static void runMemTest()
{
    console.print("=== MEMORY ALLOCATION TEST ===\n", CLR_RED);

    void *data = mem::alloc(1024);

    if (data != nullptr)
    {
        console.print("Alokasi 1024 byte berhasil.\n", CLR_WHITE);

        mem::free(data);

        console.print("Memori berhasil dibebaskan.\n", CLR_WHITE);
    }
    else
    {
        console.print("Alokasi memori gagal.\n", CLR_RED);
    }
}

// ============================================================
// DEMO KERNEL THREAD
// ============================================================

static void taskData()
{
    console.print("  [Data] menghitung stok barang...\n", CLR_WHITE);

    thread_sleep(2);

    console.print("  [Data] stok selesai dihitung.\n", CLR_WHITE);
}

static void taskMonitor()
{
    for (int i = 1; i <= 2; i++)
    {
        console.print("  [Monitor] cek sistem ke-", CLR_WHITE);
        printNum((u32)i, CLR_WHITE);
        console.print("\n", CLR_WHITE);

        yield();
    }
}

static void runDemo()
{
    console.print("=== DEMO KERNEL THREAD ===\n", CLR_RED);

    if (!create_thread("Data", taskData) ||
        !create_thread("Monitor", taskMonitor))
    {
        console.print(
            "Thread penuh (maksimal 10 thread).\n",
            CLR_RED);
        return;
    }

    scheduler_run();

    console.print("=== DEMO SELESAI ===\n", CLR_RED);
}
// ============================================================
// THREAD INFORMATION - FUNGSI PEMBANTU
// ============================================================

// Membandingkan dua nama thread.
static bool threadInfoNameEquals(const char *a, const char *b)
{
    if (a == nullptr || b == nullptr)
        return false;

    int i = 0;

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i])
            return false;

        i++;
    }

    return a[i] == '\0' && b[i] == '\0';
}

// Mengambil status thread berdasarkan namanya.
static const char *threadInfoStatusOf(const char *name)
{
    for (int i = 0; i < thread_count(); i++)
    {
        thread_t *t = thread_at(i);

        if (t == nullptr || !t->in_use)
            continue;

        if (threadInfoNameEquals(t->name, name))
            return thread_state_name(t->state);
    }

    return "TERMINATED";
}

// Menampilkan satu baris informasi thread.
static void printThreadInfoRow(
    u32 id,
    const char *name,
    const char *status)
{
    printNum(id, CLR_WHITE);
    console.print("    ", CLR_WHITE);

    // Nama thread
    console.print(name, CLR_WHITE);

    // Menyesuaikan posisi kolom STATUS
    int nameLength = 0;

    while (name[nameLength] != '\0')
        nameLength++;

    int spaces = 18 - nameLength;

    if (spaces < 1)
        spaces = 1;

    for (int i = 0; i < spaces; i++)
        console.print(" ", CLR_WHITE);

    // Status thread
    console.print(status, CLR_WHITE);
    console.print("\n", CLR_WHITE);
}

// ============================================================
// SHOW THREADS
// ============================================================

static void showThreads()
{
    console.print("ID   NAMA THREAD       STATUS\n", CLR_WHITE);
    console.print("-----------------------------------------\n", CLR_WHITE);

    u32 id = 1;

    // ========================================================
    // KERNEL
    // ========================================================

    printThreadInfoRow(
        id++,
        "KERNEL",
        "RUNNING");

    // ========================================================
    // INVENTORY
    // ========================================================

    printThreadInfoRow(
        id++,
        "INVENTORY",
        stokAppAktif ? "RUNNING" : "TERMINATED");

    // ========================================================
    // STORAGE
    // ========================================================

    const char *storageStatus = "TERMINATED";

    switch (storageMonitorState)
    {
    case STORAGE_MONITOR_TERMINATED:
        storageStatus = "TERMINATED";
        break;

    case STORAGE_MONITOR_READY:
        storageStatus = "READY";
        break;

    case STORAGE_MONITOR_RUNNING:
        storageStatus = "RUNNING";
        break;
    }

    printThreadInfoRow(
        id++,
        "STORAGE",
        storageStatus);

    // ========================================================
    // PENJUALAN
    // ========================================================

    printThreadInfoRow(
        id++,
        "Penjualan",
        threadInfoStatusOf("Penjualan"));

    // ========================================================
    // RESTOCK
    // ========================================================

    printThreadInfoRow(
        id++,
        "Restock",
        threadInfoStatusOf("Restock"));

    for (int i = 0; i < thread_count(); i++)
    {
        thread_t *t = thread_at(i);

        if (t == nullptr || !t->in_use)
            continue;

        // Thread yang sudah ditampilkan tidak perlu diulang.
        if (threadInfoNameEquals(t->name, "INVENTORY") ||
            threadInfoNameEquals(t->name, "Stok App") ||
            threadInfoNameEquals(t->name, "Penjualan") ||
            threadInfoNameEquals(t->name, "Restock"))
        {
            continue;
        }

        printThreadInfoRow(
            id++,
            t->name,
            thread_state_name(t->state));
    }

    console.print("-----------------------------------------\n", CLR_WHITE);
}
// ============================================================
// MUTEX DEMO
// ============================================================

static int stokBarang = 10;
static mutex_t stockMutex;

static void taskPenjualan()
{
    mutex_lock(&stockMutex);

    console.print("[Penjualan] Mengakses data stok...\n", CLR_WHITE);

    int stokSementara = stokBarang;
    yield();

    stokBarang = stokSementara - 2;

    console.print("[Penjualan] Berhasil mengurangi stok.\n", CLR_WHITE);

    mutex_unlock(&stockMutex);
}

static void taskRestock()
{
    mutex_lock(&stockMutex);

    console.print("[Restock] Mengakses data stok...\n", CLR_WHITE);

    int stokSementara = stokBarang;
    yield();

    stokBarang = stokSementara + 5;

    console.print("[Restock] Berhasil menambah stok.\n", CLR_WHITE);

    mutex_unlock(&stockMutex);
}

// ============================================================
// SYNCHRONIZATION TEST
// ============================================================

static int syncStok = 0;
static int syncPenjualan = 0;
static int syncRestock = 0;
static bool syncGunakanMutex = false;
static mutex_t syncMutex;

static int inputAngka(const char *pesan)
{
    console.print(pesan, CLR_WHITE);

    int angka = 0;
    int jumlahDigit = 0;

    while (true)
    {
        char ch = keyboard.getChar();

        if (ch == '\n')
            break;

        if (ch >= '0' && ch <= '9' && jumlahDigit < 5)
        {
            angka = angka * 10 + (ch - '0');
            jumlahDigit++;

            console.putChar(ch);
        }
    }

    console.print("\n", CLR_WHITE);

    return angka;
}

static void prosesSync(bool penjualan)
{
    if (syncGunakanMutex)
        mutex_lock(&syncMutex);

    if (penjualan)
        console.print("[Thread Penjualan] Mengakses stok...\n", CLR_WHITE);
    else
        console.print("[Thread Restock] Mengakses stok...\n", CLR_WHITE);

    int stokSementara = syncStok;

    yield();

    if (penjualan)
        syncStok = stokSementara - syncPenjualan;
    else
        syncStok = stokSementara + syncRestock;

    if (penjualan)
        console.print("[Thread Penjualan] Selesai.\n", CLR_WHITE);
    else
        console.print("[Thread Restock] Selesai.\n", CLR_WHITE);

    if (syncGunakanMutex)
        mutex_unlock(&syncMutex);
}

static void taskSyncPenjualan()
{
    prosesSync(true);
}

static void taskSyncRestock()
{
    prosesSync(false);
}

static int jalankanSyncTest(
    int stokAwal,
    int penjualan,
    int restock,
    bool gunakanMutex)
{
    syncStok = stokAwal;
    syncPenjualan = penjualan;
    syncRestock = restock;
    syncGunakanMutex = gunakanMutex;

    if (gunakanMutex)
        mutex_init(&syncMutex);

    if (gunakanMutex)
        console.print("\n=== PENGUJIAN DENGAN MUTEX ===\n", CLR_RED);
    else
        console.print("\n=== PENGUJIAN TANPA MUTEX ===\n", CLR_RED);

    if (!create_thread("Sync-Penjualan", taskSyncPenjualan))
    {
        console.print("Gagal membuat thread Penjualan.\n", CLR_RED);
        return -1;
    }

    if (!create_thread("Sync-Restock", taskSyncRestock))
    {
        console.print("Gagal membuat thread Restock.\n", CLR_RED);
        scheduler_run();
        return -1;
    }

    scheduler_run();

    console.print("Stok akhir: ", CLR_WHITE);
    printNum((u32)syncStok, CLR_WHITE);
    console.print("\n", CLR_WHITE);

    return syncStok;
}

static void runSynchronizationTest()
{
    console.clear();

    for (int r = 0; r < 25; r++)
        console.putCell(r, 39, '|', CLR_RED);

    console.writeAt(0, 42, "THREAD MONITOR", CLR_RED);
    console.writeAt(1, 42, "ID  NAMA THREAD    STATUS", CLR_WHITE);
    console.writeAt(0, 0, "SYNCHRONIZATION TEST", CLR_RED);

    console.setCursor(3, 0);

    int stokAwal = inputAngka("Stok awal        : ");
    int penjualan = inputAngka("Jumlah penjualan : ");
    int restock = inputAngka("Jumlah restock   : ");

    if (penjualan > stokAwal)
    {
        console.print("\nPenjualan melebihi stok awal!\n", CLR_RED);
        return;
    }

    syncLogRow = 15;
    syncMonitorAktif = true;

    // Aktifkan pencatatan perubahan status thread.
    scheduler_set_logger(logSyncThreadState);

    console.print("\n=== TANPA MUTEX ===\n", CLR_RED);

    int hasilTanpaMutex =
        jalankanSyncTest(stokAwal, penjualan, restock, false);

    console.print("\n=== DENGAN MUTEX ===\n", CLR_RED);

    int hasilDenganMutex =
        jalankanSyncTest(stokAwal, penjualan, restock, true);

    // Matikan monitor setelah pengujian.
    syncMonitorAktif = false;
    scheduler_set_logger(0);

    if (hasilTanpaMutex < 0 || hasilDenganMutex < 0)
    {
        console.print("\nPengujian tidak selesai.\n", CLR_RED);
        return;
    }

    console.print("\n=== PERBANDINGAN ===\n", CLR_RED);

    console.print("Stok awal    : ", CLR_WHITE);
    printNum((u32)stokAwal, CLR_WHITE);

    console.print("\nTanpa Mutex  : ", CLR_WHITE);
    printNum((u32)hasilTanpaMutex, CLR_WHITE);

    console.print("\nDengan Mutex : ", CLR_WHITE);
    printNum((u32)hasilDenganMutex, CLR_WHITE);

    console.print("\n========================\n", CLR_RED);
}

// ============================================================
// COMMAND LINE
// ============================================================

static const char *PROMPT = "JACK_OS:> ";

static void runCommand(const char *cmd)
{
    if (cmd[0] == '\0')
        return;

    if (strEqual(cmd, "help") || strEqual(cmd, "mulai"))
    {
        console.print("Perintah yang tersedia:\n", CLR_RED);
        console.print("  help          - tampilkan bantuan ini\n");
        console.print("  clear         - bersihkan layar\n");
        console.print("  info          - informasi JACK_OS\n");
        console.print("  inventory     - jalankan thread inventory management\n");
        console.print("  demo          - demo 2 thread + perubahan status\n");
        console.print("  threads       - tampilkan daftar thread & status\n");
        console.print("  echo <teks>   - cetak teks\n");
        console.print("  reboot        - restart sistem\n");
    }
    else if (strEqual(cmd, "inventory"))
    {
        console.print("=== SIMULASI INVENTORY MANAGEMENT ===\n", CLR_RED);

        stokBarang = 10;
        mutex_init(&stockMutex);

        if (!create_thread("Penjualan", taskPenjualan) ||
            !create_thread("Restock", taskRestock))
        {
            console.print("Thread penuh (maksimal 10 thread).\n", CLR_RED);
            return;
        }

        scheduler_run();

        console.print("Stok akhir: ", CLR_WHITE);
        printNum((u32)stokBarang, CLR_WHITE);
        console.print("\n=== SIMULASI SELESAI ===\n", CLR_RED);
    }
    else if (strEqual(cmd, "demo"))
    {
        runDemo();
    }
    else if (strEqual(cmd, "threads"))
    {
        showThreads();
    }
    else if (strEqual(cmd, "clear"))
    {
        console.clear();
    }
    else if (strEqual(cmd, "info"))
    {
        console.print("JACK_OS v1.2 (Multithreaded)\n", CLR_RED);
        console.print("  Arsitektur : x86 32-bit (i686)\n");
        console.print("  Bahasa     : C++ freestanding + Assembly\n");
        console.print("  Fitur      : VGA, Keyboard, Thread & Scheduler\n");
    }
    else if (startsWith(cmd, "echo "))
    {
        console.print(cmd + 5);
        console.print("\n");
    }
    else if (strEqual(cmd, "reboot"))
    {
        console.print("Restarting...\n", CLR_RED);
        outb(0x64, 0xFE);

        while (true)
        {
        }
    }
    else
    {
        console.print("Perintah tidak dikenal: ", CLR_RED);
        console.print(cmd, CLR_RED);
        console.print("\n");
    }
}

// ============================================================
// MENU INPUT
// ============================================================
static char getMenuChoice()
{
    char choice = keyboard.getChar();

    console.putChar(choice);
    console.putChar('\n');

    return choice;
}
static bool kernelPassword()
{
    const char PASSWORD[] = "jack123";

    char input[32];
    int length = 0;

    console.clear();

    console.writeAt(
        3, 25,
        "====== KERNEL MODE SECURITY ======",
        CLR_RED);

    console.writeAt(
        7, 25,
        "Masukkan password untuk akses Kernel Mode:",
        CLR_WHITE);

    console.setCursor(9, 25);
    console.print("Password: ", CLR_WHITE);

    while (true)
    {
        char ch = keyboard.getChar();

        if (ch == '\n')
        {
            input[length] = '\0';
            break;
        }

        // Hapus karakter terakhir
        if (ch == '\b')
        {
            if (length > 0)
            {
                length--;
                input[length] = '\0';
                console.putChar('\b');
            }

            continue;
        }

        if (ch >= 32 && ch <= 126 && length < 31)
        {
            input[length] = ch;
            length++;
            console.putChar('*');
        }
    }

    console.print("\n\n", CLR_WHITE);

    if (strEqual(input, PASSWORD))
    {
        console.print(
            "Password benar! Akses diterima.\n",
            CLR_WHITE);

        console.print(
            "Tekan tombol apa saja untuk melanjutkan...",
            CLR_WHITE);

        keyboard.getChar();

        return true;
    }

    console.print(
        "Password salah! Akses ditolak.\n",
        CLR_RED);

    console.print(
        "Kembali ke Main Menu...",
        CLR_WHITE);

    keyboard.getChar();

    return false;
}

// ============================================================
// INVENTORY DASHBOARD
// ============================================================
static void showStokDashboard()
{
    console.clear();

    for (int r = 0; r < 25; r++)
        console.putCell(r, 39, '|', CLR_RED);

    console.writeAt(0, 2, "JACK OS - INVENTORY", CLR_RED);
    console.writeAt(1, 2, "==================", CLR_WHITE);

    console.writeAt(0, 42, "THREAD MONITOR", CLR_RED);
    console.writeAt(2, 42, "NAMA THREAD", CLR_WHITE);
    console.writeAt(2, 62, "STATUS", CLR_WHITE);

    threadLogRow = 3;
    syncLogRow = 15;

    componentMonitorRefresh();

    console.setCursor(2, 2);
}

// ============================================================
// BANNER JACK OS - FONT
// ============================================================

// Pola huruf 5 x 5 untuk tulisan JACK OS.
// Urutan: J, A, C, K, spasi, O, S.

static const char FONT[7][5][6] =
    {
        {"#####",
         "   # ",
         "   # ",
         "#  # ",
         " ##  "},
        {" ### ",
         "#   #",
         "#####",
         "#   #",
         "#   #"},
        {" ####",
         "#    ",
         "#    ",
         "#    ",
         " ####"},
        {"#   #",
         "#  # ",
         "###  ",
         "#  # ",
         "#   #"},
        {"     ",
         "     ",
         "     ",
         "     ",
         "     "},
        {" ### ",
         "#   #",
         "#   #",
         "#   #",
         " ### "},
        {" ####",
         "#    ",
         " ### ",
         "    #",
         "#### "}};

static void drawFlag(int top, int left)
{
    static const int wave[8] =
        {
            0, 1, 1, 1, 0, -1, -1, -1};

    // Tiang bendera
    for (int r = top - 1; r <= top + 9; r++)
    {
        console.putCell(r, left - 1, CH_BLOCK, CLR_POLE);
    }

    // Bendera Merah Putih
    for (int x = 0; x < 20; x++)
    {
        for (int b = 0; b < 8; b++)
        {
            u8 clr = (b < 4) ? CLR_RED : CLR_WHITE;

            console.putCell(
                top + b + wave[x % 8],
                left + x,
                CH_BLOCK,
                clr);
        }
    }
}

// ============================================================
// DRAW GLYPH
// ============================================================

static void drawGlyph(int glyph, int top, int left)
{
    if (glyph < 0 || glyph >= 7)
        return;

    for (int r = 0; r < 5; r++)
    {
        u8 clr = (r < 2) ? CLR_RED : CLR_WHITE;

        for (int c = 0; c < 5; c++)
        {
            if (FONT[glyph][r][c] == '#')
            {
                console.putCell(
                    top + r,
                    left + c,
                    CH_BLOCK,
                    clr);
            }
        }
    }
}

// ============================================================
// DRAW TITLE
// ============================================================

static void drawTitle(int top, int left)
{
    for (int glyph = 0; glyph < 7; glyph++)
    {
        drawGlyph(
            glyph,
            top,
            left + glyph * 6);
    }
}

// ============================================================
// SHOW BANNER
// ============================================================

static void showBanner()
{
    console.clear();

    // Bendera Merah Putih
    drawFlag(1, 3);

    // Tulisan blok JACK OS
    drawTitle(4, 25);

    // Menu utama
    console.writeAt(10, 28, "[1] User Mode", CLR_WHITE);
    console.writeAt(11, 28, "[2] Kernel Mode", CLR_WHITE);
    console.writeAt(12, 28, "[3] Clear", CLR_WHITE);
    console.writeAt(13, 28, "[0] Reboot", CLR_WHITE);

    console.writeAt(19, 28, "Pilih menu: ", CLR_WHITE);

    console.setCursor(19, 40);
}

// ============================================================
// USER MODE MENU
// ============================================================

static void userModeMenu()
{
    while (true)
    {
        console.clear();

        console.writeAt(
            3, 25,
            "========== USER MODE ==========",
            CLR_RED);

        console.writeAt(7, 28, "[1] Inventory Management", CLR_WHITE);
        console.writeAt(8, 28, "[0] Back to Main Menu", CLR_WHITE);

        console.writeAt(12, 28, "Pilih menu: ", CLR_WHITE);
        console.setCursor(12, 40);

        char choice = getMenuChoice();

        if (choice == '1')
        {
            showStokDashboard();

            thread_t *stokThread =
                create_thread("Stok App", taskStokApp);

            if (stokThread != nullptr)
            {
                // create_thread() sudah memasukkan thread
                // ke Ready Queue. Tidak perlu scheduler_add().
                scheduler_run();
            }
            else
            {
                console.print(
                    "Gagal membuat thread aplikasi stok.\n",
                    CLR_RED);
            }

            console.print("\nTekan tombol apa saja untuk kembali...");
            keyboard.getChar();
        }
        else if (choice == '0')
        {
            return;
        }
        else
        {
            console.print("Pilihan tidak tersedia.\n", CLR_RED);
            keyboard.getChar();
        }
    }
}

// ============================================================
// PROCESS & THREAD MENU
// ============================================================

static void processThreadMenu()
{
    while (true)
    {
        console.clear();

        console.print("========== PROCESS & THREAD ==========\n", CLR_RED);
        console.print("[1] Thread Information\n", CLR_WHITE);
        console.print("[2] Thread Demo\n", CLR_WHITE);
        console.print("[0] Back to Kernel Mode\n", CLR_WHITE);

        console.print("\nPilih menu: ", CLR_WHITE);

        char choice = getMenuChoice();

        if (choice == '1')
        {
            console.clear();

            console.print(
                "========== THREAD INFORMATION ==========\n",
                CLR_RED);

            showThreads();

            console.print("\nTekan tombol apa saja untuk kembali...");
            keyboard.getChar();
        }
        else if (choice == '2')
        {
            console.clear();

            runDemo();

            console.print("\nTekan tombol apa saja untuk kembali...");
            keyboard.getChar();
        }
        else if (choice == '0')
        {
            return;
        }
        else
        {
            console.print("Pilihan tidak tersedia.\n", CLR_RED);
            keyboard.getChar();
        }
    }
}

// ============================================================
// SYSTEM & TESTING MENU
// ============================================================

static void systemTestingMenu()
{
    while (true)
    {
        console.clear();

        console.print("========== SYSTEM & TESTING ==========\n", CLR_RED);
        console.print("[1] Informasi Sistem\n", CLR_WHITE);
        console.print("[2] Memory Management\n", CLR_WHITE);
        console.print("[3] Synchronization Test\n", CLR_WHITE);
        console.print("[0] Back to Kernel Mode\n", CLR_WHITE);

        console.print("\nPilih menu: ", CLR_WHITE);

        char choice = getMenuChoice();

        if (choice == '1')
        {
            console.clear();

            console.print(
                "========== SYSTEM INFORMATION ==========\n",
                CLR_RED);

            console.print("JACK OS v1.2\n", CLR_WHITE);
            console.print("Arsitektur : x86 32-bit\n", CLR_WHITE);
            console.print("Bahasa     : C++ freestanding + Assembly\n", CLR_WHITE);
            console.print("Fitur      : VGA, Keyboard, Thread, Scheduler\n", CLR_WHITE);

            console.print("\nTekan tombol apa saja untuk kembali...");
            keyboard.getChar();
        }
        else if (choice == '2')
        {
            console.clear();

            showMem();

            console.print("\nTekan tombol apa saja untuk kembali...");
            keyboard.getChar();
        }
        else if (choice == '3')
        {
            runSynchronizationTest();

            console.print("\nTekan tombol apa saja untuk kembali...");
            keyboard.getChar();
        }
        else if (choice == '0')
        {
            return;
        }
        else
        {
            console.print("Pilihan tidak tersedia.\n", CLR_RED);
            keyboard.getChar();
        }
    }
}

// ============================================================
// KERNEL MODE MENU
// ============================================================

static void kernelModeMenu()
{
    while (true)
    {
        console.clear();

        console.writeAt(
            3, 25,
            "========== KERNEL MODE ==========",
            CLR_RED);

        console.writeAt(7, 28, "[1] Process & Thread", CLR_WHITE);
        console.writeAt(8, 28, "[2] System & Testing", CLR_WHITE);
        console.writeAt(9, 28, "[0] Back to Main Menu", CLR_WHITE);

        console.writeAt(13, 28, "Pilih menu: ", CLR_WHITE);
        console.setCursor(13, 40);

        char choice = getMenuChoice();

        if (choice == '1')
        {
            processThreadMenu();
        }
        else if (choice == '2')
        {
            systemTestingMenu();
        }
        else if (choice == '0')
        {
            return;
        }
        else
        {
            console.print("Pilihan tidak tersedia.\n", CLR_RED);
            keyboard.getChar();
        }
    }
}

// ============================================================
// MAIN MENU
// ============================================================

static void mainMenu()
{
    while (true)
    {
        showBanner();

        char choice = getMenuChoice();

        if (choice == '1')
        {
            userModeMenu();
        }
        else if (choice == '2')
        {
            if (kernelPassword())
            {
                kernelModeMenu();
            }
        }
        else if (choice == '3')
        {
            console.clear();
        }
        else if (choice == '0')
        {
            console.clear();

            console.print("Restarting JACK OS...\n", CLR_RED);

            outb(0x64, 0xFE);

            while (true)
            {
            }
        }
        else
        {
            console.print("Pilihan tidak tersedia.\n", CLR_RED);
            keyboard.getChar();
        }
    }
}

// ============================================================
// KERNEL ENTRY POINT
// ============================================================
static void testATA()
{
    console.print("\n=== ATA PIO TEST ===\n");

    ATAResult result = ata_init();

    if (result != ATA_OK)
    {
        console.print("[ATA] Gagal mendeteksi disk!\n");
        return;
    }

    console.print("[ATA] Disk berhasil terdeteksi!\n");

    console.print("[ATA] Jumlah sektor: ");
    ramdiskPrintNum(ata_get_sector_count(), 0);
    console.print("\n");

    static ata_u16 buffer[256];

    result = ata_read_sector(0, buffer);

    if (result != ATA_OK)
    {
        console.print("[ATA] Gagal membaca sektor 0!\n");
        return;
    }

    console.print("[ATA] Berhasil membaca sektor 0!\n");
}

extern "C" void kmain(void)
{
    console.setColor(CLR_WHITE);
    mem::init();
    ramdisk.init();

    scheduler_init();
    mutex_set_logger(logMutexEvent);
    testATA();
    StorageResult initResult = storage_init();

    if (initResult == STORAGE_OK)
    {
        console.print("\n[STORAGE] Memeriksa data disk...\n", CLR_WHITE);
        StorageResult loadResult = storage_load();

        if (loadResult == STORAGE_OK)
        {
            console.print("Data berhasil dimuat!\n", CLR_WHITE);
        }
    }
    else
    {
        console.print("Disk tidak tersedia!\n", CLR_RED);
    }
    console.print(
        "\nTekan tombol apa saja untuk masuk ke Main Menu...",
        CLR_WHITE);

    keyboard.getChar();

    mainMenu();
}
