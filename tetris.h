#ifndef TETRIS_H
#define TETRIS_H

// Game Tetris untuk JACK_OS.
// Di-include dari kernel.cpp SETELAH `console`, `keyboard`, inb/outb dan
// makro CLR_RED / CLR_WHITE didefinisikan.

#define T_W 10   // lebar papan (kolom)
#define T_H 20   // tinggi papan (baris)
#define T_TOP 2  // baris layar tempat papan dimulai
#define T_LEFT 29 // kolom layar border kiri papan

// Bentuk balok: 4 rotasi, tiap rotasi berupa bitmask 4x4 (bit 15 = kiri atas)
static const u16 PIECES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
    {0x6600, 0x6600, 0x6600, 0x6600}, // O
    {0x4E00, 0x4640, 0x0E40, 0x4C40}, // T
    {0x6C00, 0x4620, 0x06C0, 0x8C40}, // S
    {0xC600, 0x2640, 0x0C60, 0x4C80}, // Z
    {0x8E00, 0x6440, 0x0E20, 0x44C0}, // J
    {0x2E00, 0x4460, 0x0E80, 0xC440}  // L
};
static const u8 PIECE_COLOR[7] = {0x0B, 0x0E, 0x0D, 0x0A, 0x0C, 0x09, 0x06};
static const u32 LINE_SCORE[5] = {0, 100, 300, 500, 800};

// ---------- Timer berbasis PIT (channel 0) ----------
static u16 pitLast = 0;
static u32 pitTicks = 0;

static u16 readPit()
{
    outb(0x43, 0x00); // latch counter channel 0
    u8 lo = inb(0x40);
    u8 hi = inb(0x40);
    return (u16)((hi << 8) | lo);
}

static void timeInit()
{
    pitLast = readPit();
    pitTicks = 0;
}

// Milidetik sejak timeInit(). Harus dipanggil cukup sering (< ~50 ms).
static u32 millis()
{
    u16 now = readPit();
    pitTicks += (u16)(pitLast - now); // counter menghitung mundur
    pitLast = now;
    return pitTicks / 1193;           // PIT berjalan 1.193182 MHz
}

static void drawNumber(int r, int c, u32 v, u8 clr)
{
    char buf[8];
    for (int i = 0; i < 7; i++)
        buf[i] = ' ';
    buf[7] = '\0';
    if (v == 0)
        buf[6] = '0';
    else
    {
        int i = 6;
        while (v > 0 && i >= 0)
        {
            buf[i--] = (char)('0' + v % 10);
            v /= 10;
        }
    }
    console.writeAt(r, c, buf, clr);
}

class Tetris
{
private:
    u8 board[T_H][T_W];
    int curType, curRot, curX, curY, nextType;
    u32 score, lines, level;
    bool over;
    u32 rngState;
    u8 bag[7];
    int bagPos;

    // ----- acak (LCG) + 7-bag -----
    u32 rnd()
    {
        rngState = rngState * 1664525u + 1013904223u;
        return rngState >> 16;
    }

    int nextPiece()
    {
        if (bagPos >= 7)
        {
            for (int i = 0; i < 7; i++)
                bag[i] = (u8)i;
            for (int i = 6; i > 0; i--)
            {
                int j = (int)(rnd() % (u32)(i + 1));
                u8 t = bag[i];
                bag[i] = bag[j];
                bag[j] = t;
            }
            bagPos = 0;
        }
        return bag[bagPos++];
    }

    // ----- logika -----
    static bool cellSet(int type, int rot, int r, int c)
    {
        return ((PIECES[type][rot] >> (15 - (r * 4 + c))) & 1) != 0;
    }

    bool collides(int type, int rot, int px, int py) const
    {
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                if (!cellSet(type, rot, r, c))
                    continue;
                int x = px + c;
                int y = py + r;
                if (x < 0 || x >= T_W || y >= T_H)
                    return true;
                if (y >= 0 && board[y][x])
                    return true;
            }
        }
        return false;
    }

    void spawn()
    {
        curType = nextType;
        nextType = nextPiece();
        curRot = 0;
        curX = 3;
        curY = 0;
        if (collides(curType, curRot, curX, curY))
            over = true;
    }

    void clearLines()
    {
        u32 cleared = 0;
        int y = T_H - 1;
        while (y >= 0)
        {
            bool full = true;
            for (int x = 0; x < T_W; x++)
            {
                if (!board[y][x])
                {
                    full = false;
                    break;
                }
            }
            if (full)
            {
                for (int r = y; r > 0; r--)
                    for (int x = 0; x < T_W; x++)
                        board[r][x] = board[r - 1][x];
                for (int x = 0; x < T_W; x++)
                    board[0][x] = 0;
                cleared++;
            }
            else
                y--;
        }
        if (cleared > 0)
        {
            score += LINE_SCORE[cleared] * level;
            lines += cleared;
            level = lines / 10 + 1;
        }
    }

    void lockPiece()
    {
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                if (!cellSet(curType, curRot, r, c))
                    continue;
                int x = curX + c;
                int y = curY + r;
                if (y >= 0 && y < T_H && x >= 0 && x < T_W)
                    board[y][x] = PIECE_COLOR[curType];
            }
        }
        clearLines();
        spawn();
    }

    void stepDown()
    {
        if (!collides(curType, curRot, curX, curY + 1))
            curY++;
        else
            lockPiece();
    }

    void rotate()
    {
        static const int kicks[5] = {0, -1, 1, -2, 2};
        int newRot = (curRot + 1) & 3;
        for (int i = 0; i < 5; i++)
        {
            if (!collides(curType, newRot, curX + kicks[i], curY))
            {
                curRot = newRot;
                curX += kicks[i];
                return;
            }
        }
    }

    u32 dropInterval() const
    {
        int ms = 800 - ((int)level - 1) * 70;
        if (ms < 100)
            ms = 100;
        return (u32)ms;
    }

    // ----- tampilan -----
    static void drawCell(int y, int x, u8 ch, u8 clr)
    {
        int sx = T_LEFT + 1 + x * 2;
        console.putCell(T_TOP + y, sx, ch, clr);
        console.putCell(T_TOP + y, sx + 1, ch, clr);
    }

    static void drawEmpty(int y, int x)
    {
        int sx = T_LEFT + 1 + x * 2;
        console.putCell(T_TOP + y, sx, 0xFA, 0x08); // titik grid
        console.putCell(T_TOP + y, sx + 1, ' ', 0x07);
    }

    void drawFrame()
    {
        console.clear();
        console.writeAt(0, 34, "T E T ", CLR_RED);
        console.writeAt(0, 40, "R I S", CLR_WHITE);

        // border atas & bawah (merah), samping (putih)
        console.putCell(T_TOP - 1, T_LEFT, 0xC9, CLR_RED);
        console.putCell(T_TOP + T_H, T_LEFT, 0xC8, CLR_RED);
        for (int i = 1; i <= T_W * 2; i++)
        {
            console.putCell(T_TOP - 1, T_LEFT + i, 0xCD, CLR_RED);
            console.putCell(T_TOP + T_H, T_LEFT + i, 0xCD, CLR_RED);
        }
        console.putCell(T_TOP - 1, T_LEFT + T_W * 2 + 1, 0xBB, CLR_RED);
        console.putCell(T_TOP + T_H, T_LEFT + T_W * 2 + 1, 0xBC, CLR_RED);
        for (int r = 0; r < T_H; r++)
        {
            console.putCell(T_TOP + r, T_LEFT, 0xBA, CLR_WHITE);
            console.putCell(T_TOP + r, T_LEFT + T_W * 2 + 1, 0xBA, CLR_WHITE);
        }

        // panel kiri
        console.writeAt(4, 4, "SKOR", CLR_RED);
        console.writeAt(7, 4, "LEVEL", CLR_RED);
        console.writeAt(10, 4, "BARIS", CLR_RED);

        // panel kanan
        console.writeAt(4, 54, "BERIKUTNYA", CLR_RED);
        console.writeAt(12, 54, "KONTROL", CLR_RED);
        console.writeAt(13, 54, "A / D    : geser", CLR_WHITE);
        console.writeAt(14, 54, "W        : putar", CLR_WHITE);
        console.writeAt(15, 54, "S        : turun", CLR_WHITE);
        console.writeAt(16, 54, "SPASI    : jatuhkan", CLR_WHITE);
        console.writeAt(17, 54, "P        : jeda", CLR_WHITE);
        console.writeAt(18, 54, "Q / ESC  : keluar", CLR_WHITE);
        console.writeAt(19, 54, "(panah juga bisa)", 0x08);
    }

    void drawStats()
    {
        drawNumber(5, 4, score, CLR_WHITE);
        drawNumber(8, 4, level, CLR_WHITE);
        drawNumber(11, 4, lines, CLR_WHITE);
    }

    void drawNext()
    {
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 8; c++)
                console.putCell(6 + r, 56 + c, ' ', 0x07);
        for (int r = 0; r < 4; r++)
        {
            for (int c = 0; c < 4; c++)
            {
                if (!cellSet(nextType, 0, r, c))
                    continue;
                console.putCell(6 + r, 56 + c * 2, 0xDB, PIECE_COLOR[nextType]);
                console.putCell(6 + r, 56 + c * 2 + 1, 0xDB, PIECE_COLOR[nextType]);
            }
        }
    }

    void drawBoard()
    {
        // papan + balok yang sudah terkunci
        for (int y = 0; y < T_H; y++)
        {
            for (int x = 0; x < T_W; x++)
            {
                if (board[y][x])
                    drawCell(y, x, 0xDB, board[y][x]);
                else
                    drawEmpty(y, x);
            }
        }

        // bayangan (ghost) tempat balok akan mendarat
        int gy = curY;
        while (!collides(curType, curRot, curX, gy + 1))
            gy++;
        if (gy > curY)
        {
            for (int r = 0; r < 4; r++)
                for (int c = 0; c < 4; c++)
                    if (cellSet(curType, curRot, r, c) && gy + r >= 0)
                        drawCell(gy + r, curX + c, 0xB0, PIECE_COLOR[curType]);
        }

        // balok yang sedang jatuh
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                if (cellSet(curType, curRot, r, c) && curY + r >= 0)
                    drawCell(curY + r, curX + c, 0xDB, PIECE_COLOR[curType]);
    }

    void reset()
    {
        for (int y = 0; y < T_H; y++)
            for (int x = 0; x < T_W; x++)
                board[y][x] = 0;
        score = 0;
        lines = 0;
        level = 1;
        over = false;
        bagPos = 7;
        nextType = nextPiece();
        spawn();
    }

public:
    Tetris()
        : curType(0), curRot(0), curX(0), curY(0), nextType(0),
          score(0), lines(0), level(1), over(false),
          rngState(0x1234ABCDu), bagPos(7)
    {
    }

    void run()
    {
        timeInit();
        rngState ^= (u32)readPit() * 2654435761u;
        reset();

        drawFrame();
        drawStats();
        drawNext();
        drawBoard();

        u32 last = millis();
        bool paused = false;

        while (!over)
        {
            bool dirty = false;
            char k = keyboard.tryGetChar();

            if (k)
            {
                if (k == 'q' || k == 27)
                    return;

                if (k == 'p')
                {
                    paused = !paused;
                    last = millis();
                    if (paused)
                        console.writeAt(T_TOP + 8, T_LEFT + 8, "  JEDA  ", 0x4F);
                    else
                        dirty = true;
                }
                else if (!paused)
                {
                    if (k == 'a' && !collides(curType, curRot, curX - 1, curY))
                    {
                        curX--;
                        dirty = true;
                    }
                    else if (k == 'd' && !collides(curType, curRot, curX + 1, curY))
                    {
                        curX++;
                        dirty = true;
                    }
                    else if (k == 'w')
                    {
                        rotate();
                        dirty = true;
                    }
                    else if (k == 's')
                    {
                        stepDown();
                        score += 1;
                        last = millis();
                        dirty = true;
                    }
                    else if (k == ' ')
                    {
                        while (!collides(curType, curRot, curX, curY + 1))
                        {
                            curY++;
                            score += 2;
                        }
                        lockPiece();
                        last = millis();
                        dirty = true;
                    }
                }
            }

            if (!paused)
            {
                u32 now = millis();
                if (now - last >= dropInterval())
                {
                    last = now;
                    stepDown();
                    dirty = true;
                }
            }

            if (dirty)
            {
                drawBoard();
                drawStats();
                drawNext();
            }
        }

        // ----- game over -----
        drawBoard();
        drawStats();
        console.writeAt(T_TOP + 8, T_LEFT + 1, "     GAME OVER     ", 0x4F);
        console.writeAt(T_TOP + 10, T_LEFT + 4, "Tekan tombol", CLR_WHITE);

        // buang tombol yang masih tertahan, lalu tunggu tombol baru
        u32 until = millis() + 600;
        while (millis() < until)
            keyboard.tryGetChar();
        keyboard.getChar();
    }
};

static void runTetris()
{
    Tetris game;
    game.run();
}

#endif