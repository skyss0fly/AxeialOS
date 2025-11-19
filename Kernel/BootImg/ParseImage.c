
#include <BootImg.h>
#include <RamFs.h>
#include <String.h>
#include <VFS.h>

int
InitializeBootImage(void)
{
    if (!LimineMod.response || LimineMod.response->module_count == 0)
    {
        PError("RamFS: No modules provided by Limine.\n");
        return -1;
    }

    for (uint64_t I = 0; I < LimineMod.response->module_count; I++)
    {
        struct limine_file* Mod = LimineMod.response->modules[I];
        if (!Mod || !Mod->path)
        {
            continue;
        }

        if (strcmp(Mod->path, "/BootImg.img") == 0)
        {
            PDebug("RamFS: Found BootImg.img at %p, size %llu bytes\n", Mod->address, Mod->size);

            /* Hand off to BootMountRamFs to wire into VFS */
            return BootMountRamFs(Mod->address, Mod->size);
        }
    }

    PError("RamFS: BootImg.img not found in Limine modules.\n");
    return -3;
}
