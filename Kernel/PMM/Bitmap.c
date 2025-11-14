#include <PMM.h>

/**
 * @brief Initialize the PMM allocation bitmap.
 *
 * @details Calculates required bitmap size based on total pages.
 * 			Finds a suitable usable memory region to store the bitmap.
 * 			Maps the bitmap into virtual memory.
 * 			Initializes all bits to zero (free).
 *
 * @return void
 *
 * @note Must be called before any allocations are performed.
 */
void
InitializeBitmap(void)
{
    /*Calculate bitmap size in 64-bit entries*/
    Pmm.BitmapSize = (Pmm.TotalPages + BitsPerUint64 - 1) / BitsPerUint64;
    uint64_t BitmapBytes = Pmm.BitmapSize * sizeof(uint64_t);

    PInfo("Bitmap requires %lu KB for %lu pages\n", BitmapBytes / 1024, Pmm.TotalPages);

    /*Find a usable memory region large enough for the bitmap*/
    uint64_t BitmapPhys = 0;
    for (uint32_t Index = 0; Index < Pmm.RegionCount; Index++)
    {
        if (Pmm.Regions[Index].Type == MemoryTypeUsable &&
            Pmm.Regions[Index].Length >= BitmapBytes)
        {
            BitmapPhys = Pmm.Regions[Index].Base;
            PDebug("Found bitmap location in region %u\n", Index);
            break;
        }
    }

    if (BitmapPhys == 0)
    {
        PError("No suitable region for PMM bitmap\n");
        return;
    }

    /*Map bitmap physical address to virtual address for access*/
    Pmm.Bitmap = (uint64_t*)PhysToVirt(BitmapPhys);

    /*Initialize all bits to 0 (free)*/
    for (uint64_t Index = 0; Index < Pmm.BitmapSize; Index++)
        Pmm.Bitmap[Index] = 0;

    PSuccess("PMM bitmap initialized at 0x%016lx\n", BitmapPhys);
}

/**
 * @brief Mark a page as used in the bitmap.
 *
 * @param __PageIndex__ Index of the page to mark.
 *
 * @return void
 */
void
SetBitmapBit(uint64_t __PageIndex__)
{
    uint64_t ByteIndex = __PageIndex__ / BitsPerUint64;
    uint64_t BitIndex = __PageIndex__ % BitsPerUint64;
    Pmm.Bitmap[ByteIndex] |= (1ULL << BitIndex);
}

/**
 * @brief Mark a page as free in the bitmap.
 *
 * @param __PageIndex__ Index of the page to clear.
 *
 * @return void
 */
void
ClearBitmapBit(uint64_t __PageIndex__)
{
    uint64_t ByteIndex = __PageIndex__ / BitsPerUint64;
    uint64_t BitIndex = __PageIndex__ % BitsPerUint64;
    Pmm.Bitmap[ByteIndex] &= ~(1ULL << BitIndex);
}

/**
 * @brief Test whether a page is marked as used.
 *
 * @param __PageIndex__ Index of the page to test.
 *
 * @return 1 if the page is used, 0 if free. (Bool?)
 */
int
TestBitmapBit(uint64_t __PageIndex__)
{
    uint64_t ByteIndex = __PageIndex__ / BitsPerUint64;
    uint64_t BitIndex = __PageIndex__ % BitsPerUint64;
    return (Pmm.Bitmap[ByteIndex] & (1ULL << BitIndex)) != 0;
}
