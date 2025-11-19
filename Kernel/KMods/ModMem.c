#include <ModMemMgr.h>

/* Global module memory manager state */
ModuleMemoryManager ModMem = {0, 0, 0};

void
ModMemInit(void)
{
    /* Reset allocation cursors */
    ModMem.TextCursor  = 0;
    ModMem.DataCursor  = 0;
    ModMem.Initialized = 1;

    /* Log successful initialization */
    PSuccess("[MOD]: Arena Initialized\n");

    /* Debug: show memory ranges */
    PDebug("[MOD]: Text=%#llx..%#llx Data=%#llx..%#llx\n",
           (unsigned long long)ModTextBase,
           (unsigned long long)(ModTextBase + ModTextSize - 1),
           (unsigned long long)ModDataBase,
           (unsigned long long)(ModDataBase + ModDataSize - 1));
}

void*
ModMalloc(size_t __Size__, int __IsText__)
{
    /* Validate parameters and state */
    if (!ModMem.Initialized || __Size__ == 0)
    {
        return NULL;
    }

    /* Calculate number of pages needed */
    size_t Pages = (__Size__ + PageSize - 1) / PageSize;

    /* Determine allocation range based on section type */
    uint64_t Start =
        __IsText__ ? (ModTextBase + ModMem.TextCursor) : (ModDataBase + ModMem.DataCursor);
    uint64_t End   = Start + Pages * PageSize;
    uint64_t Limit = __IsText__ ? (ModTextBase + ModTextSize) : (ModDataBase + ModDataSize);

    /* Check for out of memory */
    if (End > Limit)
    {
        PError("[MOD]: Out of space (req=%zu pages)\n", Pages);
        return NULL;
    }

    /* Allocate and map each page */
    for (size_t I = 0; I < Pages; ++I)
    {
        /* Get a free physical page */
        uint64_t Phys = AllocPage();
        if (!Phys)
        {
            PError("[MOD]: AllocPage failed\n");
            return NULL;
        }

        /* Calculate virtual address */
        uint64_t Virt = Start + I * PageSize;

        /* Set page table flags based on section type */
        uint64_t Flags = PTEPRESENT | PTEGLOBAL;
        if (__IsText__)
        {
            Flags |= PTEWRITABLE; /* allow section copy */
        }
        else
        {
            /* Data/Rodata/Bss: writable + NX */
            Flags |= PTEWRITABLE;
            Flags |= PTENOEXECUTE;
        }

        /* Map the page */
        if (MapPage(Vmm.KernelSpace, Virt, Phys, Flags) == 0)
        {
            PError("[MOD]: MapPage failed @%#llx\n", (unsigned long long)Virt);
            return NULL;
        }
    }

    /* Update allocation cursor */
    if (__IsText__)
    {
        ModMem.TextCursor += Pages * PageSize;
    }
    else
    {
        ModMem.DataCursor += Pages * PageSize;
    }

    /* Debug logging */
    PDebug(
        "[MOD]: Alloc %zu pages at %p (%s)\n", Pages, (void*)Start, __IsText__ ? "Text" : "Data");

    return (void*)Start;
}

void
ModFree(void* __Addr__, size_t __Size__)
{
    /* Validate parameters */
    if (!__Addr__ || __Size__ == 0)
    {
        return;
    }

    /* Calculate number of pages */
    size_t   Pages = (__Size__ + PageSize - 1) / PageSize;
    uint64_t Virt  = (uint64_t)__Addr__;

    /* Unmap and free each page */
    for (size_t I = 0; I < Pages; ++I)
    {
        uint64_t Va   = Virt + I * PageSize;
        uint64_t Phys = GetPhysicalAddress(Vmm.KernelSpace, Va);
        if (Phys)
        {
            UnmapPage(Vmm.KernelSpace, Va);
            FreePage(Phys);
        }
    }

    /* Debug logging */
    PDebug("[MOD]: Freed %zu pages at %p\n", Pages, __Addr__);
}
