#pragma once

#include <EveryType.h>

/*KHeap*/

#define MaxSlabSizes    8
#define SlabMagic       0xDEADBEEF
#define FreeObjectMagic 0xFEEDFACE

void* KMalloc(size_t __Size__);
void  KFree(void* __Ptr__);

/*Module*/

#define ModTextBase 0xffffffff90000000ULL
#define ModTextSize 0x08000000ULL /* 128 MB */
#define ModDataBase 0xffffffff98000000ULL
#define ModDataSize 0x08000000ULL /* 128 MB */

void  ModMemInit(void);
void* ModMalloc(size_t __Size__, int __IsText__);
void  ModFree(void* __Addr__, size_t __Size__);
