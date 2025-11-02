#include <KrnPrintf.h>

/**
 * Main printf
 */

void 
KrnPrintf(const char *__Format__, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, __Format__);
    
    while (*__Format__)
    {
        if (*__Format__ == '%')
        {
            __Format__++;
            ProcessFormatSpecifier(&__Format__, &args);
        }
        else
        {
            PutChar(*__Format__);
            __Format__++;
        }
    }
    
    __builtin_va_end(args);
}

void 
KrnPrintfColor(uint32_t __FG__, uint32_t __BG__, const char *__Format__, ...)
{
    uint32_t
	OldFG = Console.TXColor;
    uint32_t
	OldBG = Console.BGColor;
    
    SetBGColor(__FG__, __BG__);
    
    __builtin_va_list args;
    __builtin_va_start(args, __Format__);
    
    while (*__Format__)
    {
        if (*__Format__ == '%')
        {
            __Format__++;
            ProcessFormatSpecifier(&__Format__, &args);
        }
        else
        {
            PutChar(*__Format__);
            __Format__++;
        }
    }
    
    __builtin_va_end(args);
    SetBGColor(OldFG, OldBG);
}

void 
ProcessFormatSpecifier(const char **__Format__, __builtin_va_list *__Args__)
{
    switch (**__Format__)
    {
        case 'd':
        case 'i':
            PrintInteger(__builtin_va_arg(*__Args__, int), 10, 0);
            break;

        case 'u':
            PrintUnsigned(__builtin_va_arg(*__Args__, unsigned int), 10, 0);
            break;

        case 'x':
            PrintUnsigned(__builtin_va_arg(*__Args__, unsigned int), 16, 0);
            break;

        case 'X':
            PrintUnsigned(__builtin_va_arg(*__Args__, unsigned int), 16, 1);
            break;

        case 'o':
            PrintUnsigned(__builtin_va_arg(*__Args__, unsigned int), 8, 0);
            break;

        case 'b':
            PrintUnsigned(__builtin_va_arg(*__Args__, unsigned int), 2, 0);
            break;
			
        case 's':
            PrintString(__builtin_va_arg(*__Args__, const char*));
            break;

        case 'c':
            PrintChar((char)__builtin_va_arg(*__Args__, int));
            break;

        case 'p':
            PrintPointer(__builtin_va_arg(*__Args__, void*));
            break;

        case '%':
            PutChar('%');
            break;
			
        default:
            PutChar('%');
            PutChar(**__Format__);
            break;
    }
    (*__Format__)++;
}

/**
 * Helpers
 */

void 
PrintInteger(int __Value__, int __Base__, int __Uppercase__)
{
    char
	Buffer[32];
    IntegerToString(__Value__, Buffer, __Base__);
    
    if (__Uppercase__)
    {
        for (int Iteration = 0; Buffer[Iteration]; Iteration++)
        {
            if (Buffer[Iteration] >= 'a' && Buffer[Iteration] <= 'f')
            Buffer[Iteration] = Buffer[Iteration] - 'a' + 'A';
        }
    }
    
    PrintString(Buffer);
}

void 
PrintUnsigned(uint32_t __Value__, int __Base__, int __Uppercase__)
{
    char
	Buffer[32];
    UnsignedToString(__Value__, Buffer, __Base__);
    
    if (__Uppercase__)
    {
        for (int Iteration = 0; Buffer[Iteration]; Iteration++)
        {
            if (Buffer[Iteration] >= 'a' && Buffer[Iteration] <= 'f')
            Buffer[Iteration] = Buffer[Iteration] - 'a' + 'A';
        }
    }
    
    PrintString(Buffer);
}

void 
PrintString(const char *__String__)
{
    if (__String__ == 0)
    {
        PutPrint("(null)");
        return;
    }

    PutPrint(__String__);
}

void 
PrintChar(char __Char__)
{
    PutChar(__Char__);/*simple asf*/
}

void 
PrintPointer(void *__Pointer__)
{
    PutPrint("0x");
    PrintUnsigned((uint32_t)(uintptr_t)__Pointer__, 16, 0);
}

int 
StringLength(const char *__String__)
{
    int
	Length = 0;
    while (__String__[Length]) Length++;
	/*strlen?*/
    return Length;
}

void 
ReverseString(char *__String__, int __Length__)
{
    int
	Start = 0;
    int
	End = __Length__ - 1;
    
    while (Start < End)
    {
        char Temp = __String__[Start];
        __String__[Start] = __String__[End];
        __String__[End] = Temp;
        Start++;
        End--;
    }
}

void 
IntegerToString(int __Value__, char *__Buffer__, int __Base__)
{
    int
	Iteration = 0;
    int
	IsNegative = 0;
    
    if (__Value__ == 0)
    {
        __Buffer__[Iteration++] = '0';
        __Buffer__[Iteration] = '\0';	/*NullTerminate*/
        return;
    }
    
    if (__Value__ < 0 && __Base__ == 10)
    {
        IsNegative = 1;
        __Value__ = -__Value__;
    }
    
    while (__Value__ != 0)
    {
        int Remainder = __Value__ % __Base__;
        __Buffer__[Iteration++] = (Remainder > 9) ? (Remainder - 10) + 'a' : Remainder + '0';
        __Value__ = __Value__ / __Base__;
    }
    
    if (IsNegative) __Buffer__[Iteration++] = '-';
    
    __Buffer__[Iteration] = '\0';
    ReverseString(__Buffer__, Iteration);
}

void 
UnsignedToString(uint32_t __Value__, char *__Buffer__, int __Base__)
{
    int Iteration = 0;
    
    if (__Value__ == 0)
    {
        __Buffer__[Iteration++] = '0';
        __Buffer__[Iteration] = '\0';

        return;
    }
    
    while (__Value__ != 0)
    {
        int Remainder = __Value__ % __Base__;
        __Buffer__[Iteration++] = (Remainder > 9) ? 
		(Remainder - 10) + 'a' : Remainder + '0';
        __Value__ = __Value__ / __Base__;
    }
    
    __Buffer__[Iteration] = '\0';
    ReverseString(__Buffer__, Iteration);
}
