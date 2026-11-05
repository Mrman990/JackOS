// test
class VGAConsole
{
private:
    volatile unsigned char *video_memory;
    const unsigned char default_color = 0x07;
public:
    VGAConsole() : video_memory((volatile unsigned char *)0xb8000) {}

    void clear()
    {
        for (int i = 0; i < 80 * 25 * 2; i += 2)
        {
            video_memory[i] = ' ';
            video_memory[i + 1] = default_color;
        }
    }

    void writeString(int row, int col, const char *str, unsigned char color)
    {
        int offset = (row * 80 + col) * 2;
        int i = 0;
        while (str[i] != '\0')
        {
            video_memory[offset] = str[i];
            video_memory[offset + 1] = color;
            i++;
            offset += 2;
        }
    }
};
extern "C" void kmain(void)
{
    VGAConsole console;
    console.clear();

    // Logo ASCII "JACK OS" (lebar 46 kolom, ditengahkan di kolom 17)
    console.writeString(2, 17, "     _     _      ____  _  __     ___   ____  ", 0x0A);
    console.writeString(3, 17, "    | |   / \\    / ___|| |/ /    / _ \\ / ___| ", 0x0A);
    console.writeString(4, 17, " _  | |  / _ \\  | |    | ' /    | | | |\\___ \\ ", 0x0A);
    console.writeString(5, 17, "| |_| | / ___ \\ | |___ | . \\    | |_| | ___) |", 0x0A);
    console.writeString(6, 17, " \\___/ /_/   \\_\\ \\____||_|\\_\\    \\___/ |____/ ", 0x0A);

    console.writeString(9, 25, "=== JACK_OS C++ KERNEL v1.0 ===", 0x0E); // Kuning terang
    console.writeString(11, 10, "Status: Booted successfully with C++ Class Architecture!", 0x0F);
    console.writeString(13, 10, "Interactive Shell Ready.", 0x0B);
    console.writeString(15, 10, "JACK_OS:> _", 0x0A);

    while (1)
    {
    }
}