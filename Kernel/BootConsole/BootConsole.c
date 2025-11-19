
#include <BootConsole.h>
#include <Serial.h>

BootConsole Console = {0};

void
KickStartConsole(uint32_t* __FrameBuffer__, uint32_t __CW__, uint32_t __CH__)
{
    Console.FrameBuffer  = __FrameBuffer__;
    Console.FrameBufferW = __CW__;
    Console.FrameBufferH = __CH__;
    Console.ConsoleCol   = __CW__ / FontW;
    Console.ConsoleRow   = __CH__ / FontH;
    Console.CursorX      = 0;
    Console.CursorY      = 0;
    Console.TXColor      = 0xFFFFFF; /* White text */
    Console.BGColor      = 0x000000; /* Black background */
}

void
ClearConsole(void)
{
    /* Fill entire framebuffer with background color */
    for (uint32_t I = 0; I < Console.FrameBufferW * Console.FrameBufferH; I++)
    {
        Console.FrameBuffer[I] = Console.BGColor;
    }

    /* Reset cursor to top-left position */
    Console.CursorX = 0;
    Console.CursorY = 0;
}

void
ScrollConsole(void)
{
    /* Move all lines up by one character row */
    for (uint32_t PosY = 0; PosY < Console.ConsoleRow - 1; PosY++)
    {
        for (uint32_t PosX = 0; PosX < Console.ConsoleCol; PosX++)
        {
            /* Calculate source and destination Y positions in pixels */
            uint32_t srcY   = (PosY + 1) * FontH; /* Line below */
            uint32_t dstY   = PosY * FontH;       /* Current line */
            uint32_t pixelX = PosX * FontW;       /* Character column start */

            /* Copy each pixel of the character (FontW x FontH block) */
            for (uint32_t py = 0; py < FontH; py++)
            {
                for (uint32_t px = 0; px < FontW; px++)
                {
                    uint32_t SrcIdx = (srcY + py) * Console.FrameBufferW + (pixelX + px);
                    uint32_t DstIdx = (dstY + py) * Console.FrameBufferW + (pixelX + px);
                    Console.FrameBuffer[DstIdx] = Console.FrameBuffer[SrcIdx];
                }
            }
        }
    }

    /* Clear the newly exposed last line with background color */
    uint32_t lastLineY = (Console.ConsoleRow - 1) * FontH;
    for (uint32_t PosY = lastLineY; PosY < lastLineY + FontH; PosY++)
    {
        for (uint32_t PosX = 0; PosX < Console.FrameBufferW; PosX++)
        {
            Console.FrameBuffer[PosY * Console.FrameBufferW + PosX] = Console.BGColor;
        }
    }
}

void
PutChar(char __Char__)
{
    /* Always mirror output to serial port for debugging */
    SerialPutChar(__Char__);

    if (__Char__ == '\n') /* Newline: move to next line */
    {
        Console.CursorX = 0;
        Console.CursorY++;
    }
    else if (__Char__ == '\r') /* Carriage return: move to line start */
    {
        Console.CursorX = 0;
    }
    else /* Printable character: render and advance cursor */
    {
        uint32_t PixelX = Console.CursorX * FontW;
        uint32_t PixelY = Console.CursorY * FontH;

        DisplayChar(
            Console.FrameBuffer, Console.FrameBufferW, PixelX, PixelY, __Char__, Console.TXColor);
        Console.CursorX++;
    }

    /* Handle line wrapping at right edge */
    if (Console.CursorX >= Console.ConsoleCol)
    {
        Console.CursorX = 0;
        Console.CursorY++;
    }

    /* Handle scrolling at bottom of screen */
    if (Console.CursorY >= Console.ConsoleRow)
    {
        ScrollConsole();
        Console.CursorY = Console.ConsoleRow - 1;
    }
}

void
PutPrint(const char* __String__)
{
    /* Process each character until null terminator */
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
    /* Bounds checking to prevent cursor from going out of bounds */
    if (__CurX__ < Console.ConsoleCol)
    {
        Console.CursorX = __CurX__;
    }
    if (__CurY__ < Console.ConsoleRow)
    {
        Console.CursorY = __CurY__;
    }
}
