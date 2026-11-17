#pragma once
// Perintah shell untuk Protection: mode, proteksi, izin.
// Include SETELAH syscall.h dan ramdisk_cmd.h. Di runCommand tambah:
//     else if (protectionCommand(cmd)) { }

static void protShowPerms()
{
    console.print("Izin mode USER:\n", CLR_RED);
    console.print(userPerms & PERM_ADD ? "  tambah : ON\n" : "  tambah : OFF\n", CLR_WHITE);
    console.print(userPerms & PERM_LIST ? "  barang : ON\n" : "  barang : OFF\n", CLR_WHITE);
    console.print(userPerms & PERM_UPDATE ? "  ubah   : ON\n" : "  ubah   : OFF\n", CLR_WHITE);
    console.print(userPerms & PERM_DELETE ? "  hapus  : ON\n" : "  hapus  : OFF\n", CLR_WHITE);
}

// Demo: USER mencoba memanggil fungsi kernel langsung (ditolak),
// lalu lewat gerbang system call (diterima).
static void protDemo()
{
    console.print("=== DEMO PROTEKSI ===\n", CLR_RED);
    console.print("Mode sekarang: ", CLR_WHITE);
    console.print(modeName(g_mode), CLR_WHITE);
    console.print("\n1) Panggil ramdisk_add() langsung dari USER:\n", CLR_WHITE);
    RdResult r = ramdisk_add("Uji Proteksi", 1);
    printResult("  hasil: ditolak", r == RD_ACCESS_DENIED);

    console.print("2) Panggil lewat gerbang sys_add_item():\n", CLR_WHITE);
    u32 id = 0;
    r = sys_add_item("Uji Proteksi", 1, &id);
    printResult("  hasil: diterima", r == RD_OK);
    if (r == RD_OK)
    {
        sys_delete_item(id); // bersihkan barang uji
        console.print("  (barang uji sudah dihapus lagi)\n", CLR_WHITE);
    }
    console.print("Mode setelah syscall: ", CLR_WHITE);
    console.print(modeName(g_mode), CLR_WHITE);
    console.print("\n=== DEMO SELESAI ===\n", CLR_RED);
}

// izin <tambah|barang|ubah|hapus> <on|off>
static void protCmdIzin(const char *args)
{
    char a[16], b[16];
    if (!rdTwoWords(args, a, b))
    {
        console.print("Pemakaian: izin <tambah|barang|ubah|hapus> <on|off>\n", CLR_RED);
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
    if (bit == 0 || !(rdEq(b, "on") || rdEq(b, "off")))
    {
        console.print("Pemakaian: izin <tambah|barang|ubah|hapus> <on|off>\n", CLR_RED);
        return;
    }
    if (rdEq(b, "on"))
        userPerms |= bit;
    else
        userPerms &= ~bit;
    protShowPerms();
}

static bool protectionCommand(const char *cmd)
{
    if (rdEq(cmd, "mode"))
    {
        console.print("Mode saat ini: ", CLR_WHITE);
        console.print(modeName(g_mode), CLR_RED);
        console.print("\n", CLR_WHITE);
        protShowPerms();
    }
    else if (rdEq(cmd, "proteksi"))
        protDemo();
    else if (rdStarts(cmd, "izin "))
        protCmdIzin(cmd + 5);
    else if (rdEq(cmd, "izin"))
        protShowPerms();
    else
        return false;
    return true;
}
