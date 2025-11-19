#include <PMM.h>

void
ParseMemoryMap(void)
{
    if (!MemmapRequest.response)
    {
        PError("Failed to get memory map from Limine\n");
        return;
    }

    PInfo("Parsing memory map (%lu entries)...\n", MemmapRequest.response->entry_count);

    Pmm.RegionCount      = 0;
    uint64_t HighestAddr = 0;

    /*Process each memory map entry*/
    for (uint64_t Index = 0; Index < MemmapRequest.response->entry_count; Index++)
    {
        struct limine_memmap_entry* Entry = MemmapRequest.response->entries[Index];

        if (Pmm.RegionCount >= MaxMemoryRegions)
        {
            PWarn("Too many memory regions, truncating at %u\n", MaxMemoryRegions);
            break;
        }

        /*Store region information*/
        Pmm.Regions[Pmm.RegionCount].Base   = Entry->base;
        Pmm.Regions[Pmm.RegionCount].Length = Entry->length;

        /*Classify region type based on Limine type*/
        switch (Entry->type)
        {
            case LIMINE_MEMMAP_USABLE:
                Pmm.Regions[Pmm.RegionCount].Type = MemoryTypeUsable;
                break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                Pmm.Regions[Pmm.RegionCount].Type = MemoryTypeKernel;
                break;
            default:
                Pmm.Regions[Pmm.RegionCount].Type = MemoryTypeReserved;
                break;
        }

        /*Track the highest address seen for total memory calculation*/
        uint64_t EndAddr = Entry->base + Entry->length;
        if (EndAddr > HighestAddr)
        {
            HighestAddr = EndAddr;
        }

        Pmm.RegionCount++;

        PDebug("Region %lu: 0x%016lx-0x%016lx Type=%u\n",
               Index,
               Entry->base,
               EndAddr,
               Pmm.Regions[Pmm.RegionCount - 1].Type);
    }

    /*Calculate total pages in system (round up)*/
    Pmm.TotalPages = (HighestAddr + PageSize - 1) / PageSize;
    PInfo(
        "Total pages: %lu (%lu MB)\n", Pmm.TotalPages, (Pmm.TotalPages * PageSize) / (1024 * 1024));
}

void
MarkMemoryRegions(void)
{
    PInfo("Marking memory regions...\n");

    /*Start with all pages marked as used (safe default)*/
    for (uint64_t Index = 0; Index < Pmm.TotalPages; Index++)
    {
        SetBitmapBit(Index);
    }

    /*Mark usable regions as available for allocation*/
    uint64_t TotalFreePages = 0;
    for (uint32_t RegionIndex = 0; RegionIndex < Pmm.RegionCount; RegionIndex++)
    {
        if (Pmm.Regions[RegionIndex].Type == MemoryTypeUsable)
        {
            uint64_t StartPage = Pmm.Regions[RegionIndex].Base / PageSize;
            uint64_t PageCount = Pmm.Regions[RegionIndex].Length / PageSize;

            /*Mark each page in the region as free*/
            for (uint64_t Page = StartPage; Page < StartPage + PageCount; Page++)
            {
                if (Page < Pmm.TotalPages)
                {
                    ClearBitmapBit(Page);
                }
            }

            TotalFreePages += PageCount;
            PDebug("Marked %lu pages free at 0x%016lx\n", PageCount, Pmm.Regions[RegionIndex].Base);
        }
    }

    /*Protect the bitmap itself from allocation*/
    uint64_t BitmapPhys      = VirtToPhys(Pmm.Bitmap);
    uint64_t BitmapStartPage = BitmapPhys / PageSize;
    uint64_t BitmapPageCount = (Pmm.BitmapSize * sizeof(uint64_t) + PageSize - 1) / PageSize;

    for (uint64_t Page = BitmapStartPage; Page < BitmapStartPage + BitmapPageCount; Page++)
    {
        SetBitmapBit(Page);
    }

    PInfo("Protected %lu bitmap pages from allocation\n", BitmapPageCount);
    PSuccess("Memory regions marked: %lu pages available\n", TotalFreePages - BitmapPageCount);
}
