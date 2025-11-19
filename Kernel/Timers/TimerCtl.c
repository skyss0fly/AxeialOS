#include <APICTimer.h>  /* APIC timer constants and functions */
#include <AxeSchd.h>    /* Scheduler functions */
#include <AxeThreads.h> /* Thread management functions */
#include <HPETTimer.h>  /* HPET timer constants and functions */
#include <PerCPUData.h> /* Per-CPU data structures */
#include <SMP.h>        /* Symmetric multiprocessing functions */
#include <SymAP.h>      /* Symmetric Application Processor definitions */
#include <Timer.h>      /* Timer management structures and definitions */
#include <VMM.h>        /* Virtual memory management (for timer mapping) */

TimerManager Timer;

volatile uint32_t TimerInterruptCount = 0;

void
InitializeTimer(void)
{
    Timer.ActiveTimer      = TIMER_TYPE_NONE;
    Timer.SystemTicks      = 0;
    Timer.TimerInitialized = 0;

    if (DetectApicTimer() && InitializeApicTimer())
    {
        /* APIC timer successfully initialized */
    }

    else if (DetectHpetTimer() && InitializeHpetTimer())
    {
        /* HPET timer successfully initialized */
    }

    else if (InitializePitTimer())
    {
        /* PIT timer successfully initialized */
    }

    else
    {
        PError("No timer available!\n");
        return;
    }

    Timer.TimerInitialized = 1;

    PSuccess("Timer system initialized using %s\n",
             Timer.ActiveTimer == TIMER_TYPE_HPET   ? "HPET"
             : Timer.ActiveTimer == TIMER_TYPE_APIC ? "APIC"
                                                    : "PIT");

    __asm__ volatile("sti");
}

void
TimerHandler(InterruptFrame* __Frame__)
{
    uint32_t    CpuId   = GetCurrentCpuId();
    PerCpuData* CpuData = GetPerCpuData(CpuId);

    __atomic_fetch_add(&CpuData->LocalInterrupts, 1, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&CpuData->LocalTicks, 1, __ATOMIC_SEQ_CST);

    __atomic_fetch_add(&TimerInterruptCount, 1, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&Timer.SystemTicks, 1, __ATOMIC_SEQ_CST);

    WakeupSleepingThreads(CpuId);
    Schedule(CpuId, __Frame__);

    volatile uint32_t* EoiReg = (volatile uint32_t*)(CpuData->ApicBase + TimerApicRegEoi);
    *EoiReg                   = 0;
}

uint64_t
GetSystemTicks(void)
{
    return Timer.SystemTicks;
}

void
Sleep(uint32_t __Milliseconds__)
{
    if (!Timer.TimerInitialized)
    {
        return;
    }

    uint64_t StartTicks = Timer.SystemTicks;
    uint64_t EndTicks   = StartTicks + __Milliseconds__;

    while (Timer.SystemTicks < EndTicks)
    {
        __asm__ volatile("hlt"); /* Halt CPU to save power while waiting */
    }
}

uint32_t
GetTimerInterruptCount(void)
{
    return TimerInterruptCount;
}
