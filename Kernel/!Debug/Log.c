#include <KrnPrintf.h>

/**
 * ERROR
 */
void 
PError(const char *__Format__, ...)
{
    uint32_t 
	OldFG = Console.TXColor;
    uint32_t 
	OldBG = Console.BGColor;
    
    SetBGColor(ClrError, ClrInvisible);
    PutPrint("[ERROR]:");
    
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

/**
 * WARN
 */
void 
PWarn(const char *__Format__, ...)
{
    uint32_t 
	OldFG = Console.TXColor;
    uint32_t 
	OldBG = Console.BGColor;
    
    SetBGColor(ClrWarn, ClrInvisible);
    PutPrint("[WARN]:");
    
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

/**
 * INFO
 */
void 
PInfo(const char *__Format__, ...)
{
    uint32_t 
	OldFG = Console.TXColor;
    uint32_t 
	OldBG = Console.BGColor;
    
    SetBGColor(ClrInfo, ClrInvisible);
    PutPrint("[INFO]:");
    
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

/**
 * DEBUG
 */
void 
_PDebug(const char *__Format__, ...)
{
    uint32_t
	OldFG = Console.TXColor;
    uint32_t
	OldBG = Console.BGColor;
    
    SetBGColor(ClrDebug, ClrInvisible);
    PutPrint("[DEBUG]:");
    
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

/**
 * SUCCESS
 */
void 
PSuccess(const char *__Format__, ...)
{
    uint32_t
	OldFG = Console.TXColor;
    uint32_t
	OldBG = Console.BGColor;
    
    SetBGColor(ClrSuccess, ClrInvisible);
    PutPrint("[OK]:");
    
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
