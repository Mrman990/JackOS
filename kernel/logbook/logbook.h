#pragma once
#define LOG_MAX 100 
#define LOG_PENDING_MAX 4
#define LOG_TZ_OFFSET_HOURS 8

enum LogKind
{
    LOG_MASUK = 0,
    LOG_TAMBAH,
    LOG_UBAH,
    LOG_HAPUS,
    LOG_KELUAR
};

struct LogEntry
{
    u32 no;
    u32 jam, menit, detik;
    u32 kind;
    u32 itemId;
    char name[RD_NAME_LEN];
    u32 oldStock;
    u32 newStock;
};

static LogEntry logBook[LOG_MAX];
static LogEntry logPend[LOG_PENDING_MAX];
static int logBookCount;
static int logBookNext;
static int logPendCount;
static u32 logSeq;
static thread_t *logThread;
static bool logClosing;

// ---------- jam CMOS (RTC) ----------
static u8 logCmos(u8 reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

static u32 logBcd(u32 v)
{
    return (v & 0x0F) + ((v >> 4) * 10);
}

static void logNow(u32 &jam, u32 &menit, u32 &detik)
{
    for (int i = 0; i < 100000 && (logCmos(0x0A) & 0x80); i++)
    {
    }
    u32 s = logCmos(0x00);
    u32 m = logCmos(0x02);
    u32 h = logCmos(0x04);
    u8 b = logCmos(0x0B);
    bool pm = (h & 0x80) != 0;
    h &= 0x7F;
    if (!(b & 0x04))
    {
        s = logBcd(s);
        m = logBcd(m);
        h = logBcd(h);
    }
    if (!(b & 0x02))
    {
        if (h == 12)
            h = 0;
        if (pm)
            h += 12;
    }
    jam = (h + LOG_TZ_OFFSET_HOURS) % 24;
    menit = m;
    detik = s;
}

static void logCopy(LogEntry &d, const LogEntry &s)
{
    d.no = s.no;
    d.jam = s.jam;
    d.menit = s.menit;
    d.detik = s.detik;
    d.kind = s.kind;
    d.itemId = s.itemId;
    for (int i = 0; i < RD_NAME_LEN; i++)
        d.name[i] = s.name[i];
    d.oldStock = s.oldStock;
    d.newStock = s.newStock;
}

static void logWrite(const LogEntry &e)
{
    logCopy(logBook[logBookNext], e);
    logBook[logBookNext].no = ++logSeq;
    logBookNext = (logBookNext + 1) % LOG_MAX;   // ← tetap wrap 99 → 0
    if (logBookCount < LOG_MAX)
        logBookCount++;
}

static void logDrain()
{
    while (logPendCount > 0)
    {
        logWrite(logPend[0]);
        for (int i = 1; i < logPendCount; i++)
            logCopy(logPend[i - 1], logPend[i]);
        logPendCount--;
    }
}
static void loggerTask()
{
    for (;;)
    {
        logDrain();
        if (logClosing)
            return;
        thread_wait_event();
    }
}

static void logPush(u32 kind, u32 id, const char *name, u32 oldS, u32 newS)
{
    LogEntry e;
    e.no = 0;
    logNow(e.jam, e.menit, e.detik);
    e.kind = kind;
    e.itemId = id;
    for (int i = 0; i < RD_NAME_LEN; i++)
        e.name[i] = '\0';
    for (int i = 0; name && name[i] && i < RD_NAME_LEN - 1; i++)
        e.name[i] = name[i];
    e.oldStock = oldS;
    e.newStock = newS;

    if (logPendCount >= LOG_PENDING_MAX)
    {
        logWrite(e);
        return;
    }
    logCopy(logPend[logPendCount], e);
    logPendCount++;
}

static void logFlush()
{
    if (logThread)
    {
        if (logThread->state == THREAD_WAITING)
            thread_wake(logThread);
        scheduler_run();
    }
    else
    {
        logDrain();
    }
}

static void logbook_open()
{
    if (logThread)
        return;
    logClosing = false;
    logThread = create_thread("Logger", loggerTask);
    logPush(LOG_MASUK, 0, "", 0, 0);
    logFlush();
}

static void logbook_record(u32 kind, u32 id, const char *name, u32 oldS, u32 newS)
{
    logPush(kind, id, name, oldS, newS);
    logFlush();
}

static void logbook_close()
{
    logClosing = true;
    logPush(LOG_KELUAR, 0, "", 0, 0);
    logFlush();
    logThread = nullptr;
    logClosing = false;
}

static void logTwo(u32 v)
{
    if (v < 10)
        console.print("0", CLR_WHITE);
    ramdiskPrintNum(v, 0);
}

static void logbook_show()
{
    console.print("=== BUKU LOG INVENTORY ===\n", CLR_RED);
    if (logBookCount == 0)
    {
        console.print("(buku log masih kosong)\n", CLR_WHITE);
    }
    else
    {
        console.print("NO   JAM       AKSI    ID  NAMA                 DETAIL\n", CLR_RED);
        int oldest = (logBookNext - logBookCount + LOG_MAX) % LOG_MAX;
        for (int i = 0; i < logBookCount; i++)
        {
            const LogEntry &e = logBook[(oldest + i) % LOG_MAX];
            ramdiskPrintNum(e.no, 5);
            logTwo(e.jam);
            console.print(":", CLR_WHITE);
            logTwo(e.menit);
            console.print(":", CLR_WHITE);
            logTwo(e.detik);
            console.print("  ", CLR_WHITE);

            static const char *const kindName[5] = {"MASUK  ", "TAMBAH ", "UBAH   ", "HAPUS  ", "KELUAR "};
            console.print(kindName[e.kind], CLR_RED);
            console.print(" ", CLR_WHITE);

            if (e.kind == LOG_MASUK)
            {
                console.print("Manajemen inventory dibuka\n", CLR_WHITE);
            }
            else if (e.kind == LOG_KELUAR)
            {
                console.print("Manajemen inventory ditutup\n", CLR_WHITE);
            }
            else
            {
                ramdiskPrintNum(e.itemId, 4);
                console.print(e.name, CLR_WHITE);
                int len = 0;
                while (e.name[len])
                    len++;
                for (int c = len; c < 21; c++)
                    console.print(" ", CLR_WHITE);
                if (e.kind == LOG_TAMBAH)
                {
                    console.print("stok ", CLR_WHITE);
                    ramdiskPrintNum(e.newStock, 0);
                }
                else if (e.kind == LOG_UBAH)
                {
                    ramdiskPrintNum(e.oldStock, 0);
                    console.print(" -> ", CLR_WHITE);
                    ramdiskPrintNum(e.newStock, 0);
                }
                else
                {
                    console.print("stok terakhir ", CLR_WHITE);
                    ramdiskPrintNum(e.oldStock, 0);
                }
                console.print("\n", CLR_WHITE);
            }
        }
    }
    console.print("Total: ", CLR_WHITE);
    ramdiskPrintNum((u32)logBookCount, 0);
    console.print("Catatan Thread Logger: ", CLR_WHITE);
    console.print(logThread ? thread_state_name(logThread->state) : "tidak aktif (tidak ada sesi inventory)", CLR_RED);
    console.print("\n", CLR_WHITE);
}