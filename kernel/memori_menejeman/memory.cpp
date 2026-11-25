#include <stddef.h>

extern "C" void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *dest = (unsigned char *)destination;
    const unsigned char *src = (const unsigned char *)source;

    for (size_t i = 0; i < size; i++)
    {
        dest[i] = src[i];
    }

    return destination;
}