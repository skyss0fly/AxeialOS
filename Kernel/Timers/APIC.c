#include <APICTimer.h>  /* APIC timer constants and register definitions */
#include <LimineSMP.h>  /* Limine SMP request structures */
#include <PerCPUData.h> /* Per-CPU data structures */
#include <SymAP.h>      /* Symmetric Application Processor definitions */
#include <Timer.h>      /* Global timer management structures */
#include <VMM.h>        /* Virtual memory mapping functions */

static int
CheckApicSupport(void)
{
    uint32_t Eax, Ebx, Ecx, Edx;

    __asm__ volatile("cpuid" : "=a"(Eax), "=b"(Ebx), "=c"(Ecx), "=d"(Edx) : "a"(1));

    if (!(Edx & (1 << 9)))
    {
        PError("APIC: CPU does not support APIC!\n");
        return 0;
    }

    PDebug("APIC: CPU supports APIC (CPUID.1:EDX.APIC = 1)\n");
    return 1;
}

int
DetectApicTimer(void)
{
    PDebug("APIC: detecting...\n");

    if (!CheckApicSupport())
    {
        return 0;
    }

    uint64_t ApicBaseMsrValue = ReadMsr(TimerApicBaseMsr);
    PDebug("APIC: Base MSR = 0x%016llX\n", ApicBaseMsrValue);

    if (!(ApicBaseMsrValue & TimerApicBaseEnable))
    {
        PWarn("APIC: Not enabled in MSR, attempting to enable...\n");
        ApicBaseMsrValue |= TimerApicBaseEnable;
        WriteMsr(TimerApicBaseMsr, ApicBaseMsrValue);

        ApicBaseMsrValue = ReadMsr(TimerApicBaseMsr);
        if (!(ApicBaseMsrValue & TimerApicBaseEnable))
        {
            PError("APIC: Failed to enable APIC!\n");
            return 0;
        }
        PDebug("APIC: Successfully enabled\n");
    }

    uint64_t ApicPhysBase = ApicBaseMsrValue & 0xFFFFF000;
    Timer.ApicBase        = (uint64_t)PhysToVirt(ApicPhysBase);
    PDebug("APIC: Physical base = 0x%016llX, Virtual base = 0x%016llX\n",
           ApicPhysBase,
           Timer.ApicBase);

    volatile uint32_t* ApicVersionReg = (volatile uint32_t*)(Timer.ApicBase + TimerApicRegVersion);
    uint32_t           VersionValue   = *ApicVersionReg;

    if (VersionValue == 0xFFFFFFFF || VersionValue == 0x00000000)
    {
        PError("APIC: Invalid version register (0x%08X)\n", VersionValue);
        return 0;
    }

    uint32_t ApicVersion = VersionValue & 0xFF;
    uint32_t MaxLvtEntry = (VersionValue >> 16) & 0xFF;

    PDebug("APIC: Version = 0x%02X, Max LVT = %u\n", ApicVersion, MaxLvtEntry);

    if (MaxLvtEntry < 3)
    {
        PError("APIC: Timer LVT entry not available (Max LVT = %u)\n", MaxLvtEntry);
        return 0;
    }

    PSuccess("APIC Timer detected successfully\n");
    return 1;
}

int
InitializeApicTimer(void)
{
    PInfo("APIC: Starting initialization...\n");

    __asm__ volatile("cli");

    volatile uint32_t* SpuriousReg = (volatile uint32_t*)(Timer.ApicBase + TimerApicRegSpuriousInt);
    volatile uint32_t* LvtTimer    = (volatile uint32_t*)(Timer.ApicBase + TimerApicRegLvtTimer);
    volatile uint32_t* TimerDivide = (volatile uint32_t*)(Timer.ApicBase + TimerApicRegTimerDivide);
    volatile uint32_t* TimerInitCount =
        (volatile uint32_t*)(Timer.ApicBase + TimerApicRegTimerInitCount);
    volatile uint32_t* TimerCurrCount =
        (volatile uint32_t*)(Timer.ApicBase + TimerApicRegTimerCurrCount);
    volatile uint32_t* EoiReg = (volatile uint32_t*)(Timer.ApicBase + TimerApicRegEoi);
    volatile uint32_t* TprReg =
        (volatile uint32_t*)(Timer.ApicBase + 0x080); /* Task Priority Register */

    *TimerInitCount = 0;
    *LvtTimer       = TimerApicTimerMasked;

    *TprReg = 0;
    *EoiReg = 0;

    *SpuriousReg = 0x100 | 0xFF;

    *TimerDivide = TimerApicTimerDivideBy16;

    *TimerInitCount     = 0xFFFFFFFF;
    uint32_t StartCount = *TimerCurrCount;

    for (uint32_t I = 0; I < 10000; I++)
    {
        __asm__ volatile("outb %%al, $0x80" : : "a"((uint8_t)0)); /* Short delay */
    }

    uint32_t EndCount    = *TimerCurrCount;
    uint32_t TicksIn10ms = StartCount - EndCount;
    Timer.TimerFrequency = TicksIn10ms * 100;

    if (Timer.TimerFrequency < 1000000)
    {
        Timer.TimerFrequency = 100000000; /* Default APIC frequency */
    }

    uint32_t InitialCount = Timer.TimerFrequency / TimerTargetFrequency;
    if (InitialCount == 0)
    {
        InitialCount = 1;
    }

    *TimerInitCount = 0;
    while (*TimerCurrCount != 0)
    {
        __asm__ volatile("nop");
    }

    *LvtTimer = TimerVector | TimerApicTimerPeriodic | TimerApicTimerMasked;

    *TimerInitCount = InitialCount;

    Timer.ActiveTimer = TIMER_TYPE_APIC;

    struct limine_smp_response* SmpResponse = EarlyLimineSmp.response;
    for (uint32_t CpuIndex = 0; CpuIndex < SmpResponse->cpu_count; CpuIndex++)
    {
        PerCpuData* CpuData = GetPerCpuData(CpuIndex);
        CpuData->ApicBase   = Timer.ApicBase;

        PDebug("APIC: Set CPU %u APIC base to 0x%llx\n", CpuIndex, CpuData->ApicBase);
    }

    PSuccess("APIC Timer initialized at %u Hz\n", Timer.TimerFrequency);

    *LvtTimer = TimerVector | TimerApicTimerPeriodic;
    return 1;
}
