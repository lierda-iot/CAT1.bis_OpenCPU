#include <stdint.h>

extern uint32_t Image$$LOAD_DRAM_SHARED$$ZI$$Base;
extern uint32_t Image$$LOAD_DRAM_SHARED$$ZI$$Limit;
extern uint32_t Load$$LOAD_DRAM_SHARED$$Base;
extern uint32_t Image$$LOAD_DRAM_SHARED$$Base;
extern uint32_t Image$$LOAD_DRAM_SHARED$$Length;

void setZiDataToZero(void)
{
    uint32_t *start_addr = &(Image$$LOAD_DRAM_SHARED$$ZI$$Base);
    uint32_t *end_addr   = &(Image$$LOAD_DRAM_SHARED$$ZI$$Limit);
    /* Linker boundaries are 4-byte aligned. Count uint32_t elements here:
     * using the byte length as an array index clears four times the ZI area. */
    uint32_t length      = ((uint32_t)end_addr - (uint32_t)start_addr) / sizeof(*start_addr);

    while (length > 0)
    {
        start_addr[length - 1] = 0;
        length--;
    }
}

void copyRwDataFromBin(void)
{
    uint32_t *src   = &(Load$$LOAD_DRAM_SHARED$$Base);
    uint32_t *dst   = &(Image$$LOAD_DRAM_SHARED$$Base);
    uint32_t length = (uint32_t)(&(Image$$LOAD_DRAM_SHARED$$Length)) / sizeof(uint32_t);

    if (dst != src)
    {
        while (length > 0)
        {
            dst[length - 1] = src[length - 1];
            length--;
        }
    }
}
