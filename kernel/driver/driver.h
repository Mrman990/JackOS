#pragma once
// Driver sederhana untuk perangkat layar dan keyboard.
// Include SETELAH protection.h (butuh console, keyboard, requireKernel).
//
// Driver adalah kode KERNEL: ditolak jika dipanggil dari mode USER.
// Aplikasi User memakai sys_print / sys_getchar / sys_read_line (sys_io.h).

// ---------- driver layar ----------
static bool drv_screen_print(const char *str, u8 clr)
{
    if (!requireKernel("drv_screen_print"))
        return false;
    console.print(str, clr);
    return true;
}


// Mengatur posisi kursor layar.
static bool drv_screen_set_cursor(int row, int col)
{
    if (!requireKernel("drv_screen_set_cursor"))
        return false;

    if (row < 0 || row >= 25 || col < 0 || col >= 80)
        return false;

    console.setCursor(row, col);
    return true;
}

// Menampilkan teks pada posisi tertentu di layar.
static bool drv_screen_write_at(
    int row, int col,
    const char *str,
    u8 clr)
{
    if (!requireKernel("drv_screen_write_at"))
        return false;

    if (str == nullptr || row < 0 || row >= 25 ||
        col < 0 || col >= 80)
        return false;

    // Batasi tulisan agar tidak melewati batas layar.
    int maxLen = 80 - col;

    for (int i = 0; str[i] != '\0' && i < maxLen; i++)
        console.putCell(row, col + i, str[i], clr);

    return true;
}

static inline bool drv_screen_putchar(char ch, u8 clr)
{
    if (!requireKernel("drv_screen_putchar"))
        return false;
    char s[2] = {ch, '\0'};
    console.print(s, clr);
    return true;
}

static bool drv_screen_print_num(u32 v, u8 clr)
{
    if (!requireKernel("drv_screen_print_num"))
        return false;
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
    return true;
}

// ---------- driver keyboard ----------
// Tunggu satu tombol (blocking). Mengembalikan 0 jika dipanggil dari USER.
static char drv_keyboard_getchar()
{
    if (!requireKernel("drv_keyboard_getchar"))
        return 0;
    return keyboard.getChar();
}

static bool drv_keyboard_try_getchar(char *out)
{
    if (!requireKernel("drv_keyboard_try_getchar"))
        return false;

    return keyboard.tryGetChar(out);
}


// Baca satu baris dengan echo dan backspace. Mengembalikan panjang baris,
// atau -1 jika ditolak.
static int drv_keyboard_readline(char *buf, int maxLen)
{
    if (!requireKernel("drv_keyboard_readline"))
        return -1;
    int len = 0;
    while (true)
    {
        char ch = keyboard.getChar();
        if (ch == '\n')
        {
            console.putChar('\n');
            buf[len] = '\0';
            return len;
        }
        else if (ch == '\b')
        {
            if (len > 0)
            {
                len--;
                console.putChar('\b');
            }
        }
        else if (ch >= 32 && ch < 127 && len < maxLen - 1)
        {
            buf[len++] = ch;
            console.setColor(CLR_WHITE);
            console.putChar(ch);
        }
    }
}