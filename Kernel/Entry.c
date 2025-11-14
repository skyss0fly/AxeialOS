/**
 * @file Entry point of the kernel and
 * 		 the early boot init system as
 * 		 well as the main init system.
 * 
 * @brief Used for external and internal testing
 * 		  for the kernel.
 * 
 * @see read and visit All headers below for traces and
 * 		the main interfaces of the kernel.
 */
#include <AllTypes.h>
#include <LimineServices.h>
#include <EarlyBootFB.h>
#include <BootConsole.h>
#include <KrnPrintf.h>
#include <GDT.h>
#include <IDT.h>
#include <PMM.h>
#include <VMM.h>
#include <KHeap.h>
#include <Timer.h>
#include <Serial.h>
#include <SMP.h>
#include <SymAP.h>
#include <Sync.h>
#include <APICTimer.h>
#include <AxeThreads.h>
#include <AxeSchd.h>
#include <BootImg.h>
#include <KExports.h>
#include <VFS.h>
#include <ModELF.h>
#include <ModMemMgr.h>
#include <DevFS.h>

/** @test Sensitive Testing Purposes */
static SpinLock TestLock;

/**
 * @brief kernel Worker, Handles Post init.
 * 
 * @details Handles Post init after the AxeThread manager,
 * 			As the kernel now handles threads and only
 * 			accessible through interrupts.
 * 
 * @param __Argument__ handles the argument passed to the
 * 					   thread. 
 * 
 * @deprecated __Argument__ NULL
 * 			   as it is not used
 * 
 * @return Doesn't return anything, runs forever.
 * 
 * @see Also consider checking the corresponding header
 * 		files for tracement and to understand the post
 * 		Init. @section Headers
 * 
 * @internal Internal Function for kernel Work.
 */
void
KernelWorkerThread(void* __Argument__)
{
    
    PInfo("Kernel Worker: Started on CPU %u\n", GetCurrentCpuId());

	/** @subsection Kernel Modules */
	/** @see Kmods */
	ModMemInit();
	InitializeBootImage();

	/** @subsection DevFS */
	VfsPerm Perm;
	Perm.Mode = VModeRUSR | VModeWUSR | VModeXUSR |
	            VModeRGRP | VModeXGRP |
	            VModeROTH | VModeXOTH;
	Perm.Uid  = 0;
	Perm.Gid  = 0;
	
	if (VfsMkdir("/dev", Perm) != 0) {
	    PError("Failed to create /dev\n");
	}

	DevFsInit();
	//DevFsRegister();
	Superblock *SuperBlk = DevFsMountImpl(0, 0);
	if (!SuperBlk) {
	    PError("Boot: DevFsMountImpl failed\n");
	}

	if (VfsRegisterPseudoFs("/dev", SuperBlk) != 0) {
	    PError("Boot: mount devfs failed\n");
	}
	DevFsRegisterSeedDevices();
	
	/** @test Kmods Testing */
	InstallModule("/Modtest.ko");

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

/**
 * @brief Enrty point of the kernel
 * 
 * @details Handles the early boot init
 * 			system and neccessary post
 * 			init system.
 * 
 * @param void Doesn't take any.
 * 
 * @return Doesn't return anything,
 * 		   runs forever.
 * 
 * @see Consider checking the above Header(#include) files
 * 		to trace the init system.
 * 
 * @internal Internal Function for kernel Work.
 * 
 */
void
_start(void)
{
	/** @subsection Frambuffer / Early Framebuffer */
    if (EarlyLimineFrambuffer.response && EarlyLimineFrambuffer.response->framebuffer_count > 0)
    {
        struct limine_framebuffer *FrameBuffer = EarlyLimineFrambuffer.response->framebuffers[0];

		/** @subsection Testing Utils */
		InitializeSpinLock(&TestLock, "TestLock");
        
		/** @subsection UART / Debugging Utils */
        InitializeSerial();

        if (FrameBuffer->address)
        {
			/** @subsection Early Boot Console */
            KickStartConsole((uint32_t*)FrameBuffer->address,
            FrameBuffer->width,
            FrameBuffer->height);
            InitializeSpinLock(&ConsoleLock, "Console");
            ClearConsole();
            
            PInfo("AxeialOS Kernel Booting...\n");
        }

		/** @subsection Interrupts */
        InitializeGdt();        
        InitializeIdt();

        /** @subsection Memory */
        InitializePmm();        
        InitializeVmm();
		InitializeKHeap();

        /** @subsection Threading/Scheduler */
        InitializeTimer();
        InitializeThreadManager();
        InitializeSpinLock(&SMPLock, "SMP");
        InitializeSmp();
        InitializeScheduler();

		/** @subsection Kernel Worker / Kernel Post Init */
        Thread* KernelWorker = CreateThread(ThreadTypeKernel, KernelWorkerThread, NULL, ThreadPrioritykernel);
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
