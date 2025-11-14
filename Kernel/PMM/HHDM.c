#include <PMM.h>

/**
 * @brief Convert a physical address to its virtual counterpart.
 *
 * @details Adds the HHDM offset to the physical address to obtain the
 * 			corresponding higher-half virtual address.
 *
 * @param __PhysAddr__ Physical address to convert.
 *
 * @return Virtual address as a pointer.
 */
void*
PhysToVirt(uint64_t __PhysAddr__)
{
    return (void*)(__PhysAddr__ + Pmm.HhdmOffset);
}

/**
 * @brief Convert a virtual address to its physical counterpart.
 *
 * @details Subtracts the HHDM offset from the virtual address to obtain
 * 			the original physical address.
 *
 * @param __VirtAddr__ Virtual address to convert.
 *
 * @return Physical address as a 64-bit integer.
 */
uint64_t
VirtToPhys(void* __VirtAddr__)
{
    return (uint64_t)__VirtAddr__ - Pmm.HhdmOffset;
}
