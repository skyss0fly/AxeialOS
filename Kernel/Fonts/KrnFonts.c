#include <KrnFont.h>

/**
 * PutChar
 */

void 
DisplayChar(
uint32_t *__FrameBuffer__, 
uint32_t __FrameBufferW__, 
uint32_t __PosX__,
uint32_t __PosY__,
char __Char__,
uint32_t __32bitColor__)
{
    const uint8_t 
	*Character = KrnlFontMap[(uint8_t)__Char__];
    
	/*Search in the map*/
    for (int MapRow = 0; MapRow < FontH; MapRow++)	/*Row*/
	{
        uint8_t 
		Line = Character[MapRow];

        for (int Column = 0; Column < FontW; Column++)	/*Column*/
		{
            if (Line & (0x80 >> Column))
			{
                __FrameBuffer__[(__PosY__ + MapRow) * __FrameBufferW__ + (__PosX__ + Column)] = __32bitColor__;
            }
        }
    }
}

/**
 * PutString
 */

void DisplayString(
uint32_t *__FrameBuffer__,
uint32_t __FrameBufferW__,
uint32_t __PosX__,
uint32_t __PosY__,
const char *__String__,
uint32_t __32bitColor__)
{
    uint32_t 
	PosX = __PosX__;

	/*Print Indivisually*/
    while (*__String__)
	{
        DisplayChar(__FrameBuffer__, __FrameBufferW__, PosX, __PosY__, *__String__, __32bitColor__);
        PosX += FontW;
        __String__++;
    }
}
