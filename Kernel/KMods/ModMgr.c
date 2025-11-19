#include <AllTypes.h>
#include <KHeap.h>
#include <KMods.h>
#include <KrnPrintf.h>
#include <ModMemMgr.h>
#include <String.h>
#include <VFS.h>

/*Globals*/
ModuleRecord* ModuleListHead = 0;

int
ModuleRegistryInit(void)
{
    ModuleListHead = ModuleListHead;
    return 0;
}

int
ModuleRegistryAdd(ModuleRecord* __Rec__)
{
    if (!__Rec__)
    {
        PError("MOD: Registry add invalid record\n");
        return -1;
    }

    __Rec__->Next  = ModuleListHead;
    ModuleListHead = __Rec__;
    return 0;
}

ModuleRecord*
ModuleRegistryFind(const char* __Name__)
{
    if (!__Name__)
    {
        PError("MOD: Registry find invalid name\n");
        return 0;
    }

    ModuleRecord* It = ModuleListHead;
    while (It)
    {
        if (It->Name && strcmp(It->Name, __Name__) == 0)
        {
            return It;
        }
        It = It->Next;
    }
    return 0;
}

int
ModuleRegistryRemove(ModuleRecord* __Rec__)
{
    if (!__Rec__)
    {
        PError("MOD: Registry remove invalid record\n");
        return -1;
    }

    ModuleRecord* Prev = 0;
    ModuleRecord* It   = ModuleListHead;
    while (It)
    {
        if (It == __Rec__)
        {
            if (Prev)
            {
                Prev->Next = It->Next;
            }
            else
            {
                ModuleListHead = It->Next;
            }
            return 0;
        }
        Prev = It;
        It   = It->Next;
    }
    PError("MOD: Registry remove not found\n");
    return -1;
}
