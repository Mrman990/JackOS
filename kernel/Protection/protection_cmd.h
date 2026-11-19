
#pragma once

// Perintah shell untuk Protection:
// mode, exit, dan pengaturan izin.
// Include SETELAH syscall.h dan ramdisk_cmd.h.

static void protShowPerms()
{
    console.print("Izin mode USER:\n", CLR_RED);

    console.print(
        userPerms & PERM_ADD ? "  tambah : ON\n" : "  tambah : OFF\n",
        CLR_WHITE);

    console.print(
        userPerms & PERM_LIST ? "  barang : ON\n" : "  barang : OFF\n",
        CLR_WHITE);

    console.print(
        userPerms & PERM_UPDATE ? "  ubah   : ON\n" : "  ubah   : OFF\n",
        CLR_WHITE);

    console.print(
        userPerms & PERM_DELETE ? "  hapus  : ON\n" : "  hapus  : OFF\n",
        CLR_WHITE);

    console.print(
        userPerms & PERM_SAVE ? "  simpan : ON\n" : "  simpan : OFF\n",
        CLR_WHITE);
}

// izin <tambah|barang|ubah|hapus|simpan> <on|off>
static void protCmdIzin(const char *args)
{
    char a[16], b[16];

    if (!rdTwoWords(args, a, b))
    {
        console.print(
            "Pemakaian: izin <tambah|barang|ubah|hapus|simpan> <on|off>\n",
            CLR_RED);
        return;
    }

    u32 bit = 0;

    if (rdEq(a, "tambah"))
        bit = PERM_ADD;
    else if (rdEq(a, "barang"))
        bit = PERM_LIST;
    else if (rdEq(a, "ubah"))
        bit = PERM_UPDATE;
    else if (rdEq(a, "hapus"))
        bit = PERM_DELETE;
    else if (rdEq(a, "simpan"))
        bit = PERM_SAVE;

    if (bit == 0 || !(rdEq(b, "on") || rdEq(b, "off")))
    {
        console.print(
            "Pemakaian: izin <tambah|barang|ubah|hapus|simpan> <on|off>\n",
            CLR_RED);
        return;
    }

    if (rdEq(b, "on"))
        userPerms |= bit;
    else
        userPerms &= ~bit;

    protShowPerms();
}

// ============================================================
// PERINTAH PROTECTION
// ============================================================

static bool protectionCommand(const char *cmd)
{
    if (rdEq(cmd, "mode"))
    {
        console.print("Mode saat ini: ", CLR_WHITE);
        console.print(modeName(g_mode), CLR_RED);
        console.print("\n", CLR_WHITE);

        if (g_mode == MODE_USER)
        {
            console.print(
                "Ketik 'mode kernel' untuk pindah ke mode KERNEL.\n",
                CLR_WHITE);
        }
        else
        {
            console.print(
                "Ketik 'exit' untuk kembali ke mode USER.\n",
                CLR_WHITE);
        }
    }
    else if (rdEq(cmd, "mode kernel"))
    {
        if (g_mode == MODE_KERNEL)
        {
            console.print("Sudah di mode KERNEL.\n", CLR_WHITE);
        }
        else
        {
            g_mode = MODE_KERNEL;

            console.print(
                "Pindah ke mode KERNEL. Ketik 'help' untuk Menu KERNEL.\n",
                CLR_RED);
        }
    }
    else if (rdEq(cmd, "mode user") || rdEq(cmd, "exit"))
    {
        if (g_mode == MODE_USER)
        {
            console.print("Sudah di mode USER.\n", CLR_WHITE);
        }
        else
        {
            g_mode = MODE_USER;

            console.print(
                "Kembali ke mode USER. Ketik 'help' untuk Menu USER.\n",
                CLR_RED);
        }
    }
    else if (rdStarts(cmd, "mode "))
    {
        console.print(
            "Pemakaian: mode | mode kernel | mode user\n",
            CLR_RED);
    }
    else if (rdStarts(cmd, "izin "))
    {
        if (requireKernelCmd("izin"))
            protCmdIzin(cmd + 5);
    }
    else if (rdEq(cmd, "izin"))
    {
        if (requireKernelCmd("izin"))
            protShowPerms();
    }
    else
    {
        return false;
    }

    return true;
}
