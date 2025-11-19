#include <APICTimer.h>
#include <AllTypes.h>
#include <AxeSchd.h>
#include <AxeThreads.h>
#include <BootConsole.h>
#include <BootImg.h>
#include <CharBus.h>
#include <DevFS.h>
#include <EarlyBootFB.h>
#include <GDT.h>
#include <IDT.h>
#include <KExports.h>
#include <KHeap.h>
#include <KrnPrintf.h>
#include <LimineServices.h>
#include <ModELF.h>
#include <ModMemMgr.h>
#include <PMM.h>
#include <ProcFS.h>
#include <PubELF.h>
#include <SMP.h>
#include <Serial.h>
#include <SymAP.h>
#include <Sync.h>
#include <Timer.h>
#include <VFS.h>
#include <VMM.h>
/** Devs */
#define __Kernel__

static SpinLock TestLock;

void
KernelWorkerThread(void* __Argument__)
{
    PInfo("Kernel Worker: Started on CPU %u\n", GetCurrentCpuId());

    ModMemInit();
    InitializeBootImage();

    VfsPerm Perm;
    Perm.Mode = VModeRUSR | VModeWUSR | VModeXUSR | VModeRGRP | VModeXGRP | VModeROTH | VModeXOTH;
    Perm.Uid  = 0;
    Perm.Gid  = 0;

    if (VfsMkdir("/dev", Perm) != 0)
    {
        PError("Failed to create /dev\n");
    }

    DevFsInit();
    Superblock* SuperBlk = DevFsMountImpl(0, 0);
    if (!SuperBlk)
    {
        PError("Boot: DevFsMountImpl failed\n");
    }

    if (VfsRegisterPseudoFs("/dev", SuperBlk) != 0)
    {
        PError("Boot: mount devfs failed\n");
    }
    DevFsRegisterSeedDevices();

    if (ProcInit() != 0)
    {
        PError("Init: ProcInit failed\n");
        return;
    }

    if (ProcFsInit() != 0)
    {
        PError("Init: ProcFsInit failed\n");
        return;
    }

    Process* InitProc = ProcFind(1);
    if (InitProc)
    {
        ProcFsExposeProcess(InitProc);
    }

    InitRamDiskDevDrvs();

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

void
_start(void)
{
    if (EarlyLimineFrambuffer.response && EarlyLimineFrambuffer.response->framebuffer_count > 0)
    {
        struct limine_framebuffer* FrameBuffer = EarlyLimineFrambuffer.response->framebuffers[0];

        InitializeSpinLock(&TestLock, "TestLock");

        InitializeSerial();

        if (FrameBuffer->address)
        {
            KickStartConsole(
                (uint32_t*)FrameBuffer->address, FrameBuffer->width, FrameBuffer->height);
            InitializeSpinLock(&ConsoleLock, "Console");
            ClearConsole();

            PInfo("AxeialOS Kernel Booting...\n");
        }

        InitializeGdt();
        InitializeIdt();

        unsigned long Cr0, Cr4;

        /* Read CR0 and CR4 */
        __asm__ volatile("mov %%cr0, %0" : "=r"(Cr0));
        __asm__ volatile("mov %%cr4, %0" : "=r"(Cr4));

        /* CR0: clear EM (bit 2), set MP (bit 1), clear TS (bit 3) */
        Cr0 &= ~(1UL << 2); /* EM = 0 */
        Cr0 |= (1UL << 1);  /* MP = 1 */
        Cr0 &= ~(1UL << 3); /* TS = 0 */
        __asm__ volatile("mov %0, %%cr0" ::"r"(Cr0) : "memory");

        /* CR4: set OSFXSR (bit 9) and OSXMMEXCPT (bit 10) for SSE */
        Cr4 |= (1UL << 9) | (1UL << 10);
        __asm__ volatile("mov %0, %%cr4" ::"r"(Cr4) : "memory");

        /* Initialize x87/SSE state */
        __asm__ volatile("fninit");

        InitializePmm();
        InitializeVmm();
        InitializeKHeap();

        InitializeTimer();
        InitializeThreadManager();
        InitializeSpinLock(&SMPLock, "SMP");
        InitializeSmp();
        InitializeScheduler();

        Thread* KernelWorker =
            CreateThread(ThreadTypeKernel, KernelWorkerThread, NULL, ThreadPrioritykernel);
        if (KernelWorker)
        {
            ThreadExecute(KernelWorker);
            PSuccess("Ctl Transfer to Worker\n");
        }
    }

    for (;;)
    {
        __asm__("hlt");
    }
}
