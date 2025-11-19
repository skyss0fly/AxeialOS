#pragma once

#include <EveryType.h>

#define IdtTypeInterruptGate 0x8E
#define IdtTypeTrapGate      0x8F

void SetIdtEntry(int __Index__, uint64_t __Handler__, uint16_t __Selector__, uint8_t __Flags__);