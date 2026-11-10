#include <stdint.h>
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#include "process/process.h"
#include "interrupt.h"

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

static inline u32 irq_save_disable(void)
{
    u32 flags;
    __asm__ __volatile__("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(u32 flags)
{
    __asm__ __volatile__("pushl %0; popfl" : : "r"(flags) : "memory");
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
#define CLR_GREEN 0x0A
#define CLR_YELLOW 0x0E
#define CH_BLOCK 0xDB

// Serial COM1 (0x3F8) untuk output mirror & input serial terminal
static void serialInit(void)
{
    outb(0x3F8 + 1, 0x00); // Disable interrupts
    outb(0x3F8 + 3, 0x80); // Enable DLAB
    outb(0x3F8 + 0, 0x03); // Divisor 3 (38400 baud)
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03); // 8 bits, no parity, 1 stop bit
    outb(0x3F8 + 2, 0xC7); // FIFO
    outb(0x3F8 + 4, 0x0B); // IRQ enable, RTS/DSR set
}

static void serialPutc(char c)
{
    while ((inb(0x3F8 + 5) & 0x20) == 0)
    {
    }
    outb(0x3F8, c);
}

class VGAConsole
{
private:
    int row = 0;
    int col = 0;
    u8 color;

    static volatile u8 *vga() { return (volatile u8 *)0xb8000; }
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

    void clear()
    {
        u32 f = irq_save_disable();
        for (int i = 0; i < 80 * 25 * 2; i += 2)
        {
            vga()[i] = ' ';
            vga()[i + 1] = 0x07;
        }
        row = 0;
        col = 0;
        updateCursor();
        irq_restore(f);
    }
    void setCursor(int r, int c)
    {
        u32 f = irq_save_disable();
        row = r;
        col = c;
        updateCursor();
        irq_restore(f);
    }
    void writeAt(int r, int c, const char *str, u8 clr)
    {
        u32 f = irq_save_disable();
        int offset = (r * 80 + c) * 2;
        for (int i = 0; str[i] != '\0'; i++)
        {
            vga()[offset] = str[i];
            vga()[offset + 1] = clr;
            offset += 2;
        }
        irq_restore(f);
    }

    void putCell(int r, int c, u8 ch, u8 clr)
    {
        if (r < 0 || r >= 25 || c < 0 || c >= 80)
            return;
        u32 f = irq_save_disable();
        int offset = (r * 80 + c) * 2;
        vga()[offset] = ch;
        vga()[offset + 1] = clr;
        irq_restore(f);
    }

    void putChar(char ch)
    {
        u32 f = irq_save_disable();
        serialPutc(ch);
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
        irq_restore(f);
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
            // Cek port serial jika ada karakter masuk
            if (inb(0x3F8 + 5) & 1)
            {
                char sc = (char)inb(0x3F8);
                if (sc == '\r')
                    return '\n';
                return sc;
            }

            // Cek keyboard PS/2
            if (!(inb(0x64) & 1))
            {
                // Beri kesempatan CPU rileks jika tidak ada input
                __asm__ __volatile__("pause");
                continue;
            }

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
                return ch;
        }
    }
};

static VGAConsole console;
static Keyboard keyboard;

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

// Dipanggil scheduler setiap status thread berubah.
static void logThreadState(const thread_t *t, thread_state_t from, thread_state_t to)
{
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

// Dipanggil saat scheduler memotong giliran thread secara preemptive
static void logPreempt(const thread_t *from_t, const thread_t *to_t)
{
    console.print(">> [PREEMPT TIMER IRQ0] Quantum habis! Switch CPU: ", CLR_YELLOW);
    console.print(from_t ? from_t->name : "?", CLR_YELLOW);
    console.print(" -> ", CLR_YELLOW);
    console.print(to_t ? to_t->name : "?", CLR_YELLOW);
    console.print("\n", CLR_WHITE);
}

// -------------------------------------------------------------
// Tugas untuk Demo Ketentuan 2 (Status New, Ready, Running, Waiting, Terminated)
// -------------------------------------------------------------
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

void inventoryTask()
{
    console.print("[THREAD] Task Inventory Management Berjalan (Preemptive Mode)...\n", CLR_RED);
}

// -------------------------------------------------------------
// Tugas untuk Demo Ketentuan 3: Penjadwalan CPU Preemptive
// Dua thread murni melakukan komputasi berat TANPA memanggil yield()
// Scheduler PIT timer memotong giliran (preempt) secara otomatis.
// -------------------------------------------------------------
static void workerA()
{
    for (int i = 1; i <= 3; i++)
    {
        console.print("  [Worker A] Komputasi matriks siklus ", CLR_GREEN);
        printNum((u32)i, CLR_GREEN);
        console.print(" sedang diproses di CPU...\n", CLR_GREEN);

        // Komputasi CPU murni (TIDAK ADA yield!)
        for (volatile u32 d = 0; d < 25000000; d++)
        {
        }
    }
    console.print("  [Worker A] Seluruh siklus komputasi selesai!\n", CLR_GREEN);
}

static void workerB()
{
    for (int i = 1; i <= 3; i++)
    {
        console.print("  [Worker B] Enkripsi paket siklus ", CLR_YELLOW);
        printNum((u32)i, CLR_YELLOW);
        console.print(" sedang diproses di CPU...\n", CLR_YELLOW);

        // Komputasi CPU murni (TIDAK ADA yield!)
        for (volatile u32 d = 0; d < 25000000; d++)
        {
        }
    }
    console.print("  [Worker B] Seluruh siklus enkripsi selesai!\n", CLR_YELLOW);
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
    console.writeAt(13, 24, "=== JACK_OS C++ KERNEL v2.0 ===", CLR_RED);
    console.setCursor(15, 0);
    console.print("Ketik 'mulai' atau 'help' untuk melihat daftar perintah.\n\n", CLR_WHITE);
}

// Demo Ketentuan 2: 2 kernel thread + perubahan status (New, Ready, Running, Waiting, Terminated)
static void runDemo()
{
    console.print("=== DEMO KERNEL THREAD (KETENTUAN 2) ===\n", CLR_RED);
    if (!create_thread("Data", taskData) || !create_thread("Monitor", taskMonitor))
    {
        console.print("Thread penuh (maksimal 10 thread).\n", CLR_RED);
        return;
    }
    scheduler_run();
    console.print("=== DEMO THREAD SELESAI ===\n", CLR_RED);
}

// Demo Ketentuan 3: Penjadwalan CPU Preemptive murni via PIT Timer Interrupt & Context Switch
static void runPreemptDemo()
{
    console.print("=== DEMO PENJADWALAN CPU PREEMPTIVE (KETENTUAN 3) ===\n", CLR_RED);
    console.print("Thread Worker A & Worker B murni komputasi CPU TANPA memanggil yield().\n", CLR_WHITE);
    console.print("Timer PIT (IRQ0) memotong eksekusi dan memicu context switching secara berkala.\n\n", CLR_WHITE);

    u32 p_before = scheduler_preempt_count();

    if (!create_thread("WorkerA", workerA) || !create_thread("WorkerB", workerB))
    {
        console.print("Thread penuh (maksimal 10 thread).\n", CLR_RED);
        return;
    }

    scheduler_run();

    u32 p_after = scheduler_preempt_count();

    console.print("\n=== DEMO PREEMPTIVE SELESAI ===\n", CLR_RED);
    console.print("Total Pemotongan Preemptive (IRQ0) selama sesi: ", CLR_WHITE);
    printNum(p_after - p_before, CLR_RED);
    console.print(" kali.\n", CLR_WHITE);
    console.print("Pembuktian: Giliran eksekusi CPU diatur secara nyata via timer interrupt!\n\n", CLR_WHITE);
}

// Tabel semua thread beserta statusnya
static void showThreads()
{
    console.print("ID   NAMA       STATUS       QUANTUM\n", CLR_RED);
    int n = 0;
    for (int i = 0; i < thread_count(); i++)
    {
        thread_t *t = thread_at(i);
        if (!t || !t->in_use)
            continue;
        n++;
        console.print("T", CLR_WHITE);
        printNum(t->tid, CLR_WHITE);
        console.print("   ", CLR_WHITE);
        console.print(t->name, CLR_WHITE);
        console.print("   ", CLR_WHITE);
        console.print(thread_state_name(t->state), CLR_WHITE);
        console.print("     ", CLR_WHITE);
        printNum(t->time_slice, CLR_WHITE);
        console.print("\n", CLR_WHITE);
    }
    if (n == 0)
        console.print("(belum ada thread)\n", CLR_WHITE);

    console.print("\nTotal Timer Ticks   : ", CLR_WHITE);
    printNum(scheduler_ticks(), CLR_RED);
    console.print("\nTotal Preempt Switches: ", CLR_WHITE);
    printNum(scheduler_preempt_count(), CLR_RED);
    console.print("\n", CLR_WHITE);
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
        console.print("  demo          - demo Ketentuan 2 (Thread Status Transition)\n");
        console.print("  preempt       - demo Ketentuan 3 (Penjadwalan CPU Preemptive)\n");
        console.print("  threads       - tampilkan daftar thread & status CPU\n");
        console.print("  inventory     - jalankan thread inventory management\n");
        console.print("  echo <teks>   - cetak teks ke konsol\n");
        console.print("  reboot        - restart sistem\n");
    }
    else if (strEqual(cmd, "preempt"))
    {
        runPreemptDemo();
    }
    else if (strEqual(cmd, "demo"))
    {
        runDemo();
    }
    else if (strEqual(cmd, "inventory"))
    {
        console.print("Membuat & menjadwalkan Thread Inventory...\n", CLR_WHITE);
        if (!create_thread("Inventory", inventoryTask))
        {
            console.print("Thread penuh (maksimal 10 thread).\n", CLR_RED);
            return;
        }
        scheduler_run();
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
        console.print("JACK_OS v2.0 (Preemptive Multitasking Kernel)\n", CLR_RED);
        console.print("  Arsitektur  : x86 32-bit Protected Mode (i686)\n");
        console.print("  Penjadwalan : Preemptive Round Robin (PIT Timer IRQ0)\n");
        console.print("  Interrupt   : GDT, IDT (256 Gates), PIC 8259 Remapped\n");
        console.print("  Driver      : VGA Text Mode (80x25), PS/2 Keyboard, Serial COM1\n");
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
    serialInit();
    showBanner();
    console.setColor(CLR_WHITE);

    // Inisialisasi GDT, IDT, PIC, dan PIT (Timer 50Hz)
    interrupts_init();

    // Inisialisasi Scheduler
    scheduler_init();
    scheduler_set_logger(logThreadState);
    scheduler_set_preempt_logger(logPreempt);

    // Aktifkan CPU interrupt agar timer PIT (IRQ0) mulai berjalan
    __asm__ __volatile__("sti");

    char buffer[128];
    int len = 0;

    while (true)
    {
        console.print(PROMPT, CLR_RED);
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
