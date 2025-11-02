#include <BootConsole.h>

BootConsole
Console = {0};

/**
 * Boot time Console
 */

void 
KickStartConsole(
uint32_t *__FrameBuffer__, 
uint32_t __CW__, 
uint32_t __CH__)
{
    Console.FrameBuffer = __FrameBuffer__;
    Console.FrameBufferW = __CW__;
    Console.FrameBufferH = __CH__;
    Console.ConsoleCol = __CW__ / FontW;
    Console.ConsoleRow = __CH__ / FontH;
    Console.CursorX = 0;
    Console.CursorY = 0;
    Console.TXColor = 0xFFFFFF;
    Console.BGColor = 0x000000;
}

void 
ClearConsole(void)
{
    for (uint32_t i = 0; i < Console.FrameBufferW * Console.FrameBufferH; i++)
    {
        Console.FrameBuffer[i] = Console.BGColor;
    }

    Console.CursorX = 0;
    Console.CursorY = 0;
}

void 
PutChar(char __Char__)
{
    if (__Char__ == '\n')/*Newline*/
    {
        Console.CursorX = 0;
        Console.CursorY++;
    }
    else if (__Char__ == '\r')/*Return*/
    {
        Console.CursorX = 0;
    }
    else	/*Printable character*/
    {
        uint32_t 
        PixelX = Console.CursorX * FontW;
        uint32_t 
        PixelY = Console.CursorY * FontH;
        
        DisplayChar(Console.FrameBuffer, Console.FrameBufferW, PixelX, PixelY, __Char__, Console.TXColor);
        Console.CursorX++;
    }
    
    if (Console.CursorX >= Console.ConsoleCol)
    {
        Console.CursorX = 0;
        Console.CursorY++;
    }
    
    if (Console.CursorY >= Console.ConsoleRow)
    {
        Console.CursorY = Console.ConsoleRow - 1;
    }
}

void 
PutPrint(const char *__String__)
{
    while (*__String__)
    {
        PutChar(*__String__);
        __String__++;
    }
}

void 
SetBGColor(uint32_t __FG__, uint32_t __BG__)
{
    Console.TXColor = __FG__;
    Console.BGColor = __BG__;
}

void 
SetCursor(uint32_t __CurX__, uint32_t __CurY__)
{
    if (__CurX__ < Console.ConsoleCol) Console.CursorX = __CurX__;
    if (__CurY__ < Console.ConsoleRow) Console.CursorY = __CurY__;
}
