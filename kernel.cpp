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
    VGAConsole() {}
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
static void showBanner()
{
    console.clear();
    console.writeAt(1, 17, "     _     _      ____  _  __     ___    ____  ", 0x0A);
    console.writeAt(2, 17, "    | |   / \\    / ___|| |/ /    / _ \\ / ___| ", 0x0A);
    console.writeAt(3, 17, " _  | |  / _ \\  | |    | ' /    | | | |\\___ \\ ", 0x0A);
    console.writeAt(4, 17, "| |_| | / ___ \\ | |___ | . \\    | |_| | ___) |", 0x0A);
    console.writeAt(5, 17, " \\___/ /_/   \\_\\ \\____||_|\\_\\    \\___/ |____/ ", 0x0A);
    console.writeAt(7, 25, "=== JACK_OS C++ KERNEL v1.1 ===", 0x0E);
    console.setCursor(9, 0);
    console.print("Ketik 'get start' untuk melihat daftar perintah.\n\n", 0x0B);
}

static void runCommand(const char *cmd)
{
    if (cmd[0] == '\0')
        return;

    if (strEqual(cmd, "help") || strEqual(cmd, "get start"))
    {
        console.print("Perintah yang tersedia:\n", 0x0E);
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
        console.print("JACK_OS v1.1\n", 0x0A);
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
        console.print("Restarting...\n", 0x0C);
        outb(0x64, 0xFE);
    }
    else
    {
        console.print("Perintah tidak dikenal: ", 0x0C);
        console.print(cmd, 0x0C);
        console.print("\n");
    }
}

extern "C" void kmain(void)
{
    showBanner();

    char buffer[128];
    int len = 0;

    while (true)
    {
        console.print(PROMPT, 0x0A);
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
                console.setColor(0x0F);
                console.putChar(ch);
            }
        }

        runCommand(buffer);
    }
}