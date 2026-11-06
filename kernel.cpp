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

static const char *PROMPT = "JACK_OS:> ";

// Huruf blok 5x5: J A C K (spasi) O S
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
    console.writeAt(13, 24, "=== JACK_OS C++ KERNEL v1.1 ===", CLR_RED);
    console.setCursor(15, 0);
    console.print("Ketik 'mulai' untuk melihat daftar perintah.\n\n", CLR_WHITE);
}

static void runCommand(const char *cmd)
{
    if (cmd[0] == '\0')
        return;

    if (strEqual(cmd, "help") || strEqual(cmd, "mulai"))
    {
        console.print("Perintah yang tersedia:\n", CLR_RED);
        console.print("  help         - tampilkan bantuan ini\n");
        console.print("  clear        - bersihkan layar\n");
        console.print("  info         - informasi JACK_OS\n");
        console.print("  echo <teks>  - cetak teks\n");
        console.print("  reboot       - restart sistem\n");
    }
    else if (strEqual(cmd, "clear"))
    {
        showBanner();
    }
    else if (strEqual(cmd, "info"))
    {
        console.print("JACK_OS v1.1\n", CLR_RED);
        console.print("  Arsitektur : x86 32-bit (i686)\n");
        console.print("  Bahasa     : C++ freestanding + Assembly\n");
        console.print("  Layar      : VGA text mode 80x25\n");
        console.print("  Input      : keyboard PS/2 (polling)\n");
    }
    else if (startsWith(cmd, "echo "))
    {
        console.print(cmd + 5);
        console.print("\n");
    }
    else if (strEqual(cmd, "echo"))
    {
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