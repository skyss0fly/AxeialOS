#include <AllTypes.h>
#include <DevFS.h>
#include <KHeap.h>
#include <KrnPrintf.h>
#include <SMP.h>
#include <String.h>
#include <Sync.h>
#include <VFS.h>

#include <AxeSchd.h>
#include <AxeThreads.h>
#include <ProcFS.h>
#include <Process.h>

static ProcTable __ProcTable__     = {0};
static SpinLock  __ProcTableLock__ = {0};
static long      __NextPid__       = 1;
static Process*  __InitProc__      = 0;

static long
AllocPidLocked(void)
{
    return __NextPid__++;
}

static int
EnsureProcTableCapacity(long __Need__)
{
    if (__ProcTable__.Cap >= __Need__)
    {
        return 0;
    }
    long NewCap = (__ProcTable__.Cap == 0) ? 32 : (__ProcTable__.Cap * 2);
    while (NewCap < __Need__)
    {
        NewCap *= 2;
    }

    Process** NewItems = (Process**)KMalloc(sizeof(Process*) * (size_t)NewCap);
    if (!NewItems)
    {
        return -1;
    }

    for (long I = 0; I < __ProcTable__.Count; I++)
    {
        NewItems[I] = __ProcTable__.Items[I];
    }
    for (long I = __ProcTable__.Count; I < NewCap; I++)
    {
        NewItems[I] = 0;
    }

    if (__ProcTable__.Items)
    {
        KFree(__ProcTable__.Items);
    }
    __ProcTable__.Items = NewItems;
    __ProcTable__.Cap   = NewCap;
    return 0;
}

static int
InsertProcLocked(Process* __Proc__)
{
    if (EnsureProcTableCapacity(__ProcTable__.Count + 1) != 0)
    {
        return -1;
    }
    __ProcTable__.Items[__ProcTable__.Count++] = __Proc__;
    return 0;
}

static void
RemoveProcLocked(long __Pid__)
{
    for (long I = 0; I < __ProcTable__.Count; I++)
    {
        Process* P = __ProcTable__.Items[I];
        if (P && P->PID == __Pid__)
        {
            for (long J = I; J < __ProcTable__.Count - 1; J++)
            {
                __ProcTable__.Items[J] = __ProcTable__.Items[J + 1];
            }
            __ProcTable__.Items[--__ProcTable__.Count] = 0;
            return;
        }
    }
}

static Process*
FindProcLocked(long __Pid__)
{
    for (long I = 0; I < __ProcTable__.Count; I++)
    {
        Process* P = __ProcTable__.Items[I];
        if (P && P->PID == __Pid__)
        {
            return P;
        }
    }
    return 0;
}

static void
InitFdTable(Process* __Proc__, long __Cap__)
{
    if (__Cap__ <= 0)
    {
        __Cap__ = 16;
    }
    __Proc__->FdTable = (ProcFd*)KMalloc(sizeof(ProcFd) * (size_t)__Cap__);
    if (!__Proc__->FdTable)
    {
        __Proc__->FdCap   = 0;
        __Proc__->FdCount = 0;
        return;
    }
    __Proc__->FdCap   = __Cap__;
    __Proc__->FdCount = 0;
    for (long I = 0; I < __Cap__; I++)
    {
        __Proc__->FdTable[I].Fd     = I;
        __Proc__->FdTable[I].Kind   = PFdNone;
        __Proc__->FdTable[I].Obj    = 0;
        __Proc__->FdTable[I].Flags  = 0;
        __Proc__->FdTable[I].Refcnt = 0;
    }
}

static int
EnsureFdTableCapacity(Process* __Proc__, long __Need__)
{
    if (!__Proc__)
    {
        return -1;
    }
    if (__Proc__->FdCap >= __Need__)
    {
        return 0;
    }
    long NewCap = (__Proc__->FdCap == 0) ? 16 : __Proc__->FdCap * 2;
    while (NewCap < __Need__)
    {
        NewCap *= 2;
    }

    ProcFd* NewTab = (ProcFd*)KMalloc(sizeof(ProcFd) * (size_t)NewCap);
    if (!NewTab)
    {
        return -1;
    }

    for (long I = 0; I < __Proc__->FdCap; I++)
    {
        NewTab[I] = __Proc__->FdTable[I];
    }
    for (long I = __Proc__->FdCap; I < NewCap; I++)
    {
        NewTab[I].Fd     = I;
        NewTab[I].Kind   = PFdNone;
        NewTab[I].Obj    = 0;
        NewTab[I].Flags  = 0;
        NewTab[I].Refcnt = 0;
    }

    if (__Proc__->FdTable)
    {
        KFree(__Proc__->FdTable);
    }
    __Proc__->FdTable = NewTab;
    __Proc__->FdCap   = NewCap;
    return 0;
}

int
ProcInit(void)
{
    AcquireSpinLock(&__ProcTableLock__);
    __ProcTable__.Items = 0;
    __ProcTable__.Count = 0;
    __ProcTable__.Cap   = 0;

    if (EnsureProcTableCapacity(32) != 0)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        PError("Proc: table alloc failed\n");
        return -1;
    }

    Process* InitProc = (Process*)KMalloc(sizeof(Process));
    if (!InitProc)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        return -1;
    }
    __builtin_memset(InitProc, 0, sizeof(Process));

    InitProc->PID  = AllocPidLocked(); /* becomes PID 1 */
    InitProc->PPID = 0;
    InitProc->PGID = InitProc->PID;
    InitProc->SID  = InitProc->PID;

    InitProc->MainThread = GetCurrentThread(GetCurrentCpuId());
    if (InitProc->MainThread)
    {
        ((Thread*)InitProc->MainThread)->ProcessId = (uint32_t)InitProc->PID;
    }

    InitFdTable(InitProc, 16);
    InitProc->FdStdin  = 0;
    InitProc->FdStdout = 1;
    InitProc->FdStderr = 2;

    InitProc->Cred.Uid   = 0;
    InitProc->Cred.Gid   = 0;
    InitProc->Cred.Umask = 0022;

    InitProc->SigMask     = 0;
    InitProc->PendingSigs = 0;
    for (int I = 0; I < 32; I++)
    {
        InitProc->SigTable[I].Handler = 0;
        InitProc->SigTable[I].Mask    = 0;
        InitProc->SigTable[I].Flags   = 0;
    }

    StringCopy(InitProc->Cwd, "/", strlen("/"));
    StringCopy(InitProc->Root, "/", strlen("/"));

    InitProc->ExitCode = 0;
    InitProc->Zombie   = 0;

    __InitProc__ = InitProc;
    if (InsertProcLocked(InitProc) != 0)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        PError("Proc: insert PID1 failed\n");
        return -1;
    }
    ReleaseSpinLock(&__ProcTableLock__);

    PDebug("Proc: init complete, PID1=%ld\n", InitProc->PID);
    return 0;
}

Process*
ProcCreate(long __ParentPid__)
{
    Process* NewProc = (Process*)KMalloc(sizeof(Process));
    if (!NewProc)
    {
        return 0;
    }
    __builtin_memset(NewProc, 0, sizeof(Process));

    AcquireSpinLock(&__ProcTableLock__);
    long NewPid   = AllocPidLocked();
    NewProc->PID  = NewPid;
    NewProc->PPID = (__ParentPid__ > 0) ? __ParentPid__ : 0;
    NewProc->PGID = (NewProc->PPID ? NewProc->PPID : NewProc->PID);
    NewProc->SID  = (NewProc->PPID ? NewProc->PPID : NewProc->PID);

    InitFdTable(NewProc, 16);
    NewProc->FdStdin  = 0;
    NewProc->FdStdout = 1;
    NewProc->FdStderr = 2;

    NewProc->Cred.Uid   = 0;
    NewProc->Cred.Gid   = 0;
    NewProc->Cred.Umask = 0022;

    NewProc->SigMask     = 0;
    NewProc->PendingSigs = 0;
    for (int I = 0; I < 32; I++)
    {
        NewProc->SigTable[I].Handler = 0;
        NewProc->SigTable[I].Mask    = 0;
        NewProc->SigTable[I].Flags   = 0;
    }

    StringCopy(NewProc->Cwd, "/", strlen("/"));
    StringCopy(NewProc->Root, "/", strlen("/"));

    NewProc->ExitCode = 0;
    NewProc->Zombie   = 0;

    if (InsertProcLocked(NewProc) != 0)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        KFree(NewProc);
        return 0;
    }
    ReleaseSpinLock(&__ProcTableLock__);

    /* Bind main thread TCB immediately */
    Thread* Main = CreateThread(ThreadTypeUser, 0, 0, ThreadPriorityNormal);
    if (!Main)
    {
        PError("ProcCreate: CreateThread failed pid=%ld\n", NewProc->PID);
        AcquireSpinLock(&__ProcTableLock__);
        RemoveProcLocked(NewProc->PID); /* if you have it */
        ReleaseSpinLock(&__ProcTableLock__);
        KFree(NewProc);
        return 0;
    }

    Main->ProcessId = (uint32_t)NewProc->PID;
    StringCopy(Main->Name, "Main", 64);
    NewProc->MainThread = Main;

    /*To procfs*/
    ProcFsExposeProcess(NewProc);

    PDebug("Proc: create pid=%ld ppid=%ld (bound tid=%u)\n",
           NewProc->PID,
           NewProc->PPID,
           Main->ThreadId);
    return NewProc;
}

Process*
ProcFork(Process* __Parent__)
{
    if (!__Parent__)
    {
        return 0;
    }

    Process* Child = (Process*)KMalloc(sizeof(Process));
    if (!Child)
    {
        return 0;
    }

    __builtin_memcpy(Child, __Parent__, sizeof(Process)); /** metadata copy */
    Child->Zombie   = 0;
    Child->ExitCode = 0;

    AcquireSpinLock(&__ProcTableLock__);
    long NewPid = AllocPidLocked();
    Child->PID  = NewPid;
    Child->PPID = __Parent__->PID;

    if (EnsureFdTableCapacity(Child, __Parent__->FdCap) != 0)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        KFree(Child);
        return 0;
    }

    for (long I = 0; I < __Parent__->FdCap; I++)
    {
        Child->FdTable[I] = __Parent__->FdTable[I];
        if (Child->FdTable[I].Kind != PFdNone && Child->FdTable[I].Obj)
        {
            Child->FdTable[I].Refcnt += 1;
        }
    }
    Child->FdCount    = __Parent__->FdCount;
    Child->MainThread = 0; /** child thread will be created and bound later */

    if (InsertProcLocked(Child) != 0)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        KFree(Child);
        return 0;
    }
    ReleaseSpinLock(&__ProcTableLock__);

    PDebug("Proc: fork parent=%ld child=%ld\n", __Parent__->PID, Child->PID);
    return Child;
}

int
ProcExec(Process*           __Proc__,
         const char*        __Path__,
         const char* const* __Argv__,
         const char* const* __Envp__)
{
    if (!__Proc__ || !__Path__)
    {
        return -1;
    }

    (void)__Argv__;
    (void)__Envp__; /** stored later when userland ABI is ready */

    for (int I = 0; I < 32; I++)
    {
        __Proc__->SigTable[I].Handler = 0;
        __Proc__->SigTable[I].Mask    = 0;
        __Proc__->SigTable[I].Flags   = 0;
    }
    __Proc__->PendingSigs = 0;

    PDebug("Proc: exec pid=%ld path=%s\n", __Proc__->PID, __Path__);
    return 0;
}

int
ProcExit(Process* __Proc__, int __Code__)
{
    if (!__Proc__)
    {
        return -1;
    }
    __Proc__->ExitCode = __Code__;
    __Proc__->Zombie   = 1;

    for (long I = 0; I < __Proc__->FdCap; I++)
    {
        ProcFd* Entry = &__Proc__->FdTable[I];
        if (Entry->Kind == PFdNone || !Entry->Obj)
        {
            continue;
        }
        if (Entry->Refcnt > 0)
        {
            Entry->Refcnt -= 1;
        }
        if (Entry->Refcnt == 0)
        {
            Entry->Kind  = PFdNone;
            Entry->Obj   = 0;
            Entry->Flags = 0;
        }
    }

    AcquireSpinLock(&__ProcTableLock__);
    RemoveProcLocked(__Proc__->PID);
    ReleaseSpinLock(&__ProcTableLock__);

    Thread* T = (Thread*)__Proc__->MainThread;
    if (T)
    {
        T->ExitCode = (uint32_t)__Code__;
        T->State    = ThreadStateZombie;
        AddThreadToZombieQueue(T->LastCpu, T);
    }

    PDebug("Proc: exit pid=%ld code=%d\n", __Proc__->PID, __Code__);
    return 0;
}

Process*
ProcFind(long __Pid__)
{
    AcquireSpinLock(&__ProcTableLock__);
    Process* P = FindProcLocked(__Pid__);
    ReleaseSpinLock(&__ProcTableLock__);
    return P;
}

int
ProcFdEnsure(Process* __Proc__, long __Need__)
{
    return EnsureFdTableCapacity(__Proc__, __Need__);
}

long
ProcFdAlloc(Process* __Proc__, long __Flags__)
{
    if (!__Proc__)
    {
        return -1;
    }
    for (long I = 0; I < __Proc__->FdCap; I++)
    {
        ProcFd* Entry = &__Proc__->FdTable[I];
        if (Entry->Kind == PFdNone && Entry->Refcnt == 0)
        {
            Entry->Kind   = PFdVnode; /** default class; caller may bind specific kind */
            Entry->Obj    = 0;
            Entry->Flags  = __Flags__;
            Entry->Refcnt = 1;
            if (I >= __Proc__->FdCount)
            {
                __Proc__->FdCount = I + 1;
            }
            return I;
        }
    }
    if (EnsureFdTableCapacity(__Proc__, __Proc__->FdCap + 1) != 0)
    {
        return -1;
    }
    return ProcFdAlloc(__Proc__, __Flags__);
}

int
ProcFdBind(Process* __Proc__, long __Fd__, ProcFdKind __Kind__, void* __Obj__)
{
    if (!__Proc__ || __Fd__ < 0 || __Fd__ >= __Proc__->FdCap)
    {
        return -1;
    }
    ProcFd* Entry = &__Proc__->FdTable[__Fd__];
    if (Entry->Refcnt <= 0)
    {
        Entry->Refcnt = 1;
    }
    Entry->Kind = __Kind__;
    Entry->Obj  = __Obj__;
    return 0;
}

int
ProcFdClose(Process* __Proc__, long __Fd__)
{
    if (!__Proc__ || __Fd__ < 0 || __Fd__ >= __Proc__->FdCap)
    {
        return -1;
    }
    ProcFd* Entry = &__Proc__->FdTable[__Fd__];
    if (Entry->Kind == PFdNone || Entry->Refcnt <= 0)
    {
        return -1;
    }

    Entry->Refcnt -= 1;
    if (Entry->Refcnt == 0)
    {
        Entry->Kind  = PFdNone;
        Entry->Obj   = 0;
        Entry->Flags = 0;
    }
    return 0;
}

ProcFd*
ProcFdGet(Process* __Proc__, long __Fd__)
{
    if (!__Proc__ || __Fd__ < 0 || __Fd__ >= __Proc__->FdCap)
    {
        return 0;
    }
    ProcFd* Entry = &__Proc__->FdTable[__Fd__];
    return (Entry->Kind == PFdNone || Entry->Refcnt <= 0) ? 0 : Entry;
}

int
ProcSignalSend(long __Pid__, ProcSignal __Sig__)
{
    if (__Sig__ == PSigNone)
    {
        return 0;
    }
    AcquireSpinLock(&__ProcTableLock__);
    Process* P = FindProcLocked(__Pid__);
    if (!P)
    {
        ReleaseSpinLock(&__ProcTableLock__);
        return -1;
    }
    P->PendingSigs |= (1ULL << (unsigned long)__Sig__);
    ReleaseSpinLock(&__ProcTableLock__);

    Thread* T = (Thread*)P->MainThread;
    if (T)
    {
        T->WaitReason = WaitReasonSignal;
    }

    PDebug("Proc: signal pid=%ld sig=%d pend=%llx\n",
           __Pid__,
           (int)__Sig__,
           (unsigned long long)P->PendingSigs);
    return 0;
}

int
ProcSignalMask(Process* __Proc__, uint64_t __Mask__, int __SetOrClear__)
{
    if (!__Proc__)
    {
        return -1;
    }
    if (__SetOrClear__)
    {
        __Proc__->SigMask |= __Mask__;
    }
    else
    {
        __Proc__->SigMask &= ~__Mask__;
    }
    return 0;
}

int
ProcSignalSetHandler(
    Process* __Proc__, int __Sig__, void (*__Handler__)(int), uint64_t __Mask__, int __Flags__)
{
    if (!__Proc__ || __Sig__ <= 0 || __Sig__ >= 32)
    {
        return -1;
    }
    __Proc__->SigTable[__Sig__].Handler = __Handler__;
    __Proc__->SigTable[__Sig__].Mask    = __Mask__;
    __Proc__->SigTable[__Sig__].Flags   = __Flags__;
    return 0;
}

void
ProcDeliverPendingSignalsForCurrent(void)
{
    Thread* T = GetCurrentThread(GetCurrentCpuId());
    if (!T)
    {
        return;
    }
    Process* P = ProcFind((long)T->ProcessId);
    if (!P)
    {
        return;
    }

    uint64_t Pending = P->PendingSigs;
    if (Pending == 0)
    {
        return;
    }

    Pending &= ~P->SigMask;
    if (Pending == 0)
    {
        return;
    }

    for (int Sig = 1; Sig < 32; Sig++)
    {
        uint64_t Bit = (1ULL << Sig);
        if (Pending & Bit)
        {
            P->PendingSigs &= ~Bit;

            ProcSigHandler* H = &P->SigTable[Sig];
            if (H->Handler)
            {
                uint64_t OldMask = P->SigMask;
                P->SigMask |= H->Mask;
                H->Handler(Sig);
                P->SigMask = OldMask;
            }
            else
            {
                if (Sig == PSigKILL)
                {
                    ProcExit(P, 128 + PSigKILL);
                    return;
                }
                else if (Sig == PSigSTOP)
                {
                    SuspendThread(T);
                }
                else if (Sig == PSigTERM)
                {
                    ProcExit(P, 128 + PSigTERM);
                    return;
                }
                else
                {
                    PWarn("Proc: default action sig=%d pid=%ld\n", Sig, P->PID);
                }
            }
            break; /** deliver one per pass */
        }
    }
}

int
ProcSetJobControl(Process* __Proc__, long __PGID__, long __SID__)
{
    if (!__Proc__)
    {
        return -1;
    }
    if (__PGID__ > 0)
    {
        __Proc__->PGID = __PGID__;
    }
    if (__SID__ > 0)
    {
        __Proc__->SID = __SID__;
    }
    return 0;
}

int
ProcAttachTty(Process* __Proc__, const char* __TtyName__, void* __TtyCtx__)
{
    if (!__Proc__)
    {
        return -1;
    }
    __Proc__->TtyName = __TtyName__;
    __Proc__->TtyCtx  = __TtyCtx__;
    return 0;
}

int
ProcDetachTty(Process* __Proc__)
{
    if (!__Proc__)
    {
        return -1;
    }
    __Proc__->TtyName = 0;
    __Proc__->TtyCtx  = 0;
    return 0;
}

ProcCred
ProcGetCred(Process* __Proc__)
{
    ProcCred Cred = {0};
    if (!__Proc__)
    {
        return Cred;
    }
    return __Proc__->Cred;
}

int
ProcSetUidGid(Process* __Proc__, long __Uid__, long __Gid__)
{
    if (!__Proc__)
    {
        return -1;
    }
    __Proc__->Cred.Uid = __Uid__;
    __Proc__->Cred.Gid = __Gid__;
    return 0;
}

int
ProcSetUmask(Process* __Proc__, long __Umask__)
{
    if (!__Proc__)
    {
        return -1;
    }
    __Proc__->Cred.Umask = __Umask__;
    return 0;
}

long
ProcWaitPid(long __Pid__, int* __OutStatus__, int __Options__)
{
    (void)__Options__;
    Process* Child = ProcFind(__Pid__);
    if (!Child)
    {
        return -1;
    }

    while (!Child->Zombie)
    {
        ThreadYield();
    }

    if (__OutStatus__)
    {
        *__OutStatus__ = Child->ExitCode;
    }
    return Child->PID;
}

int
ProcReap(Process* __Parent__, long __ChildPid__)
{
    (void)__Parent__;
    Process* Child = ProcFind(__ChildPid__);
    if (!Child)
    {
        return -1;
    }
    if (!Child->Zombie)
    {
        return -1;
    }

    if (Child->FdTable)
    {
        KFree(Child->FdTable);
    }
    KFree(Child);
    PDebug("Proc: reaped child pid=%ld\n", __ChildPid__);
    return 0;
}

long
GetPid(void)
{
    Thread* T = GetCurrentThread(GetCurrentCpuId());
    if (!T)
    {
        return -1;
    }
    return (long)T->ProcessId;
}