#pragma once
#include <stddef.h>
#include <stdint.h>

namespace mem
{
    void init();
    void *alloc(size_t size);
    void free(void *ptr);
    size_t used();
    size_t total();
}

extern "C"
{
    void *kmalloc(size_t size);
    void kfree(void *ptr);
}

void *operator new(size_t size);
void *operator new[](size_t size);
void operator delete(void *p) noexcept;
void operator delete[](void *p) noexcept;