#include <AllTypes.h>
#include <LimineServices.h>
#include <EarlyBootFB.h>
#include <BootConsole.h>
#include <KrnPrintf.h>

/**
 * Main Kernel Entry Point
 */

void
_start(void)
{
    if (EarlyLimineFrambuffer.response && EarlyLimineFrambuffer.response->framebuffer_count > 0)
	{
        struct limine_framebuffer 
		*FrameBuffer = EarlyLimineFrambuffer.response->framebuffers[0];
        
        if (FrameBuffer->address)
		{
            KickStartConsole((uint32_t*)FrameBuffer->address,
			FrameBuffer->width,
			FrameBuffer->height);

            ClearConsole();
            
			/*Small test*/
            PInfo("Information\n");
            PSuccess("Perfection\n");
            PWarn("warning message: %d\n", 42);
            PError("error: 0x%X\n", 0xDEAD);
            PDebug("Debug information because yeah: %s\n", "get rickrolled");
        }
    }
    
    for (;;) __asm__("hlt");
}
