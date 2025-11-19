#include <AllTypes.h>

uint32_t
CpioAlignUp(uint32_t __Value__, uint32_t __Align__)
{
    return (__Value__ + (__Align__ - 1)) & ~(__Align__ - 1);
}

uint32_t
CpioParseHex(const char* __Hex__)
{
    uint32_t Value = 0;
    for (uint32_t Index = 0; Index < 8; Index++)
    {
        char     C = __Hex__[Index];
        uint32_t D = 0;

        if (C >= '0' && C <= '9')
        {
            D = (uint32_t)(C - '0');
        }
        else if (C >= 'a' && C <= 'f')
        {
            D = (uint32_t)(C - 'a' + 10);
        }
        else if (C >= 'A' && C <= 'F')
        {
            D = (uint32_t)(C - 'A' + 10);
        }

        Value = (Value << 4) | D;
    }
    return Value;
}
