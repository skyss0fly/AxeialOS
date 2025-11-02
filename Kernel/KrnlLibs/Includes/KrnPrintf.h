#pragma once

#include <AllTypes.h>
#include <BootConsole.h>

/*Uncomment for Debug Output*/
#define DEBUG

/**
 * PrintfState (Pretty much optional)
 */

typedef 
struct 
{

    const char 	*Format;
    void 		**Args;
    int 		ArgIndex;

}PrintfState;

/**
 * Logging colors
 */
#define ClrNormal     0xFFFFFF
#define ClrInvisible  0x000000
#define ClrError      0xFF0000
#define ClrSuccess    0x00FF00
#define ClrBlue       0x0000FF
#define ClrWarn       0xFFFF00
#define ClrInfo       0x00FFFF
#define ClrMagnet     0xFF00FF
#define ClrTang       0xFF8000
#define ClrDebug      0x808080

/**
 * Debug
 */
#ifdef DEBUG
    #define PDebug(fmt, ...) _PDebug(fmt, ##__VA_ARGS__)
#else
    #define PDebug(fmt, ...) do {} while(0)
#endif

/**
 * %d, %x, %X, %s, %c, %p, %u, %o, %b
 * Colorformat: \033[fg;bg]text\033[0] 
 */

/*Main*/
void KrnPrintf(const char *__Format__, ...);
void KrnPrintfColor(uint32_t __FG__, uint32_t __BG__, const char *__Format__, ...);
void ProcessFormatSpecifier(const char **__Format__, __builtin_va_list *__Args__);

/*Helpers*/
void PrintInteger(int __Value__, int __Base__, int __Uppercase__);
void PrintUnsigned(uint32_t __Value__, int __Base__, int __Uppercase__);
void PrintString(const char *__String__);
void PrintChar(char __Char__);
void PrintPointer(void *__Pointer__);
int StringLength(const char *__String__);
void ReverseString(char *__String__, int __Length__);
void IntegerToString(int __Value__, char *__Buffer__, int __Base__);
void UnsignedToString(uint32_t __Value__, char *__Buffer__, int __Base__);

/*Logging*/
void PError(const char *__Format__, ...);
void PWarn(const char *__Format__, ...);
void PInfo(const char *__Format__, ...);
void _PDebug(const char *__Format__, ...);
void PSuccess(const char *__Format__, ...);