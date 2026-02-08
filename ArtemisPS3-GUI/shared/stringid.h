#include <stdint.h>

static uint32_t stringid(const char* str, uint32_t base)
{
    if (!base)
    {
        base = 0x811c9dc5;
    }
    while (*str)
    {
        base = 0x01000193 * (base ^ *str++);
    }
    return base;
}
