#include <HPETTimer.h> /* HPET-specific constants and definitions */
#include <Timer.h>     /* Timer management structures */

int
DetectHpetTimer(void)
{
    return 0;
}

int
InitializeHpetTimer(void)
{
    PInfo("Initializing HPET Timer...\n");
    return 0;
}
