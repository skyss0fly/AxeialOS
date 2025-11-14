#include <PMM.h>

/** @brief Global PMM state */
PhysicalMemoryManager Pmm = {0};

/**
 * @brief Find the next free physical page.
 *
 * @details Searches the allocation bitmap for a free page, starting from the
 * 			last allocation hint. Wraps around if necessary.
 *
 * @return Index of the free page, or PmmBitmapNotFound if none available.
 */
uint64_t
FindFreePage(void)
{
    uint64_t StartHint = Pmm.LastAllocHint;

    /*Search from hint forward to end of memory*/
    for (uint64_t Index = StartHint; Index < Pmm.TotalPages; Index++)
    {
        if (!TestBitmapBit(Index))
        {
            Pmm.LastAllocHint = Index + 1;
            return Index;
        }
    }

    /*Search from beginning to hint if not found above*/
    for (uint64_t Index = 0; Index < StartHint; Index++)
    {
        if (!TestBitmapBit(Index))
        {
            Pmm.LastAllocHint = Index + 1;
            return Index;
        }
    }

    return PmmBitmapNotFound;
}

/**
 * @brief Initialize the Physical Memory Manager.
 *
 * @details Retrieves HHDM offset from Limine.
 * 			Parses the system memory map.
 * 			Initializes the allocation bitmap.
 * 			Marks memory regions as used/free.
 * 			Calculates memory statistics.
 *
 * @return void
 *
 * @note Must be called during kernel initialization before any allocations.
 */
void
InitializePmm(void)
{
    PInfo("Initializing Physical Memory Manager...\n");

    /*Retrieve HHDM offset for address translation*/
    if (!HhdmRequest.response)
    {
        PError("Failed to get HHDM from Limine\n");
        return;
    }
    Pmm.HhdmOffset = HhdmRequest.response->offset;
    PDebug("HHDM offset: 0x%016lx\n", Pmm.HhdmOffset);

    /*Parse system memory map from bootloader*/
    ParseMemoryMap();
    if (Pmm.RegionCount == 0)
    {
        PError("No memory regions found\n");
        return;
    }

    /*Initialize the allocation bitmap*/
    InitializeBitmap();
    if (Pmm.Bitmap == 0)
    {
        PError("Failed to initialize PMM bitmap\n");
        return;
    }

    /*Mark memory regions as used/free based on their type*/
    MarkMemoryRegions();

    /*Calculate final memory statistics*/
    Pmm.Stats.TotalPages = Pmm.TotalPages;
    Pmm.Stats.UsedPages = 0;
    Pmm.Stats.FreePages = 0;

    for (uint64_t Index = 0; Index < Pmm.TotalPages; Index++)
    {
        if (TestBitmapBit(Index))
            Pmm.Stats.UsedPages++;
        else
            Pmm.Stats.FreePages++;
    }

    PSuccess("PMM initialized: %lu MB total, %lu MB free\n",
    (Pmm.Stats.TotalPages * PageSize) / (1024 * 1024),
    (Pmm.Stats.FreePages * PageSize) / (1024 * 1024));
}

/**
 * @brief Allocate a single physical page.
 *
 * @details Finds a free page, marks it as used in the bitmap, updates statistics,
 * 			and returns its physical address.
 *
 * @return Physical address of the allocated page, or 0 if none available.
 */
uint64_t
AllocPage(void)
{
    uint64_t PageIndex = FindFreePage();

    if (PageIndex == PmmBitmapNotFound)
    {
        PError("Out of physical memory - no free pages available\n");
        return 0;
    }

    /*Mark page as used in bitmap*/
    SetBitmapBit(PageIndex);
    Pmm.Stats.UsedPages++;
    Pmm.Stats.FreePages--;

    uint64_t PhysAddr = PageIndex * PageSize;
    PDebug("Allocated page: 0x%016lx (index %lu)\n", PhysAddr, PageIndex);

    return PhysAddr;
}

/**
 * @brief Free a single physical page.
 *
 * @details Validates the physical address, checks for double free,
 * 			clears the bitmap entry, and updates statistics.
 *
 * @param __PhysAddr__ Physical address of the page to free.
 *
 * @return void
 */
void
FreePage(uint64_t __PhysAddr__)
{
    if (!PmmValidatePage(__PhysAddr__))
    {
        PError("Invalid physical address for free: 0x%016lx\n", __PhysAddr__);
        return;
    }

    uint64_t PageIndex = __PhysAddr__ / PageSize;

    if (!TestBitmapBit(PageIndex))
    {
        PError("Double free detected at: 0x%016lx\n", __PhysAddr__);
        return;
    }

    /*Mark page as free in bitmap*/
    ClearBitmapBit(PageIndex);
    Pmm.Stats.UsedPages--;
    Pmm.Stats.FreePages++;

    PDebug("Freed page: 0x%016lx (index %lu)\n", __PhysAddr__, PageIndex);
}

/**
 * @brief Allocate multiple contiguous physical pages.
 *
 * @details Searches for a contiguous block of free pages of the requested size.
 * 			Marks them as used and returns the base physical address.
 *
 * @param __Count__ Number of pages to allocate.
 *
 * @return Physical base address of the allocated block, or 0 if failed.
 */
uint64_t
AllocPages(size_t __Count__)
{
    if (__Count__ == 0)
    {
        PWarn("Attempted to allocate 0 pages\n");
        return 0;
    }

    if (__Count__ == 1)
        return AllocPage();

    if (__Count__ > Pmm.Stats.FreePages)
    {
        PError("Not enough free pages: requested %lu, available %lu\n", __Count__, Pmm.Stats.FreePages);
        return 0;
    }

    PDebug("Searching for %lu contiguous pages...\n", __Count__);

    /*Search for contiguous free block*/
    for (uint64_t StartIndex = 0; StartIndex <= Pmm.TotalPages - __Count__; StartIndex++)
    {
        int Found = 1;

        /*Check if all pages in range are free*/
        for (size_t Offset = 0; Offset < __Count__; Offset++)
        {
            if (TestBitmapBit(StartIndex + Offset))
            {
                Found = 0;
                break;
            }
        }

        if (Found)
        {
            /*Mark all pages in block as used*/
            for (size_t Offset = 0; Offset < __Count__; Offset++)
                SetBitmapBit(StartIndex + Offset);

            Pmm.Stats.UsedPages += __Count__;
            Pmm.Stats.FreePages -= __Count__;

            uint64_t PhysAddr = StartIndex * PageSize;
            PDebug("Allocated %lu contiguous pages at: 0x%016lx\n", __Count__, PhysAddr);

            return PhysAddr;
        }
    }

    PError("Failed to find %lu contiguous pages\n", __Count__);
    return 0;
}

/**
 * @brief Free multiple contiguous physical pages.
 *
 * @details Frees each page in the block individually, updating statistics.
 *
 * @param __PhysAddr__ Base physical address of the block.
 * @param __Count__    Number of pages to free.
 *
 * @return void
 */
void
FreePages(uint64_t __PhysAddr__, size_t __Count__)
{
    if (__Count__ == 0)
    {
        PWarn("Attempted to free 0 pages\n");
        return;
    }

    PDebug("Freeing %lu pages starting at 0x%016lx\n", __Count__, __PhysAddr__);

    /*Free each page individually*/
    for (size_t Index = 0; Index < __Count__; Index++)
        FreePage(__PhysAddr__ + (Index * PageSize));
}

/**
 * @brief Validate a physical page address.
 *
 * @details Checks that the address is non-zero, page-aligned, and within
 * 			the total page count managed by the PMM.
 *
 * @param __PhysAddr__ Physical address to validate.
 *
 * @return 1 if valid, 0 otherwise.
 */
int
PmmValidatePage(uint64_t __PhysAddr__)
{
    if (__PhysAddr__ == 0)
    {
        PDebug("Invalid page address: NULL\n");
        return 0;
    }
    if ((__PhysAddr__ % PageSize) != 0)
    {
        PDebug("Invalid page address: not aligned to %u bytes\n", PageSize);
        return 0;
    }
    if ((__PhysAddr__ / PageSize) >= Pmm.TotalPages)
    {
        PDebug("Invalid page address: beyond total pages\n");
        return 0;
    }
    return 1;
}
