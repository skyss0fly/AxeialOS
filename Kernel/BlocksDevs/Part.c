
#include <BlockDev.h>

int
BlockRegisterGPTPartitions(BlockDisk*  __Disk__,
                           const void* __GptHeader__,
                           const void* __GptEntries__,
                           long        __EntryCount__)
{
    (void)__GptHeader__;
    (void)__GptEntries__;
    (void)__EntryCount__;

    /*TODO*/

    return 0;
}

int
BlockRegisterMBRPartitions(BlockDisk* __Disk__, const void* __MbrSector__)
{
    (void)__MbrSector__;

    /*TODO*/

    return 0;
}