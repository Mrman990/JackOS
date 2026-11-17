#include <stdint.h>
#include <stddef.h>
#include "memori_menejeman/kmalloc.hpp"
#include "process/process.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

// --- IMPLEMENTASI MEMCPY FREESTANDING (Mencegah linker error undefined symbol: memcpy) ---
extern "C" void* memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

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

    // Riwayat layar: baris yang tergulung keluar dari atas disimpan di sini.
    static const int HIST_LINES = 200;
    u16 hist[HIST_LINES * 80];
    u16 live[25 * 80]; // salinan layar asli saat sedang melihat riwayat
    int histHead;      // indeks tulis berikutnya (ring)
    int histCount;     // jumlah baris riwayat tersimpan
    int viewOff;       // 0 = layar asli; >0 = tergulung ke atas sekian baris

    static volatile u8 *vga() { return (volatile u8 *)0xb8000; }
    u16 cellAt(int idx) { return (u16)vga()[idx * 2] | ((u16)vga()[idx * 2 + 1] << 8); }
    void setCell(int idx, u16 cell)
    {
        vga()[idx * 2] = (u8)(cell & 0xFF);
        vga()[idx * 2 + 1] = (u8)(cell >> 8);
    }
    void snap()
    {
        if (viewOff == 0)
            return;
        viewOff = 0;
        for (int i = 0; i < 80 * 25; i++)
            setCell(i, live[i]);
    }
    void render()
    {
        int oldest = (histHead - histCount + HIST_LINES) % HIST_LINES;
        for (int r = 0; r < 25; r++)
        {
            int v = histCount - viewOff + r;
            for (int c = 0; c < 80; c++)
            {
                u16 cell;
                if (v < histCount)
                    cell = hist[((oldest + v) % HIST_LINES) * 80 + c];
                else
                    cell = live[(v - histCount) * 80 + c];
                setCell(r * 80 + c, cell);
            }
        }
        const char *tag = " RIWAYAT - PgDn/End: kembali ";
        int len = 0;
        while (tag[len])
            len++;
        for (int i = 0; i < len; i++)
            setCell(80 - len + i, (u16)(u8)tag[i] | (0x4F << 8));
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
        for (int c = 0; c < 80; c++)
            hist[histHead * 80 + c] = cellAt(c);
        histHead = (histHead + 1) % HIST_LINES;
        if (histCount < HIST_LINES)
            histCount++;

        for (int r = 1; r < 25; r++)
        {
            for (int c = 0; c < 80 * 2; c++)
            {
                vga()[(r - 1) * 80 * 2 + c] = vga()[r * 80 * 2 + c];
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
    void setColor(u8 c) { color = c; }

    void scrollView(int lines)
    {
        if (lines == 0)
        {
            snap();
            return;
        }
        if (histCount == 0)
            return;
        int target = viewOff + lines;
        if (target < 0)
            target = 0;
        if (target > histCount)
            target = histCount;
        if (target == viewOff)
            return;
        if (viewOff == 0)
        {
            for (int i = 0; i < 80 * 25; i++)
                live[i] = cellAt(i);
        }
        if (target == 0)
        {
            snap();
            return;
        }
        viewOff = target;
        render();
    }

    void clear()
    {
        viewOff = 0;
        histCount = 0;
        histHead = 0;
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
        snap();
        row = r;
        col = c;
        updateCursor();
    }
    void writeAt(int r, int c, const char *str, u8 clr)
    {
        snap();
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
        snap();
        if (r < 0 || r >= 25 || c < 0 || c >= 80)
            return;
        int offset = (r * 80 + c) * 2;
        vga()[offset] = ch;
        vga()[offset + 1] = clr;
    }

    void putChar(char ch)
    {
        snap();
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
    void print(const char *str) { print(str, color); }
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

static void consoleScroll(int lines);

class Keyboard
{
private:
    bool shift;
    bool caps;
    bool extended;

public:
    Keyboard() : shift(false), caps(false), extended(false) {}

    char getChar()
    {
        while (true)
        {
            if (!(inb(0x64) & 1))
                continue;

            u8 sc = inb(0x60);

            if (sc == 0xE0)
            {
                extended = true;
                continue;
            }
            if (extended)
            {
                extended = false;
                if (sc == 0x49)
                    consoleScroll(10);
                else if (sc == 0x51)
                    consoleScroll(-10);
                else if (sc == 0x48)
                    consoleScroll(1);
                else if (sc == 0x50)
                    consoleScroll(-1);
                else if (sc == 0x47)
                    consoleScroll(9999);
                else if (sc == 0x4F)
                    consoleScroll(0);
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
                return ch;
        }
    }
};

static VGAConsole console;
static Keyboard keyboard;

static void consoleScroll(int lines)
{
    console.scrollView(lines);
}

#include "Protection/protection.h"
#include "driver/driver.h"
#include "driver/sys_io.h"
#include "ramdisk/ramdisk.h"
#include "Protection/syscall.h"
#include "ramdisk/ramdisk_cmd.h"

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

// --- PEMBACA JAM CMOS (RTC) ---
static u8 readCMOS(u8 reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static u8 bcdToBin(u8 val)
{
    return (val & 0x0F) + ((val / 16) * 10);
}

static void getCMOSTime(u8 &jam, u8 &menit, u8 &detik)
{
    while (readCMOS(0x0A) & 0x80)
        ;
    detik = readCMOS(0x00);
    menit = readCMOS(0x02);
    jam = readCMOS(0x04);

    u8 regB = readCMOS(0x0B);
    if (!(regB & 0x04))
    {
        detik = bcdToBin(detik);
        menit = bcdToBin(menit);
        jam = bcdToBin(jam);
    }
}

// --- SISTEM BUKU LOG INVENTORY (MAX 50 BARIS) ---
#define MAX_LOG_ENTRIES 50

struct LogEntry
{
    u32 seq;
    u8 jam, menit, detik;
    char jenis[16]; // "SESI", "TAMBAH", "UBAH", "HAPUS"
    int itemId;     // -1 jika event sesi
    char namaBarang[32];
    int stokLama;
    int stokBaru;
};

extern "C" thread_t *create_thread_waiting(const char *name, void (*entry_point)(void));
extern "C" void thread_transition(thread_t *t, thread_state_t next_state);

static void dummyLoggerEntry() {}

class InventoryLogger
{
private:
    LogEntry entries[MAX_LOG_ENTRIES];
    int head = 0;
    int count = 0;
    u32 nextSeq = 1;
    thread_t *loggerThread = nullptr;
    LogEntry pendingEntry;

    void writeToBook(const LogEntry &e)
    {
        // Penyalinan manual per atribut (menghindari memcpy implisit)
        entries[head].seq = e.seq;
        entries[head].jam = e.jam;
        entries[head].menit = e.menit;
        entries[head].detik = e.detik;
        for (int i = 0; i < 16; i++)
            entries[head].jenis[i] = e.jenis[i];
        entries[head].itemId = e.itemId;
        for (int i = 0; i < 32; i++)
            entries[head].namaBarang[i] = e.namaBarang[i];
        entries[head].stokLama = e.stokLama;
        entries[head].stokBaru = e.stokBaru;

        head = (head + 1) % MAX_LOG_ENTRIES;
        if (count < MAX_LOG_ENTRIES)
            count++;
    }

    void copyStr(char *dst, const char *src, int maxLen)
    {
        int i = 0;
        while (src && src[i] != '\0' && i < maxLen - 1)
        {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
    }

    void printTwoDigits(u8 v, u8 clr)
    {
        if (v < 10)
            console.print("0", clr);
        printNum(v, clr);
    }

public:
    // Dipanggil saat pengguna membuka Manajemen inventory (inventory)
    void openSession()
    {
        loggerThread = create_thread_waiting("Logger", dummyLoggerEntry);

        LogEntry e;
        e.seq = nextSeq++;
        getCMOSTime(e.jam, e.menit, e.detik);
        copyStr(e.jenis, "SESI", 16);
        e.itemId = -1;
        copyStr(e.namaBarang, "MASUK inventory", 32);
        e.stokLama = 0;
        e.stokBaru = 0;

        if (loggerThread)
            thread_transition(loggerThread, THREAD_READY);
        if (loggerThread)
            thread_transition(loggerThread, THREAD_RUNNING);
        writeToBook(e);
        if (loggerThread)
            thread_transition(loggerThread, THREAD_WAITING);
    }

    // Dipanggil setiap kali operasi TAMBAH, UBAH, atau HAPUS berhasil
    void recordChange(const char *jenis, int id, const char *nama, int stokLama, int stokBaru)
    {
        pendingEntry.seq = nextSeq++;
        getCMOSTime(pendingEntry.jam, pendingEntry.menit, pendingEntry.detik);
        copyStr(pendingEntry.jenis, jenis, 16);
        pendingEntry.itemId = id;
        copyStr(pendingEntry.namaBarang, nama, 32);
        pendingEntry.stokLama = stokLama;
        pendingEntry.stokBaru = stokBaru;

        if (loggerThread)
            thread_transition(loggerThread, THREAD_READY);
        if (loggerThread)
            thread_transition(loggerThread, THREAD_RUNNING);
        writeToBook(pendingEntry);
        if (loggerThread)
            thread_transition(loggerThread, THREAD_WAITING);
    }

    // Dipanggil saat pengguna keluar dari Manajemen inventory (menu 0)
    void closeSession()
    {
        LogEntry e;
        e.seq = nextSeq++;
        getCMOSTime(e.jam, e.menit, e.detik);
        copyStr(e.jenis, "SESI", 16);
        e.itemId = -1;
        copyStr(e.namaBarang, "KELUAR inventory", 32);
        e.stokLama = 0;
        e.stokBaru = 0;

        if (loggerThread)
            thread_transition(loggerThread, THREAD_READY);
        if (loggerThread)
            thread_transition(loggerThread, THREAD_RUNNING);
        writeToBook(e);
        if (loggerThread)
        {
            thread_transition(loggerThread, THREAD_TERMINATED);
            loggerThread = nullptr;
        }
    }

    // Dipanggil dari perintah 'log' di mode KERNEL
    void printLogs()
    {
        console.print("=== BUKU LOG INVENTORY (50 TERAKHIR) ===\n", CLR_RED);
        if (count == 0)
        {
            console.print("(Buku log masih kosong)\n", CLR_WHITE);
            return;
        }

        int oldest = (head - count + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
        for (int i = 0; i < count; i++)
        {
            const LogEntry &e = entries[(oldest + i) % MAX_LOG_ENTRIES];
            console.print("#", CLR_WHITE);
            printNum(e.seq, CLR_WHITE);
            console.print(" [", CLR_WHITE);
            printTwoDigits(e.jam, CLR_WHITE);
            console.print(":", CLR_WHITE);
            printTwoDigits(e.menit, CLR_WHITE);
            console.print(":", CLR_WHITE);
            printTwoDigits(e.detik, CLR_WHITE);
            console.print("] ", CLR_WHITE);

            if (strEqual(e.jenis, "SESI"))
            {
                console.print(e.namaBarang, CLR_RED);
                console.print("\n", CLR_WHITE);
            }
            else
            {
                console.print(e.jenis, CLR_RED);
                console.print(" | ID:", CLR_WHITE);
                printNum((u32)e.itemId, CLR_WHITE);
                console.print(" | ", CLR_WHITE);
                console.print(e.namaBarang, CLR_WHITE);
                console.print(" | Stok: ", CLR_WHITE);
                if (strEqual(e.jenis, "UBAH"))
                {
                    printNum((u32)e.stokLama, CLR_WHITE);
                    console.print(" -> ", CLR_WHITE);
                    printNum((u32)e.stokBaru, CLR_WHITE);
                }
                else
                {
                    printNum((u32)e.stokBaru, CLR_WHITE);
                }
                console.print("\n", CLR_WHITE);
            }
        }
    }
};

static InventoryLogger invLogger;

#include "aplikasi/stok_app.h"

// Dipanggil scheduler setiap status thread berubah.
// Aturan 4: Perubahan status thread hanya ditampilkan saat mode KERNEL.
static void logThreadState(const thread_t *t, thread_state_t from, thread_state_t to)
{
    if (g_mode != MODE_KERNEL)
        return;

    console.print("[T", CLR_RED);
    printNum(t->tid, CLR_RED);
    console.print(" ", CLR_RED);
    console.print(t->name, CLR_RED);
    console.print("] ", CLR_RED);
    if (from == to)
    {
        console.print(thread_state_name(to), CLR_WHITE);
    }
    else
    {
        console.print(thread_state_name(from), CLR_WHITE);
        console.print(" -> ", CLR_WHITE);
        console.print(thread_state_name(to), CLR_WHITE);
    }
    console.print("\n", CLR_WHITE);
}

// Thread Inventory: melaporkan jumlah barang yang ada di RAMDisk.
void inventoryTask()
{
    console.print("[THREAD] Inventory: ", CLR_RED);
    printNum(ramdisk.count(), CLR_RED);
    console.print(" jenis barang tercatat di RAMDisk.\n", CLR_RED);
}

// Tugas 1: menghitung total stok dari RAMDisk, menunggu (WAITING) sebelum selesai.
static void taskData()
{
    console.print("  [Data] menghitung stok dari RAMDisk...\n", CLR_WHITE);
    thread_sleep(2);
    u32 total = 0;
    for (int i = 0; i < RD_MAX_ITEMS; i++)
        if (ramdisk.items[i].used)
            total += ramdisk.items[i].stock;
    console.print("  [Data] ", CLR_WHITE);
    printNum(ramdisk.count(), CLR_WHITE);
    console.print(" jenis barang, total stok ", CLR_WHITE);
    printNum(total, CLR_WHITE);
    console.print(".\n", CLR_WHITE);
}

// Tugas 2: memeriksa kapasitas RAMDisk dan heap, melepas giliran (READY) tiap pemeriksaan.
static void taskMonitor()
{
    for (int i = 1; i <= 2; i++)
    {
        console.print("  [Monitor] cek ke-", CLR_WHITE);
        printNum((u32)i, CLR_WHITE);
        console.print(": barang ", CLR_WHITE);
        printNum(ramdisk.count(), CLR_WHITE);
        console.print("/", CLR_WHITE);
        printNum(RD_MAX_ITEMS, CLR_WHITE);
        console.print(", heap terpakai ", CLR_WHITE);
        printNum((u32)mem::used(), CLR_WHITE);
        console.print(" B\n", CLR_WHITE);
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
    static const int wave[8] = {0, 1, 1, 1, 0, -1, -1, -1};

    for (int x = 0; x < 20; x++)
    {
        for (int b = 0; b < 8; b++)
        {
            u8 clr = (b < 4) ? CLR_RED : CLR_WHITE;
            console.putCell(top + b + wave[x % 8], left + x, CH_BLOCK, clr);
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
            console.putCell(top + row, left + col, CH_BLOCK, color);
        }
    }
}

static void drawTitle(int top, int left)
{
    for (int glyph = 0; glyph < GLYPH_COUNT; glyph++)
        drawGlyph(glyph, top, left + glyph * GLYPH_SPACING);
}

static void showBanner()
{
    console.clear();
    drawFlag(3, 6);
    drawTitle(5, 31);
    console.writeAt(13, 24, "=== JACK_OS C++ KERNEL v1.2 ===", CLR_RED);
    console.setCursor(15, 0);
    console.print("Ketik 'mulai' untuk melihat daftar perintah.\n\n", CLR_WHITE);
}

// Laporan stok: 2 kernel thread dengan tugas berbeda, memakai data asli RAMDisk.
static void runLaporan()
{
    console.print("=== LAPORAN STOK (2 kernel thread) ===\n", CLR_RED);
    if (!create_thread("Data", taskData) || !create_thread("Monitor", taskMonitor))
    {
        console.print("Thread penuh (maksimal 10 thread).\n", CLR_RED);
        return;
    }
    scheduler_run();
    console.print("=== LAPORAN SELESAI ===\n", CLR_RED);
}

static void printHex(u32 v, u8 clr)
{
    const char *digits = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++)
        buf[2 + i] = digits[(v >> (28 - i * 4)) & 0xF];
    buf[10] = '\0';
    console.print(buf, clr);
}

// Ringkasan pemakaian heap.
[[maybe_unused]] static void showMem()
{
    console.print("Heap: total ", CLR_WHITE);
    printNum((u32)mem::total(), CLR_WHITE);
    console.print(" B, terpakai ", CLR_WHITE);
    printNum((u32)mem::used(), CLR_WHITE);
    console.print(" B, bebas ", CLR_WHITE);
    printNum((u32)(mem::total() - mem::used()), CLR_WHITE);
    console.print(" B\n", CLR_WHITE);
}

static void printResult(const char *label, bool ok)
{
    console.print("  ", CLR_WHITE);
    console.print(label, CLR_WHITE);
    console.print(ok ? ": OK\n" : ": GAGAL\n", ok ? CLR_WHITE : CLR_RED);
}

#include "Protection/protection_cmd.h"

// Tes kmalloc/kfree: alokasi, isi data, lubang bekas free, dan penggabungan blok.
[[maybe_unused]] static void runMemTest()
{
    console.print("=== TES MEMORI ===\n", CLR_RED);
    u32 awal = (u32)mem::used();
    showMem();

    char *a = (char *)kmalloc(100);
    char *b = (char *)kmalloc(200);
    char *c = (char *)kmalloc(50);
    console.print("kmalloc a(100) = ", CLR_WHITE);
    printHex((u32)(uintptr_t)a, CLR_WHITE);
    console.print("\nkmalloc b(200) = ", CLR_WHITE);
    printHex((u32)(uintptr_t)b, CLR_WHITE);
    console.print("\nkmalloc c(50)  = ", CLR_WHITE);
    printHex((u32)(uintptr_t)c, CLR_WHITE);
    console.print("\n", CLR_WHITE);
    showMem();
    printResult("3 blok berhasil dialokasi", a && b && c && a != b && b != c);

    bool dataOk = a && c;
    if (dataOk)
    {
        for (int i = 0; i < 100; i++)
            a[i] = (char)(i + 1);
        for (int i = 0; i < 50; i++)
            c[i] = (char)(200 - i);
        for (int i = 0; i < 100; i++)
            if (a[i] != (char)(i + 1))
                dataOk = false;
        for (int i = 0; i < 50; i++)
            if (c[i] != (char)(200 - i))
                dataOk = false;
    }
    printResult("isi data a dan c utuh", dataOk);

    kfree(b);
    char *d = (char *)kmalloc(150);
    printResult("d menempati lubang bekas b", d == b);

    kfree(a);
    kfree(c);
    kfree(d);
    printResult("semua dibebaskan, heap kembali utuh", (u32)mem::used() == awal);

    void *besar = kmalloc((size_t)mem::total() * 2);
    printResult("alokasi terlalu besar ditolak", besar == nullptr);
    showMem();
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

static void runCommand(const char *cmd)
{
    if (cmd[0] == '\0')
        return;

    if (strEqual(cmd, "help") || strEqual(cmd, "mulai"))
    {
        if (g_mode == MODE_KERNEL)
        {
            console.print("=== MENU UTAMA KERNEL ===\n", CLR_RED);
            console.print("  help          - tampilkan bantuan ini\n");
            console.print("  threads       - tampilkan daftar thread & status\n");
            console.print("  log           - baca buku log aktivitas inventory\n");
            console.print("  laporan       - laporan stok oleh 2 thread (data asli RAMDisk)\n");
            console.print("  izin <op> <on|off> - Akses sistem Inventory (atur izin mode USER)\n");
            console.print("  exit          - kembali ke mode USER\n");
            console.print("  (perintah Menu USER juga tetap bisa dipakai di mode ini)\n");
        }
        else
        {
            console.print("=== MENU UTAMA USER ===\n", CLR_RED);
            console.print("  help                - tampilkan bantuan ini\n");
            console.print("  inventory           - Manajemen inventory\n");
            console.print("  info                - informasi\n");
            console.print("  mode                - tampilkan mode; 'mode kernel' pindah ke mode KERNEL\n");
            console.print("  clear               - bersihkan layar\n");
            console.print("  echo <teks>         - cetak teks\n");
            console.print("  reboot              - restart sistem\n");
        }
        console.print("Gulir layar: PgUp/PgDn, panah atas/bawah, Home, End.\n", CLR_WHITE);
    }
    else if (strEqual(cmd, "inventory"))
    {
        runStokApp();
    }
    else if (strEqual(cmd, "log"))
    {
        if (g_mode != MODE_KERNEL)
        {
            console.print("Akses ditolak: perintah 'log' hanya untuk mode KERNEL.\n", CLR_RED);
        }
        else
        {
            invLogger.printLogs();
        }
    }
    else if (strEqual(cmd, "laporan"))
    {
        if (requireKernelCmd("laporan"))
            runLaporan();
    }
    else if (strEqual(cmd, "threads"))
    {
        if (requireKernelCmd("threads"))
            showThreads();
    }
    else if (protectionCommand(cmd))
    {
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

extern "C" void kmain(void)
{
    showBanner();
    console.setColor(CLR_WHITE);

    mem::init(); // Inisialisasi Heap Memori
    ramdisk.init();

    scheduler_init();
    scheduler_set_logger(logThreadState);

    char buffer[128];
    int len = 0;

    while (true)
    {
        console.print(g_mode == MODE_KERNEL ? "JACK_OS#> " : PROMPT, CLR_RED);
        len = 0;

        while (true)
        {
            char ch = keyboard.getChar();

            if (ch == '\n')
            {
                console.putChar('\n');
                buffer[len] = '\0';
                break;
            }
            else if (ch == '\b')
            {
                if (len > 0)
                {
                    len--;
                    console.putChar('\b');
                }
            }
            else if (ch >= 32 && ch < 127 && len < 127)
            {
                buffer[len++] = ch;
                console.setColor(CLR_WHITE);
                console.putChar(ch);
            }
        }

        runCommand(buffer);
    }
}