#include "memori_menejeman/kmalloc.hpp"

namespace mem
{
    constexpr size_t HEAP_SIZE = 512 * 1024; // 512 KB Heap
    alignas(16) unsigned char heap[HEAP_SIZE];

    constexpr uint32_t BLOCK_MAGIC = 0x4A41434B; // "JACK": penanda blok yang valid

    struct alignas(16) Block
    {
        size_t size;
        uint32_t free;
        Block *next;
        uint32_t magic; // muat di ruang padding, ukuran Block tetap 16 byte
    };
    static_assert(sizeof(Block) % 16 == 0, "Block harus kelipatan 16 byte");
    Block *head = nullptr;
    bool ready = false;

    constexpr size_t alignUp(size_t x) { return (x + 15) & ~size_t(15); }

    void init()
    {
        head = reinterpret_cast<Block *>(heap);
        head->size = HEAP_SIZE - sizeof(Block);
        head->free = 1;
        head->next = nullptr;
        head->magic = BLOCK_MAGIC;
        ready = true;
    }

    void *alloc(size_t size)
    {
        // Cek ukuran SEBELUM dibulatkan: kalau tidak, ukuran sangat besar
        // (mis. 0xFFFFFFFF) meluap jadi 0 dan malah lolos.
        if (!ready || size == 0 || size > HEAP_SIZE)
            return nullptr;
        size = alignUp(size);

        for (Block *b = head; b; b = b->next)
        {
            if (!b->free || b->size < size)
                continue;

            size_t remaining = b->size - size;
            if (remaining >= sizeof(Block) + 16)
            {
                Block *n = reinterpret_cast<Block *>(
                    reinterpret_cast<char *>(b) + sizeof(Block) + size);
                n->size = remaining - sizeof(Block);
                n->free = 1;
                n->magic = BLOCK_MAGIC;
                n->next = b->next;
                b->size = size;
                b->next = n;
            }
            b->free = 0;
            return reinterpret_cast<char *>(b) + sizeof(Block);
        }
        return nullptr; // Heap penuh
    }

    void free(void *ptr)
    {
        if (!ptr || !ready)
            return;
        // Tolak pointer di luar heap, supaya tidak menulis ke memori sembarang.
        unsigned char *p = reinterpret_cast<unsigned char *>(ptr);
        if (p < heap + sizeof(Block) || p >= heap + HEAP_SIZE)
            return;

        Block *b = reinterpret_cast<Block *>(p - sizeof(Block));
        // Tolak pointer yang bukan hasil alloc, atau blok yang sudah bebas (double free).
        if (b->magic != BLOCK_MAGIC || b->free)
            return;
        b->free = 1;

        for (Block *curr = head; curr && curr->next;)
        {
            if (curr->free && curr->next->free)
            {
                curr->size += sizeof(Block) + curr->next->size;
                curr->next = curr->next->next;
            }
            else
            {
                curr = curr->next;
            }
        }
    }

    size_t used()
    {
        size_t u = 0;
        for (Block *b = head; b; b = b->next)
            if (!b->free)
                u += b->size;
        return u;
    }

    size_t total() { return HEAP_SIZE; }
} // <-- INI PENUTUP NAMESPACE MEM

// Jembatan C
extern "C" void *kmalloc(size_t size) { return mem::alloc(size); }
extern "C" void kfree(void *ptr) { mem::free(ptr); }

// Operator C++
void *operator new(size_t size) { return mem::alloc(size); }
void *operator new[](size_t size) { return mem::alloc(size); }
void operator delete(void *p) noexcept { mem::free(p); }
void operator delete[](void *p) noexcept { mem::free(p); }

// Fungsi wajib linker
extern "C" void __cxa_pure_virtual()
{
    for (;;)
        ;
}
extern "C" void *__dso_handle = 0;