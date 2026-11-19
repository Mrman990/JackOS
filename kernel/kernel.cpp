#include <stdint.h>
#include "memori_menejeman/kmalloc.hpp"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

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

#define CLR_RED 0x0C
#define CLR_WHITE 0x0F
#define CLR_POLE 0x08
#define CH_BLOCK 0xDB

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
        int offset = (r * 80 + c) * 2;

        for (int i = 0; str[i] != '\0'; i++)
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

static VGAConsole console;
static Keyboard keyboard;

#include "process/process.h"
#include "process/mutex.h"

#include "Protection/protection.h"
#include "ramdisk/ramdisk.h"
#include "Protection/syscall.h"
#include "ramdisk/ramdisk_cmd.h"

static void printResult(const char *message, bool success);

#include "Protection/protection_cmd.h"
#include "driver/driver.h"
#include "driver/sys_io.h"
#include "aplikasi/stok_app.h"

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


static bool syncMonitorAktif = false;
static int syncLogRow = 15;
static int threadLogRow = 3;
static void writeNumberAt(int row, int col, int number, u8 clr);

// Dipanggil scheduler setiap status thread berubah.

static void logThreadState(
    const thread_t *t,
    thread_state_t from,
    thread_state_t to)
{
    if (t == nullptr)
        return;

    // Batasi monitor pada area kanan atas.
    if (threadLogRow >= 12)
        threadLogRow = 3;

    // Bersihkan baris sebelumnya.
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
        console.putCell(row, col + i, digits[count - 1 - i], clr);
}



static void logSyncThreadState(
    const thread_t *t,
    thread_state_t from,
    thread_state_t to)
{
    if (!syncMonitorAktif || syncLogRow >= 24)
        return;

    // Bersihkan baris sebelum menulis log baru.
    for (int c = 42; c < 80; c++)
        console.putCell(syncLogRow, c, ' ', CLR_WHITE);

    // ID dan nama thread.
    console.writeAt(syncLogRow, 42, "T", CLR_WHITE);
    writeNumberAt(syncLogRow, 43, t->tid, CLR_WHITE);
    console.writeAt(syncLogRow, 47, t->name, CLR_WHITE);

    // Status thread.
    if (from == to)
    {
        console.writeAt(
            syncLogRow, 70,
            thread_state_name(to), CLR_WHITE
        );
    }
    else
    {
        console.writeAt(
            syncLogRow, 60,
            thread_state_name(from), CLR_WHITE
        );

        console.putCell(syncLogRow, 68, '>', CLR_WHITE);

        console.writeAt(
            syncLogRow, 70,
            thread_state_name(to), CLR_WHITE
        );
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


static void showMem()
{
    console.print("=== MEMORY MANAGEMENT ===\n", CLR_RED);

    console.print("Total Heap : ", CLR_WHITE);
    printNum((u32)mem::total(), CLR_WHITE);

    console.print("\nMemori Terpakai : ", CLR_WHITE);
    printNum((u32)mem::used(), CLR_WHITE);

    console.print("\nMemori Tersedia : ", CLR_WHITE);
    printNum((u32)(mem::total() - mem::used()), CLR_WHITE);

    console.print("\n");
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

// Tugas 1: menghitung stok, lalu menunggu sebelum selesai.
static void taskData()
{
    console.print("  [Data] menghitung stok barang...\n", CLR_WHITE);

    thread_sleep(2);

    console.print("  [Data] stok selesai dihitung.\n", CLR_WHITE);
}

// Tugas 2: memeriksa sistem, melepas giliran di tiap pemeriksaan.
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

static const char *PROMPT = "JACK_OS:> ";

static const char *const FONT[7][5] = {
    {"#####", "   # ", "   # ", "#  # ", " ##  "},
    {" ### ", "#   #", "#####", "#   #", "#   #"},
    {" ####", "#    ", "#    ", "#    ", " ####"},
    {"#   #", "#  # ", "###  ", "#  # ", "#   #"},
    {"     ", "     ", "     ", "     ", "     "},
    {" ### ", "#   #", "#   #", "#   #", " ### "},
    {" ####", "#    ", " ### ", "    #", "#### "}};

static void drawFlag(int top, int left)
{
    static const int wave[8] = {
        0, 1, 1, 1, 0, -1, -1, -1};

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

    for (int r = top - 1; r < top + 10; r++)
        console.putCell(r, left - 1, CH_BLOCK, CLR_POLE);
}

constexpr int GLYPH_COUNT = 7;
constexpr int FONT_WIDTH = 5;
constexpr int FONT_HEIGHT = 5;
constexpr int GLYPH_SPACING = FONT_WIDTH + 1;
constexpr int RED_ROWS = 2;

static u8 colorForTitleRow(int row)
{
    return row < RED_ROWS ? CLR_RED : CLR_WHITE;
}

static void drawGlyph(int glyph, int top, int left)
{
    for (int row = 0; row < FONT_HEIGHT; row++)
    {
        u8 color = colorForTitleRow(row);

        for (int col = 0; col < FONT_WIDTH; col++)
        {
            if (FONT[glyph][row][col] != '#')
                continue;

            console.putCell(
                top + row,
                left + col,
                CH_BLOCK,
                color);
        }
    }
}

static void drawTitle(int top, int left)
{
    for (int glyph = 0; glyph < GLYPH_COUNT; glyph++)
        drawGlyph(glyph, top, left + glyph * GLYPH_SPACING);
}

// ============================================================
// BANNER JACK OS
// ============================================================

static void showBanner()
{
    console.clear();

    // Bendera Merah Putih
    drawFlag(1, 3);

    // Judul JACK OS dengan desain huruf blok lama
    drawTitle(4, 25);

    // Menu utama
    console.writeAt(10, 28, "[1] Process & Thread", CLR_WHITE);
    console.writeAt(11, 28, "[2] Inventory Management", CLR_WHITE);
    console.writeAt(12, 28, "[3] System & Testing", CLR_WHITE);
    console.writeAt(13, 28, "[4] Clear", CLR_WHITE);
    console.writeAt(14, 28, "[0] Reboot", CLR_WHITE);
    console.writeAt(19, 28, "Pilih menu: ", CLR_WHITE);

    console.setCursor(19, 40);
}

// Demo 2 kernel thread dengan tugas berbeda + perubahan statusnya.
static void runDemo()
{
    console.print("=== DEMO KERNEL THREAD ===\n", CLR_RED);

    if (!create_thread("Data", taskData) ||
        !create_thread("Monitor", taskMonitor))
    {
        console.print("Thread penuh (maksimal 10 thread).\n", CLR_RED);
        return;
    }

    scheduler_run();

    console.print("=== DEMO SELESAI ===\n", CLR_RED);
}

// Tabel semua thread beserta statusnya.
static void showThreads()
{
    console.print("ID   NAMA       STATUS\n", CLR_RED);

    int n = 0;

    for (int i = 0; i < thread_count(); i++)
    {
        thread_t *t = thread_at(i);

        if (!t->in_use)
            continue;

        n++;

        console.print("T", CLR_WHITE);
        printNum(t->tid, CLR_WHITE);
        console.print("   ", CLR_WHITE);
        console.print(t->name, CLR_WHITE);
        console.print("   ", CLR_WHITE);
        console.print(thread_state_name(t->state), CLR_WHITE);
        console.print("\n", CLR_WHITE);
    }

    if (n == 0)
        console.print("(belum ada thread)\n", CLR_WHITE);
}

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

// ================= SYNCHRONIZATION TEST =================

static int syncStok = 0;
static int syncPenjualan = 0;
static int syncRestock = 0;
static bool syncGunakanMutex = false;
static mutex_t syncMutex;

// Membaca input angka dari pengguna
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

// Proses perubahan stok oleh thread
static void prosesSync(bool penjualan)
{
    if (syncGunakanMutex)
        mutex_lock(&syncMutex);

    if (penjualan)
        console.print("[Thread Penjualan] Mengakses stok...\n", CLR_WHITE);
    else
        console.print("[Thread Restock] Mengakses stok...\n", CLR_WHITE);

    int stokSementara = syncStok;

    // Memberi kesempatan thread lain berjalan
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

// Menjalankan satu skenario pengujian
static int jalankanSyncTest(int stokAwal, int penjualan,
                            int restock, bool gunakanMutex)
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

// Menu utama Synchronization Test

static void runSynchronizationTest()
{
    console.clear();

    // Garis pemisah panel.
    for (int r = 0; r < 25; r++)
        console.putCell(r, 39, '|', CLR_RED);

    // Judul panel kanan.
    console.writeAt(0, 42, "THREAD MONITOR", CLR_RED);
    console.writeAt(1, 42, "ID  NAMA THREAD    STATUS", CLR_WHITE);

    // Judul panel kiri.
    console.writeAt(0, 0, "SYNCHRONIZATION TEST", CLR_RED);

    // Input pengguna.
    console.setCursor(3, 0);

    int stokAwal = inputAngka("Stok awal        : ");
    int penjualan = inputAngka("Jumlah penjualan : ");
    int restock = inputAngka("Jumlah restock   : ");

    if (penjualan > stokAwal)
    {
        console.print("\nPenjualan melebihi stok awal!\n", CLR_RED);
        return;
    }

    // Aktifkan logger khusus panel kanan.
    syncLogRow = 15;
    syncMonitorAktif = true;
// scheduler_set_logger(logSyncThreadState);

    console.print("\n=== TANPA MUTEX ===\n", CLR_RED);

    int hasilTanpaMutex =
        jalankanSyncTest(stokAwal, penjualan, restock, false);

    console.print("\n=== DENGAN MUTEX ===\n", CLR_RED);

    int hasilDenganMutex =
        jalankanSyncTest(stokAwal, penjualan, restock, true);

    // Kembalikan logger seperti semula.
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
        showBanner();
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
    }
    else
    {
        console.print("Perintah tidak dikenal: ", CLR_RED);
        console.print(cmd, CLR_RED);
        console.print("\n");
    }
}

static char getMenuChoice()
{
    char choice = keyboard.getChar();

    console.putChar(choice);
    console.putChar('\n');

    return choice;
}

static void showStokDashboard()
{
    console.clear();

    // Garis pemisah dua panel.
    for (int r = 0; r < 25; r++)
        console.putCell(r, 39, '|', CLR_RED);

    // Panel kiri: Inventory CRUD.
    console.writeAt(0, 2, "JACK OS - INVENTORY", CLR_RED);
    console.writeAt(1, 2, "==================", CLR_WHITE);

    // Panel kanan: Thread Monitor.
    console.writeAt(0, 42, "THREAD MONITOR", CLR_RED);
    console.writeAt(1, 42, "ID  NAMA THREAD  STATUS", CLR_WHITE);

    // Panel kanan: Synchronization Monitor.
    console.writeAt(13, 42, "SYNC MONITOR", CLR_RED);
    console.writeAt(14, 42, "AKTIVITAS SINKRONISASI", CLR_WHITE);

    threadLogRow = 3;
    syncLogRow = 15;
    // Posisi awal kursor aplikasi CRUD.
    console.setCursor(2, 2);
}


static void mainMenu()
{
    while (true)
    {
        showBanner();

        char choice = getMenuChoice();

if (choice == '1')
{
    console.clear();
    console.print("=== PROCESS & THREAD ===\n", CLR_RED);
    console.print("Thread Monitor tersedia di Dashboard Inventory.\n", CLR_WHITE);
}
        else if (choice == '2')
        {
            console.clear();

            console.print("=== INVENTORY MANAGEMENT ===\n", CLR_RED);
            console.print("[1] Aplikasi Stok Barang\n", CLR_WHITE);
            console.print("[2] Synchronization Test\n", CLR_WHITE);
            console.print("[0] Kembali\n", CLR_WHITE);

            char sub = getMenuChoice();

            if (sub == '1')
            {
                showStokDashboard();

                thread_t *stokThread =
                    create_thread("Stok App", taskStokApp);

                if (stokThread != nullptr)
                {
                    scheduler_add(stokThread);
                    scheduler_run();
                }
                else
                {
                    console.print(
                        "Gagal membuat thread aplikasi stok.\n",
                        CLR_RED);
                }
            }
            else if (sub == '2')
            {
                runSynchronizationTest();
            }
        }
        else if (choice == '3')
        {
            console.clear();

            console.print("=== SYSTEM & TESTING ===\n", CLR_RED);
            console.print("[1] Informasi Sistem\n", CLR_WHITE);
            console.print("[2] Memory Management\n", CLR_WHITE);
            console.print("[3] Memory Allocation Test\n", CLR_WHITE);
            console.print("[4] I/O Test\n", CLR_WHITE);
            console.print("[0] Kembali\n", CLR_WHITE);

            char sub = getMenuChoice();

            if (sub == '1')
            {
                console.print("JACK OS v1.2\n", CLR_RED);
                console.print("Arsitektur: x86 32-bit\n", CLR_WHITE);
            }
            else if (sub == '2')
                showMem();
            else if (sub == '3')
                runMemTest();
            else if (sub == '4')
                ioDemo();
        }
        else if (choice == '4')
        {
            continue;
        }
        else if (choice == '0')
        {
            console.print("Restarting...\n", CLR_RED);
            outb(0x64, 0xFE);
        }
        else
        {
            console.print("Pilihan tidak tersedia.\n", CLR_RED);
        }

        console.print("\nTekan tombol apa saja untuk kembali...");
        keyboard.getChar();
    }
}

extern "C" void kmain(void)
{
    console.setColor(CLR_WHITE);

    mem::init();
    ramdisk.init();
    
scheduler_init();
mutex_set_logger(logMutexEvent);
//scheduler_set_logger(logThreadState);

mainMenu();
}
