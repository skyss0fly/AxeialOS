#include <SymAP.h>          /* Symmetric Multiprocessing Application Processor definitions */
#include <Timer.h>          /* Timer management interfaces */
#include <VMM.h>            /* Virtual Memory Management functions */
#include <APICTimer.h>      /* APIC Timer specific constants and functions */
#include <AxeSchd.h>        /* Axe Scheduler definitions */
#include <AxeThreads.h>     /* Thread management interfaces */

/**
 * @brief Application Processor (AP) entry point.
 *
 * @details This function is executed by each Application Processor (AP) after startup.
 * 			It performs the following steps:
 *			Identifies the logical CPU number by matching LAPIC ID against the SMP manager.
 *			Marks the CPU as online and signals startup completion.
 *			Allocates and maps a dedicated stack for the AP.
 *			Initializes per-CPU interrupt handling and APIC timer configuration.
 *			Sets up the CPU scheduler for thread management.
 *			Enables interrupts and enters an idle loop (`hlt`) until scheduled work arrives.
 *
 * @param __CpuInfo__ Pointer to the Limine SMP information structure for this CPU.
 *
 * @return void
 *
 * @note This function never returns. Each AP remains in the idle loop
 *       until scheduled threads or interrupts are dispatched.
 *
 * @internal Called automatically by APs during SMP initialization.
 */
void
ApEntryPoint(struct limine_smp_info *__CpuInfo__)
{
    
    uint32_t CpuNumber = 0;
    for (uint32_t Index = 0; Index < Smp.CpuCount; Index++)
    {
        if (Smp.Cpus[Index].ApicId == __CpuInfo__->lapic_id)
        {
            CpuNumber = Index;
            break;
        }
    }

    
    Smp.Cpus[CpuNumber].Status = CPU_STATUS_ONLINE;
    Smp.Cpus[CpuNumber].Started = 1; /* Boolean flag indicating startup completion */

    
    __atomic_fetch_add(&CpuStartupCount, 1, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&Smp.OnlineCpus, 1, __ATOMIC_SEQ_CST);

    
    uint64_t CpuStackPhys = AllocPages(SMPCPUStackSize / 0x1000);
    if (!CpuStackPhys)
    {
        PError("AP: Failed to allocate stack for CPU %u\n", CpuNumber);
        while(1) __asm__("hlt"); /* Infinite loop of shame; CPU cannot proceed */
    }

    
    void* CpuStack = PhysToVirt(CpuStackPhys);

    
    uint64_t NewStackTop = (uint64_t)CpuStack + SMPCPUStackSize - 16;
    __asm__ volatile("movq %0, %%rsp" : : "r"(NewStackTop));

    
    PInfo("AP: CPU %u online with stack at 0x%016lx\n", CpuNumber, NewStackTop);

    
    PerCpuInterruptInit(CpuNumber, NewStackTop);

    
    SetupApicTimerForThisCpu();

    
    InitializeCpuScheduler(CpuNumber);

    
    __asm__ volatile("sti");

    
    for(;;)
    {
        __asm__ volatile("hlt");
    }
}
