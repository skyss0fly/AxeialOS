#include <AllTypes.h>
#include <KHeap.h>
#include <KrnPrintf.h>
#include <String.h>
#include <VFS.h>

static const long __MaxFsTypes__ = 32;
static const long __MaxMounts__  = 64;

static const FsType* __FsReg__[32];
static long          __FsCount__ = 0;

typedef struct __MountEntry__
{
    Superblock* Sb;
    char        Path[1024];

} __MountEntry__;

static __MountEntry__ __Mounts__[64];
static long           __MountCount__ = 0;

static Vnode*  __RootNode__ = 0;
static Dentry* __RootDe__   = 0;

static long  __Umask__          = 0;
static long  __MaxName__        = 256;
static long  __MaxPath__        = 1024;
static long  __DirCacheLimit__  = 0;
static long  __FileCacheLimit__ = 0;
static long  __IoBlockSize__    = 0;
static char  __DefaultFs__[64]  = {0};
static Mutex VfsLock;

static int
__is_sep__(char c)
{
    return c == '/';
}
static const char*
__skip_sep__(const char* __Path)
{
    while (__Path && __is_sep__(*__Path))
    {
        __Path++;
    }
    return __Path;
}
static long
__next_comp__(const char* __Path, char* __Output, long __Cap)
{
    if (!__Path || !*__Path)
    {
        return 0;
    }
    const char* s = __Path;
    long        N = 0;
    while (*s && !__is_sep__(*s))
    {
        if (N + 1 < __Cap)
        {
            __Output[N++] = *s;
        }
        s++;
    }
    __Output[N] = 0;
    return N;
}

static Dentry*
__alloc_dentry__(const char* __Name__, Dentry* __Parent__, Vnode* __Node__)
{
    Dentry* De = (Dentry*)KMalloc(sizeof(Dentry));
    if (!De)
    {
        return 0;
    }
    De->Name   = __Name__;
    De->Parent = __Parent__;
    De->Node   = __Node__;
    De->Flags  = 0;
    return De;
}

static Dentry*
__walk__(Vnode* __StartNode__, Dentry* __StartDe__, const char* __Path__)
{
    if (!__StartNode__ || !__Path__)
    {
        return 0;
    }
    const char* __Path = __Path__;
    if (__is_sep__(*__Path))
    {
        __Path = __skip_sep__(__Path);
    }
    Vnode*  Cur    = __StartNode__;
    Dentry* Parent = __StartDe__;
    char    Comp[256];
    while (*__Path)
    {
        long N = __next_comp__(__Path, Comp, sizeof(Comp));
        if (N <= 0)
        {
            break;
        }
        while (*__Path && !__is_sep__(*__Path))
        {
            __Path++;
        }
        __Path = __skip_sep__(__Path);
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            return 0;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Comp);
        if (!Next)
        {
            return 0;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return 0;
        }
        __builtin_memcpy(Dup, Comp, (size_t)(N + 1));
        Dentry* De = __alloc_dentry__(Dup, Parent, Next);
        if (!De)
        {
            return 0;
        }
        Parent = De;
        Cur    = Next;
    }
    return Parent;
}

static __MountEntry__*
__find_mount__(const char* __Path__)
{
    long Best    = -1;
    long BestLen = -1;
    for (long I = 0; I < __MountCount__; I++)
    {
        const char* Mp = __Mounts__[I].Path;
        long        Ml = (long)strlen(Mp);
        if (Ml <= 0)
        {
            continue;
        }
        if (strncmp(__Path__, Mp, (size_t)Ml) == 0)
        {
            if (Ml > BestLen)
            {
                Best    = I;
                BestLen = Ml;
            }
        }
    }
    return Best >= 0 ? &__Mounts__[Best] : 0;
}

int
VfsInit(void)
{
    AcquireMutex(&VfsLock);
    InitializeMutex(&VfsLock, "vfs-central");
    __FsCount__        = 0;
    __MountCount__     = 0;
    __RootNode__       = 0;
    __RootDe__         = 0;
    __Umask__          = 0;
    __MaxName__        = 256;
    __MaxPath__        = 1024;
    __DirCacheLimit__  = 0;
    __FileCacheLimit__ = 0;
    __IoBlockSize__    = 0;
    __DefaultFs__[0]   = 0;
    PDebug("VFS: Init\n");
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsShutdown(void)
{
    AcquireMutex(&VfsLock);
    for (long I = 0; I < __MountCount__; I++)
    {
        Superblock* Sb = __Mounts__[I].Sb;
        if (Sb && Sb->Ops && Sb->Ops->Umount)
        {
            Sb->Ops->Umount(Sb);
        }
        if (Sb && Sb->Ops && Sb->Ops->Release)
        {
            Sb->Ops->Release(Sb);
        }
        __Mounts__[I].Sb      = 0;
        __Mounts__[I].Path[0] = 0;
    }
    __MountCount__ = 0;
    __FsCount__    = 0;
    __RootNode__   = 0;
    __RootDe__     = 0;
    PDebug("VFS: Shutdown\n");
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsRegisterFs(const FsType* __FsType__)
{
    AcquireMutex(&VfsLock);
    if (!__FsType__ || !__FsType__->Name || !__FsType__->Mount)
    {
        PError("VFS: RegisterFs invalid\n");
        return -1;
    }

    if (__FsCount__ >= __MaxFsTypes__)
    {
        PError("VFS: RegisterFs full: %s\n", __FsType__->Name);
        return -1;
    }

    for (long I = 0; I < __FsCount__; I++)
    {
        if (strcmp(__FsReg__[I]->Name, __FsType__->Name) == 0)
        {
            PWarn("VFS: FS exists %s\n", __FsType__->Name);
            return -1;
        }
    }

    __FsReg__[__FsCount__++] = __FsType__;
    PDebug("VFS: FS registered %s\n", __FsType__->Name);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsUnregisterFs(const char* __Name__)
{
    AcquireMutex(&VfsLock);
    if (!__Name__)
    {
        PError("VFS: UnregisterFs NULL\n");
        return -1;
    }

    for (long I = 0; I < __FsCount__; I++)
    {
        if (strcmp(__FsReg__[I]->Name, __Name__) == 0)
        {
            for (long J = I; J < __FsCount__ - 1; J++)
            {
                __FsReg__[J] = __FsReg__[J + 1];
            }
            __FsReg__[--__FsCount__] = 0;
            PDebug("VFS: FS unregistered %s\n", __Name__);
            return 0;
        }
    }

    PError("VFS: FS not found %s\n", __Name__);
    ReleaseMutex(&VfsLock);
    return -1;
}

const FsType*
VfsFindFs(const char* __Name__)
{
    if (!__Name__)
    {
        return 0;
    }

    for (long I = 0; I < __FsCount__; I++)
    {
        if (strcmp(__FsReg__[I]->Name, __Name__) == 0)
        {
            return __FsReg__[I];
        }
    }

    return 0;
}

long
VfsListFs(const char** __Out__, long __Cap__)
{
    AcquireMutex(&VfsLock);
    if (!__Out__ || __Cap__ <= 0)
    {
        return -1;
    }

    long N = (__FsCount__ < __Cap__) ? __FsCount__ : __Cap__;
    for (long I = 0; I < N; I++)
    {
        __Out__[I] = __FsReg__[I]->Name;
    }

    ReleaseMutex(&VfsLock);
    return N;
}

Superblock*
VfsMount(const char* __Dev__,
         const char* __Path__,
         const char* __Type__,
         long        __Flags__,
         const char* __Opts__)
{
    AcquireMutex(&VfsLock);
    const FsType* Fs = VfsFindFs(__Type__);
    if (!Fs)
    {
        PError("VFS: Mount unknown FS %s\n", __Type__);
        return 0;
    }

    if (!__Path__ || !*__Path__)
    {
        PError("VFS: Mount invalid path\n");
        return 0;
    }

    long Plen = (long)strlen(__Path__);
    if (Plen <= 0 || Plen >= __MaxPath__)
    {
        PError("VFS: Mount path len invalid\n");
        return 0;
    }

    if (__MountCount__ >= __MaxMounts__)
    {
        PError("VFS: Mount table full\n");
        return 0;
    }

    Superblock* Sb = Fs->Mount(__Dev__, __Opts__);
    if (!Sb || !Sb->Root)
    {
        PError("VFS: Mount failed %s on %s\n", __Type__, __Path__);
        return 0;
    }

    __MountEntry__* M = &__Mounts__[__MountCount__++];
    M->Sb             = Sb;
    __builtin_memcpy(M->Path, __Path__, (size_t)(Plen + 1));

    if (!__RootNode__ && strcmp(__Path__, "/") == 0)
    {
        __RootNode__ = Sb->Root;
        __RootDe__   = __alloc_dentry__("/", 0, __RootNode__);
        PDebug("VFS: Root mounted /\n");
    }

    PDebug("VFS: Mounted %s at %s\n", __Type__, __Path__);
    (void)__Flags__;
    ReleaseMutex(&VfsLock);
    return Sb;
}

int
VfsUnmount(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__)
    {
        PError("VFS: Unmount NULL\n");
        return -1;
    }

    for (long I = 0; I < __MountCount__; I++)
    {
        if (strcmp(__Mounts__[I].Path, __Path__) == 0)
        {
            Superblock* Sb = __Mounts__[I].Sb;
            if (Sb && Sb->Ops && Sb->Ops->Umount)
            {
                Sb->Ops->Umount(Sb);
            }
            if (Sb && Sb->Ops && Sb->Ops->Release)
            {
                Sb->Ops->Release(Sb);
            }
            for (long J = I; J < __MountCount__ - 1; J++)
            {
                __Mounts__[J] = __Mounts__[J + 1];
            }
            __Mounts__[--__MountCount__].Sb    = 0;
            __Mounts__[__MountCount__].Path[0] = 0;

            if (strcmp(__Path__, "/") == 0)
            {
                __RootNode__ = 0;
                __RootDe__   = 0;
            }
            PDebug("VFS: Unmounted %s\n", __Path__);
            return 0;
        }
    }

    PError("VFS: Unmount path not found %s\n", __Path__);
    ReleaseMutex(&VfsLock);
    return -1;
}

int
VfsSwitchRoot(const char* __NewRoot__)
{
    AcquireMutex(&VfsLock);
    if (!__NewRoot__)
    {
        PError("VFS: SwitchRoot NULL\n");
        return -1;
    }

    Dentry* De = VfsResolve(__NewRoot__);
    if (!De || !De->Node)
    {
        PError("VFS: SwitchRoot resolve failed %s\n", __NewRoot__);
        return -1;
    }

    __RootNode__ = De->Node;
    __RootDe__   = De;
    PDebug("VFS: Root switched to %s\n", __NewRoot__);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsBindMount(const char* __Src__, const char* __Dst__)
{
    AcquireMutex(&VfsLock);
    if (!__Src__ || !__Dst__)
    {
        return -1;
    }

    __MountEntry__* M = __find_mount__(__Src__);
    if (!M || !M->Sb)
    {
        return -1;
    }

    if (__MountCount__ >= __MaxMounts__)
    {
        return -1;
    }

    long N = (long)strlen(__Dst__);
    if (N <= 0 || N >= __MaxPath__)
    {
        return -1;
    }

    __MountEntry__* New = &__Mounts__[__MountCount__++];
    New->Sb             = M->Sb;
    __builtin_memcpy(New->Path, __Dst__, (size_t)(N + 1));

    PDebug("VFS: Bind mount %s -> %s\n", __Src__, __Dst__);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsMoveMount(const char* __Src__, const char* __Dst__)
{
    AcquireMutex(&VfsLock);
    if (!__Src__ || !__Dst__)
    {
        return -1;
    }

    __MountEntry__* M = __find_mount__(__Src__);
    if (!M || !M->Sb)
    {
        return -1;
    }

    long N = (long)strlen(__Dst__);
    if (N <= 0 || N >= __MaxPath__)
    {
        return -1;
    }

    __builtin_memcpy(M->Path, __Dst__, (size_t)(N + 1));
    PDebug("VFS: Move mount %s -> %s\n", __Src__, __Dst__);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsRemount(const char* __Path__, long __Flags__, const char* __Opts__)
{
    AcquireMutex(&VfsLock);
    (void)__Flags__;
    (void)__Opts__;
    __MountEntry__* M = __find_mount__(__Path__);
    if (!M || !M->Sb)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return 0;
}

Dentry*
VfsResolve(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__)
    {
        return 0;
    }

    if (!__RootNode__)
    {
        return 0;
    }

    if (strcmp(__Path__, "/") == 0)
    {
        return __RootDe__;
    }

    __MountEntry__* M = __find_mount__(__Path__);
    if (!M)
    {
        /*Walk from the global root for non-mounted prefixes*/
        return __walk__(__RootNode__, __RootDe__, __Path__);
    }

    /*Strip the mount path prefix before walking from the mount root*/
    const char* Mp = M->Path;
    long        Ml = (long)strlen(Mp);

    /*If the path is exactly the mount point, return a dentry for the mount root*/
    if (strncmp(__Path__, Mp, (size_t)Ml) == 0 && __Path__[Ml] == '\0')
    {
        /*Construct a dentry anchored at mount root*/
        Dentry* De = __alloc_dentry__(Mp, __RootDe__, M->Sb->Root);
        return De ? De : 0;
    }

    /*Otherwise, walk the subpath tail from the mount's root*/
    const char* Tail = __Path__ + Ml;
    /*Skip extra slashes to normalize*/
    while (*Tail == '/')
    {
        Tail++;
    }

    /*if tail is empty after stripping, we are at mount root*/
    if (!*Tail)
    {
        Dentry* De = __alloc_dentry__(Mp, __RootDe__, M->Sb->Root);
        return De ? De : 0;
    }

    ReleaseMutex(&VfsLock);
    return __walk__(M->Sb->Root, __RootDe__, Tail);
}

Dentry*
VfsResolveAt(Dentry* __Base__, const char* __Rel__)
{
    AcquireMutex(&VfsLock);
    if (!__Base__ || !__Base__->Node || !__Rel__)
    {
        return 0;
    }

    if (!*__Rel__)
    {
        return __Base__;
    }

    if (__is_sep__(*__Rel__))
    {
        return VfsResolve(__Rel__);
    }

    ReleaseMutex(&VfsLock);
    return __walk__(__Base__->Node, __Base__, __Rel__);
}

Vnode*
VfsLookup(Dentry* __Base__, const char* __Name__)
{
    AcquireMutex(&VfsLock);
    if (!__Base__ || !__Base__->Node || !__Name__)
    {
        return 0;
    }

    if (!__Base__->Node->Ops || !__Base__->Node->Ops->Lookup)
    {
        return 0;
    }

    ReleaseMutex(&VfsLock);
    return __Base__->Node->Ops->Lookup(__Base__->Node, __Name__);
}

int
VfsMkpath(const char* __Path__, long __Perm__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__)
    {
        return -1;
    }
    const char* p = __Path__;
    if (__is_sep__(*p))
    {
        p = __skip_sep__(p);
    }

    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    char    Comp[256];

    while (*p)
    {
        long N = __next_comp__(p, Comp, sizeof(Comp));
        if (N <= 0)
        {
            break;
        }
        while (*p && !__is_sep__(*p))
        {
            p++;
        }
        p = __skip_sep__(p);

        Vnode* Next = Cur->Ops ? Cur->Ops->Lookup(Cur, Comp) : 0;
        if (!Next)
        {
            if (!Cur->Ops || !Cur->Ops->Mkdir)
            {
                return -1;
            }
            VfsPerm perm = {.Mode = __Perm__, .Uid = 0, .Gid = 0};
            if (Cur->Ops->Mkdir(Cur, Comp, perm) != 0)
            {
                return -1;
            }
            Next = Cur->Ops->Lookup(Cur, Comp);
            if (!Next)
            {
                return -1;
            }
        }
        char* Dup = (char*)KMalloc(N + 1);
        __builtin_memcpy(Dup, Comp, N + 1);
        De  = __alloc_dentry__(Dup, De, Next);
        Cur = Next;
    }
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsRealpath(const char* __Path__, char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    long L = (long)strlen(__Path__);
    if (L >= __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Buf__, __Path__, (size_t)(L + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

File*
VfsOpen(const char* __Path__, long __Flags__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        PError("VFS: Open resolve failed %s\n", __Path__);
        return 0;
    }

    if (!De->Node->Ops || !De->Node->Ops->Open)
    {
        PError("VFS: Open not supported %s\n", __Path__);
        return 0;
    }

    File* F = (File*)KMalloc(sizeof(File));
    if (!F)
    {
        return 0;
    }

    F->Node   = De->Node;
    F->Offset = 0;
    F->Flags  = __Flags__;
    F->Refcnt = 1;
    F->Priv   = 0;

    if (De->Node->Ops->Open(De->Node, F) != 0)
    {
        KFree(F);
        PError("VFS: Open failed %s\n", __Path__);
        return 0;
    }

    PDebug("VFS: Open %s\n", __Path__);
    ReleaseMutex(&VfsLock);
    return F;
}

File*
VfsOpenAt(Dentry* __Base__, const char* __Rel__, long __Flags__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolveAt(__Base__, __Rel__);
    if (!De || !De->Node)
    {
        return 0;
    }

    if (!De->Node->Ops || !De->Node->Ops->Open)
    {
        return 0;
    }

    File* F = (File*)KMalloc(sizeof(File));
    if (!F)
    {
        return 0;
    }

    F->Node   = De->Node;
    F->Offset = 0;
    F->Flags  = __Flags__;
    F->Refcnt = 1;
    F->Priv   = 0;

    if (De->Node->Ops->Open(De->Node, F) != 0)
    {
        KFree(F);
        return 0;
    }

    ReleaseMutex(&VfsLock);
    return F;
}

int
VfsClose(File* __File__)
{
    AcquireMutex(&VfsLock);
    if (!__File__)
    {
        return -1;
    }

    if (__File__->Node && __File__->Node->Ops && __File__->Node->Ops->Close)
    {
        __File__->Node->Ops->Close(__File__);
    }

    KFree(__File__);
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsRead(File* __File__, void* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__File__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    if (!__File__->Node || !__File__->Node->Ops || !__File__->Node->Ops->Read)
    {
        return -1;
    }

    long Got = __File__->Node->Ops->Read(__File__, __Buf__, __Len__);
    if (Got > 0)
    {
        __File__->Offset += Got;
    }
    ReleaseMutex(&VfsLock);
    return Got;
}

long
VfsWrite(File* __File__, const void* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__File__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    if (!__File__->Node || !__File__->Node->Ops || !__File__->Node->Ops->Write)
    {
        return -1;
    }

    long Put = __File__->Node->Ops->Write(__File__, __Buf__, __Len__);
    if (Put > 0)
    {
        __File__->Offset += Put;
    }
    ReleaseMutex(&VfsLock);
    return Put;
}

long
VfsLseek(File* __File__, long __Off__, int __Whence__)
{
    AcquireMutex(&VfsLock);
    if (!__File__)
    {
        return -1;
    }

    if (!__File__->Node || !__File__->Node->Ops || !__File__->Node->Ops->Lseek)
    {
        return -1;
    }

    long New = __File__->Node->Ops->Lseek(__File__, __Off__, __Whence__);
    if (New >= 0)
    {
        __File__->Offset = New;
    }
    ReleaseMutex(&VfsLock);
    return New;
}

int
VfsIoctl(File* __File__, unsigned long __Cmd__, void* __Arg__)
{
    AcquireMutex(&VfsLock);
    if (!__File__)
    {
        return -1;
    }

    if (!__File__->Node || !__File__->Node->Ops || !__File__->Node->Ops->Ioctl)
    {
        return -1;
    }

    ReleaseMutex(&VfsLock);
    return __File__->Node->Ops->Ioctl(__File__, __Cmd__, __Arg__);
}

int
VfsFsync(File* __File__)
{
    AcquireMutex(&VfsLock);
    if (!__File__ || !__File__->Node || !__File__->Node->Ops)
    {
        return -1;
    }

    if (!__File__->Node->Ops->Sync)
    {
        return 0;
    }

    ReleaseMutex(&VfsLock);
    return __File__->Node->Ops->Sync(__File__->Node);
}

int
VfsFstats(File* __File__, VfsStat* __Buf__)
{
    AcquireMutex(&VfsLock);
    if (!__File__ || !__Buf__)
    {
        return -1;
    }

    if (!__File__->Node || !__File__->Node->Ops || !__File__->Node->Ops->Stat)
    {
        return -1;
    }

    ReleaseMutex(&VfsLock);
    return __File__->Node->Ops->Stat(__File__->Node, __Buf__);
}

int
VfsStats(const char* __Path__, VfsStat* __Buf__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Buf__)
    {
        return -1;
    }

    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        return -1;
    }

    if (!De->Node->Ops || !De->Node->Ops->Stat)
    {
        return -1;
    }

    ReleaseMutex(&VfsLock);
    return De->Node->Ops->Stat(De->Node, __Buf__);
}

long
VfsReaddir(const char* __Path__, void* __Buf__, long __BufLen__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Buf__ || __BufLen__ <= 0)
    {
        return -1;
    }

    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        return -1;
    }

    if (!De->Node->Ops || !De->Node->Ops->Readdir)
    {
        return -1;
    }

    ReleaseMutex(&VfsLock);
    return De->Node->Ops->Readdir(De->Node, __Buf__, __BufLen__);
}

long
VfsReaddirF(File* __Dir__, void* __Buf__, long __BufLen__)
{
    AcquireMutex(&VfsLock);
    if (!__Dir__ || !__Buf__ || __BufLen__ <= 0)
    {
        return -1;
    }

    if (!__Dir__->Node || !__Dir__->Node->Ops || !__Dir__->Node->Ops->Readdir)
    {
        return -1;
    }

    ReleaseMutex(&VfsLock);
    return __Dir__->Node->Ops->Readdir(__Dir__->Node, __Buf__, __BufLen__);
}

int
VfsCreate(const char* __Path__, long __Flags__, VfsPerm __Perm__)
{
    AcquireMutex(&VfsLock);
    Dentry* Parent = 0;
    char    Name[256];
    if (!__Path__)
    {
        return -1;
    }
    const char* __Path = __Path__;
    if (__is_sep__(*__Path))
    {
        __Path = __skip_sep__(__Path);
    }
    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    while (*__Path)
    {
        long N = __next_comp__(__Path, Name, sizeof(Name));
        if (N <= 0)
        {
            break;
        }
        while (*__Path && !__is_sep__(*__Path))
        {
            __Path++;
        }
        __Path = __skip_sep__(__Path);
        Parent = De;
        if (*__Path == 0)
        {
            break;
        }
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Name);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return -1;
        }
        __builtin_memcpy(Dup, Name, (size_t)(N + 1));
        De = __alloc_dentry__(Dup, De, Next);
        if (!De)
        {
            return -1;
        }
        Cur = Next;
    }
    if (!Parent || !Parent->Node || !Parent->Node->Ops || !Parent->Node->Ops->Create)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return Parent->Node->Ops->Create(Parent->Node, Name, __Flags__, __Perm__);
}

int
VfsUnlink(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    Dentry* Base = 0;
    char    Name[256];
    if (!__Path__)
    {
        return -1;
    }
    const char* __Path = __Path__;
    if (__is_sep__(*__Path))
    {
        __Path = __skip_sep__(__Path);
    }
    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    while (*__Path)
    {
        long N = __next_comp__(__Path, Name, sizeof(Name));
        if (N <= 0)
        {
            break;
        }
        while (*__Path && !__is_sep__(*__Path))
        {
            __Path++;
        }
        __Path = __skip_sep__(__Path);
        if (*__Path == 0)
        {
            Base = De;
            break;
        }
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Name);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return -1;
        }
        __builtin_memcpy(Dup, Name, (size_t)(N + 1));
        De = __alloc_dentry__(Dup, De, Next);
        if (!De)
        {
            return -1;
        }
        Cur = Next;
    }
    if (!Base || !Base->Node || !Base->Node->Ops || !Base->Node->Ops->Unlink)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return Base->Node->Ops->Unlink(Base->Node, Name);
}

int
VfsMkdir(const char* __Path__, VfsPerm __Perm__)
{
    AcquireMutex(&VfsLock);
    Dentry* Base = 0;
    char    Name[256];
    if (!__Path__)
    {
        ReleaseMutex(&VfsLock);
        return -1;
    }
    const char* __PathCur = __Path__;
    if (__is_sep__(*__PathCur))
    {
        __PathCur = __skip_sep__(__PathCur);
    }
    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    while (*__PathCur)
    {
        long N = __next_comp__(__PathCur, Name, sizeof(Name));
        if (N <= 0)
        {
            break;
        }
        while (*__PathCur && !__is_sep__(*__PathCur))
        {
            __PathCur++;
        }
        __PathCur = __skip_sep__(__PathCur);
        if (*__PathCur == 0)
        {
            Base = De;
            break;
        }
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            ReleaseMutex(&VfsLock);
            return -1;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Name);
        if (!Next)
        {
            ReleaseMutex(&VfsLock);
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            ReleaseMutex(&VfsLock);
            return -1;
        }
        __builtin_memcpy(Dup, Name, (size_t)(N + 1));
        De = __alloc_dentry__(Dup, De, Next);
        if (!De)
        {
            ReleaseMutex(&VfsLock);
            return -1;
        }
        Cur = Next;
    }
    if (!Base || !Base->Node || !Base->Node->Ops || !Base->Node->Ops->Mkdir)
    {
        ReleaseMutex(&VfsLock);
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return Base->Node->Ops->Mkdir(Base->Node, Name, __Perm__);
}

int
VfsRmdir(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    Dentry* Base = 0;
    char    Name[256];
    if (!__Path__)
    {
        return -1;
    }
    const char* p = __Path__;
    if (__is_sep__(*p))
    {
        p = __skip_sep__(p);
    }
    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    while (*p)
    {
        long N = __next_comp__(p, Name, sizeof(Name));
        if (N <= 0)
        {
            break;
        }
        while (*p && !__is_sep__(*p))
        {
            p++;
        }
        p = __skip_sep__(p);
        if (*p == 0)
        {
            Base = De;
            break;
        }
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Name);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc(N + 1);
        __builtin_memcpy(Dup, Name, N + 1);
        De  = __alloc_dentry__(Dup, De, Next);
        Cur = Next;
    }
    if (!Base || !Base->Node || !Base->Node->Ops || !Base->Node->Ops->Rmdir)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return Base->Node->Ops->Rmdir(Base->Node, Name);
}

int
VfsSymlink(const char* __Target__, const char* __LinkPath__, VfsPerm __Perm__)
{
    AcquireMutex(&VfsLock);
    Dentry* Base = 0;
    char    Name[256];
    if (!__LinkPath__ || !__Target__)
    {
        return -1;
    }
    const char* __Path = __LinkPath__;
    if (__is_sep__(*__Path))
    {
        __Path = __skip_sep__(__Path);
    }
    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    while (*__Path)
    {
        long N = __next_comp__(__Path, Name, sizeof(Name));
        if (N <= 0)
        {
            break;
        }
        while (*__Path && !__is_sep__(*__Path))
        {
            __Path++;
        }
        __Path = __skip_sep__(__Path);
        if (*__Path == 0)
        {
            Base = De;
            break;
        }
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Name);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return -1;
        }
        __builtin_memcpy(Dup, Name, (size_t)(N + 1));
        De = __alloc_dentry__(Dup, De, Next);
        if (!De)
        {
            return -1;
        }
        Cur = Next;
    }
    if (!Base || !Base->Node || !Base->Node->Ops || !Base->Node->Ops->Symlink)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return Base->Node->Ops->Symlink(Base->Node, Name, __Target__, __Perm__);
}

int
VfsReadlink(const char* __Path__, char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        return -1;
    }
    if (!De->Node->Ops || !De->Node->Ops->Readlink)
    {
        return -1;
    }

    VfsNameBuf NB;
    NB.Buf = __Buf__;
    NB.Len = __Len__;
    ReleaseMutex(&VfsLock);
    return De->Node->Ops->Readlink(De->Node, &NB);
}

int
VfsLink(const char* __OldPath__, const char* __NewPath__)
{
    AcquireMutex(&VfsLock);
    if (!__OldPath__ || !__NewPath__)
    {
        return -1;
    }

    Dentry* OldDe   = VfsResolve(__OldPath__);
    Dentry* NewBase = 0;
    char    Name[256];

    if (!OldDe || !OldDe->Node)
    {
        return -1;
    }

    const char* __Path = __NewPath__;
    if (__is_sep__(*__Path))
    {
        __Path = __skip_sep__(__Path);
    }
    Vnode*  Cur = __RootNode__;
    Dentry* De  = __RootDe__;
    while (*__Path)
    {
        long N = __next_comp__(__Path, Name, sizeof(Name));
        if (N <= 0)
        {
            break;
        }
        while (*__Path && !__is_sep__(*__Path))
        {
            __Path++;
        }
        __Path = __skip_sep__(__Path);
        if (*__Path == 0)
        {
            NewBase = De;
            break;
        }
        if (!Cur || !Cur->Ops || !Cur->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = Cur->Ops->Lookup(Cur, Name);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return -1;
        }
        __builtin_memcpy(Dup, Name, (size_t)(N + 1));
        De = __alloc_dentry__(Dup, De, Next);
        if (!De)
        {
            return -1;
        }
        Cur = Next;
    }

    if (!NewBase || !NewBase->Node || !NewBase->Node->Ops || !NewBase->Node->Ops->Link)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return NewBase->Node->Ops->Link(NewBase->Node, OldDe->Node, Name);
}

int
VfsRename(const char* __OldPath__, const char* __NewPath__, long __Flags__)
{
    AcquireMutex(&VfsLock);
    Dentry* OldBase = 0;
    Dentry* NewBase = 0;
    char    OldName[256];
    char    NewName[256];

    if (!__OldPath__ || !__NewPath__)
    {
        return -1;
    }

    const char* po = __OldPath__;
    if (__is_sep__(*po))
    {
        po = __skip_sep__(po);
    }
    Vnode*  CurO = __RootNode__;
    Dentry* DeO  = __RootDe__;
    while (*po)
    {
        long N = __next_comp__(po, OldName, sizeof(OldName));
        if (N <= 0)
        {
            break;
        }
        while (*po && !__is_sep__(*po))
        {
            po++;
        }
        po = __skip_sep__(po);
        if (*po == 0)
        {
            OldBase = DeO;
            break;
        }
        if (!CurO || !CurO->Ops || !CurO->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = CurO->Ops->Lookup(CurO, OldName);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return -1;
        }
        __builtin_memcpy(Dup, OldName, (size_t)(N + 1));
        DeO = __alloc_dentry__(Dup, DeO, Next);
        if (!DeO)
        {
            return -1;
        }
        CurO = Next;
    }

    const char* pn = __NewPath__;
    if (__is_sep__(*pn))
    {
        pn = __skip_sep__(pn);
    }
    Vnode*  CurN = __RootNode__;
    Dentry* DeN  = __RootDe__;
    while (*pn)
    {
        long N = __next_comp__(pn, NewName, sizeof(NewName));
        if (N <= 0)
        {
            break;
        }
        while (*pn && !__is_sep__(*pn))
        {
            pn++;
        }
        pn = __skip_sep__(pn);
        if (*pn == 0)
        {
            NewBase = DeN;
            break;
        }
        if (!CurN || !CurN->Ops || !CurN->Ops->Lookup)
        {
            return -1;
        }
        Vnode* Next = CurN->Ops->Lookup(CurN, NewName);
        if (!Next)
        {
            return -1;
        }
        char* Dup = (char*)KMalloc((size_t)(N + 1));
        if (!Dup)
        {
            return -1;
        }
        __builtin_memcpy(Dup, NewName, (size_t)(N + 1));
        DeN = __alloc_dentry__(Dup, DeN, Next);
        if (!DeN)
        {
            return -1;
        }
        CurN = Next;
    }

    if (!OldBase || !NewBase)
    {
        return -1;
    }
    if (!OldBase->Node || !NewBase->Node)
    {
        return -1;
    }
    if (!OldBase->Node->Ops || !OldBase->Node->Ops->Rename)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return OldBase->Node->Ops->Rename(OldBase->Node, OldName, NewBase->Node, NewName, __Flags__);
}

int
VfsChmod(const char* __Path__, long __Mode__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        return -1;
    }
    if (!De->Node->Ops || !De->Node->Ops->Chmod)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return De->Node->Ops->Chmod(De->Node, __Mode__);
}

int
VfsChown(const char* __Path__, long __Uid__, long __Gid__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        return -1;
    }
    if (!De->Node->Ops || !De->Node->Ops->Chown)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return De->Node->Ops->Chown(De->Node, __Uid__, __Gid__);
}

int
VfsTruncate(const char* __Path__, long __Len__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    if (!De || !De->Node)
    {
        return -1;
    }
    if (!De->Node->Ops || !De->Node->Ops->Truncate)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return De->Node->Ops->Truncate(De->Node, __Len__);
}

int
VnodeRefInc(Vnode* __Node__)
{
    AcquireMutex(&VfsLock);
    if (!__Node__)
    {
        return -1;
    }
    __Node__->Refcnt++;
    ReleaseMutex(&VfsLock);
    return (int)__Node__->Refcnt;
}

int
VnodeRefDec(Vnode* __Node__)
{
    AcquireMutex(&VfsLock);
    if (!__Node__)
    {
        return -1;
    }
    if (__Node__->Refcnt > 0)
    {
        __Node__->Refcnt--;
    }
    ReleaseMutex(&VfsLock);
    return (int)__Node__->Refcnt;
}

int
VnodeGetAttr(Vnode* __Node__, VfsStat* __Buf__)
{
    AcquireMutex(&VfsLock);
    if (!__Node__ || !__Buf__)
    {
        return -1;
    }
    if (!__Node__->Ops || !__Node__->Ops->Stat)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return __Node__->Ops->Stat(__Node__, __Buf__);
}

int
VnodeSetAttr(Vnode* __Node__, const VfsStat* __Buf__)
{
    AcquireMutex(&VfsLock);
    (void)__Node__;
    (void)__Buf__;
    ReleaseMutex(&VfsLock);
    return -1;
}

int
DentryInvalidate(Dentry* __De__)
{
    AcquireMutex(&VfsLock);
    if (!__De__)
    {
        return -1;
    }
    __De__->Flags |= 1;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
DentryRevalidate(Dentry* __De__)
{
    AcquireMutex(&VfsLock);
    if (!__De__)
    {
        return -1;
    }
    __De__->Flags &= ~1;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
DentryAttach(Dentry* __De__, Vnode* __Node__)
{
    AcquireMutex(&VfsLock);
    if (!__De__ || !__Node__)
    {
        return -1;
    }
    __De__->Node = __Node__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
DentryDetach(Dentry* __De__)
{
    AcquireMutex(&VfsLock);
    if (!__De__)
    {
        return -1;
    }
    __De__->Node = 0;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
DentryName(Dentry* __De__, char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__De__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    long N = (long)strlen(__De__->Name);
    if (N >= __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Buf__, __De__->Name, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsSetCwd(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsGetCwd(char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    const char* __Path = "/";
    long        N      = (long)strlen(__Path);
    if (N >= __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Buf__, __Path, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsSetRoot(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return VfsSwitchRoot(__Path__);
}

int
VfsGetRoot(char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    const char* __Path = "/";
    long        N      = (long)strlen(__Path);
    if (N >= __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Buf__, __Path, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsSetUmask(long __Mode__)
{
    AcquireMutex(&VfsLock);
    __Umask__ = __Mode__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetUmask(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return __Umask__;
}

int
VfsNotifySubscribe(const char* __Path__, long __Mask__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    (void)__Mask__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsNotifyUnsubscribe(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsNotifyPoll(const char* __Path__, long* __OutMask__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    if (!__OutMask__)
    {
        return -1;
    }
    *__OutMask__ = 0;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsAccess(const char* __Path__, long __Mode__)
{
    AcquireMutex(&VfsLock);
    (void)__Mode__;
    Dentry* De = VfsResolve(__Path__);
    ReleaseMutex(&VfsLock);
    return De ? 0 : -1;
}

int
VfsExists(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    ReleaseMutex(&VfsLock);
    return De ? 1 : 0;
}

int
VfsIsDir(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    ReleaseMutex(&VfsLock);
    return (De && De->Node && De->Node->Type == VNodeDIR) ? 1 : 0;
}

int
VfsIsFile(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    ReleaseMutex(&VfsLock);
    return (De && De->Node && De->Node->Type == VNodeFILE) ? 1 : 0;
}

int
VfsIsSymlink(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    Dentry* De = VfsResolve(__Path__);
    ReleaseMutex(&VfsLock);
    return (De && De->Node && De->Node->Type == VNodeSYM) ? 1 : 0;
}

int
VfsCopy(const char* __Src__, const char* __Dst__, long __Flags__)
{
    AcquireMutex(&VfsLock);
    (void)__Flags__;
    File* S = VfsOpen(__Src__, VFlgRDONLY);
    if (!S)
    {
        return -1;
    }
    File* D = VfsOpen(__Dst__, VFlgCREATE | VFlgWRONLY | VFlgTRUNC);
    if (!D)
    {
        VfsClose(S);
        return -1;
    }

    char Buf[4096];
    for (;;)
    {
        long r = VfsRead(S, Buf, sizeof(Buf));
        if (r < 0)
        {
            VfsClose(S);
            VfsClose(D);
            return -1;
        }
        if (r == 0)
        {
            break;
        }
        long w = VfsWrite(D, Buf, r);
        if (w != r)
        {
            VfsClose(S);
            VfsClose(D);
            return -1;
        }
    }

    VfsClose(S);
    VfsClose(D);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsMove(const char* __Src__, const char* __Dst__, long __Flags__)
{
    AcquireMutex(&VfsLock);
    int rc = VfsRename(__Src__, __Dst__, __Flags__);
    if (rc == 0)
    {
        return 0;
    }
    rc = VfsCopy(__Src__, __Dst__, __Flags__);
    if (rc != 0)
    {
        return -1;
    }
    ReleaseMutex(&VfsLock);
    return VfsUnlink(__Src__);
}

int
VfsReadAll(const char* __Path__, void* __Buf__, long __BufLen__, long* __OutLen__)
{
    AcquireMutex(&VfsLock);
    File* F = VfsOpen(__Path__, VFlgRDONLY);
    if (!F)
    {
        return -1;
    }
    long total = 0;
    while (total < __BufLen__)
    {
        long r = VfsRead(F, (char*)__Buf__ + total, __BufLen__ - total);
        if (r < 0)
        {
            VfsClose(F);
            return -1;
        }
        if (r == 0)
        {
            break;
        }
        total += r;
    }
    if (__OutLen__)
    {
        *__OutLen__ = total;
    }
    VfsClose(F);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsWriteAll(const char* __Path__, const void* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    File* F = VfsOpen(__Path__, VFlgCREATE | VFlgWRONLY | VFlgTRUNC);
    if (!F)
    {
        return -1;
    }
    long total = 0;
    while (total < __Len__)
    {
        long w = VfsWrite(F, (const char*)__Buf__ + total, __Len__ - total);
        if (w <= 0)
        {
            VfsClose(F);
            return -1;
        }
        total += w;
    }
    VfsClose(F);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsMountTableEnumerate(char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    long off = 0;
    for (long I = 0; I < __MountCount__; I++)
    {
        const char* __Path = __Mounts__[I].Path;
        long        N      = (long)strlen(__Path);
        if (off + N + 2 >= __Len__)
        {
            break;
        }
        __builtin_memcpy(__Buf__ + off, __Path, (size_t)N);
        off += N;
        __Buf__[off++] = '\n';
    }
    if (off < __Len__)
    {
        __Buf__[off] = 0;
    }
    ReleaseMutex(&VfsLock);
    return (int)off;
}

int
VfsMountTableFind(const char* __Path__, char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    for (long I = 0; I < __MountCount__; I++)
    {
        if (strcmp(__Mounts__[I].Path, __Path__) == 0)
        {
            long N = (long)strlen(__Mounts__[I].Path);
            if (N >= __Len__)
            {
                return -1;
            }
            __builtin_memcpy(__Buf__, __Mounts__[I].Path, (size_t)(N + 1));
            return 0;
        }
    }
    ReleaseMutex(&VfsLock);
    return -1;
}

int
VfsNodePath(Vnode* __Node__, char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    (void)__Node__;
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    const char* __Path = "/";
    long        N      = (long)strlen(__Path);
    if (N >= __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Buf__, __Path, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsNodeName(Vnode* __Node__, char* __Buf__, long __Len__)
{
    AcquireMutex(&VfsLock);
    (void)__Node__;
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }
    const char* __Path = "";
    long        N      = (long)strlen(__Path);
    if (N >= __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Buf__, __Path, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsAllocName(char** __Out__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__Out__ || __Len__ <= 0)
    {
        return -1;
    }
    *__Out__ = (char*)KMalloc((size_t)__Len__);
    ReleaseMutex(&VfsLock);
    return *__Out__ ? 0 : -1;
}

int
VfsFreeName(char* __Name__)
{
    AcquireMutex(&VfsLock);
    if (!__Name__)
    {
        return -1;
    }
    KFree(__Name__);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsJoinPath(const char* __A__, const char* __B__, char* __Out__, long __Len__)
{
    AcquireMutex(&VfsLock);
    if (!__A__ || !__B__ || !__Out__ || __Len__ <= 0)
    {
        return -1;
    }
    long la   = (long)strlen(__A__);
    long lb   = (long)strlen(__B__);
    long need = la + 1 + lb + 1;
    if (need > __Len__)
    {
        return -1;
    }
    __builtin_memcpy(__Out__, __A__, (size_t)la);
    __Out__[la] = '/';
    __builtin_memcpy(__Out__ + la + 1, __B__, (size_t)lb);
    __Out__[la + 1 + lb] = 0;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsSetFlag(const char* __Path__, long __Flag__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    (void)__Flag__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsClearFlag(const char* __Path__, long __Flag__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    (void)__Flag__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetFlags(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsSyncAll(void)
{
    AcquireMutex(&VfsLock);
    for (long I = 0; I < __MountCount__; I++)
    {
        Superblock* Sb = __Mounts__[I].Sb;
        if (Sb && Sb->Ops && Sb->Ops->Sync)
        {
            Sb->Ops->Sync(Sb);
        }
    }
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsPruneCaches(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsRegisterDevNode(const char* __Path__, void* __Priv__, long __Flags__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Priv__)
    {
        return -1;
    }

    /* Ensure parent directory exists */
    char Buf[1024];
    VfsRealpath(__Path__, Buf, sizeof(Buf));
    const char* Name = strrchr(Buf, '/');
    if (!Name)
    {
        return -1;
    }
    long nlen = (long)strlen(Name + 1);

    char Parent[1024];
    long plen = (long)(Name - Buf);
    __builtin_memcpy(Parent, Buf, plen);
    Parent[plen] = 0;
    VfsMkpath(Parent, 0);

    /* Create vnode for device */
    Vnode* Node = (Vnode*)KMalloc(sizeof(Vnode));
    if (!Node)
    {
        return -1;
    }
    Node->Type   = VNodeDEV;
    Node->Ops    = (VnodeOps*)__Priv__; /* device ops table */
    Node->Sb     = __RootNode__->Sb;
    Node->Priv   = __Priv__;
    Node->Refcnt = 1;

    char* Dup = (char*)KMalloc(nlen + 1);
    __builtin_memcpy(Dup, Name + 1, nlen + 1);
    Dentry* De = __alloc_dentry__(Dup, __RootDe__, Node);
    if (!De)
    {
        KFree(Node);
        return -1;
    }

    PDebug("VFS: Registered devnode %s\n", __Path__);
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsUnregisterDevNode(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    (void)__Path__;
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsRegisterPseudoFs(const char* __Path__, Superblock* __Sb__)
{
    AcquireMutex(&VfsLock);
    if (!__Path__ || !__Sb__)
    {
        return -1;
    }
    if (__MountCount__ >= __MaxMounts__)
    {
        return -1;
    }
    long            N = (long)strlen(__Path__);
    __MountEntry__* M = &__Mounts__[__MountCount__++];
    M->Sb             = __Sb__;
    __builtin_memcpy(M->Path, __Path__, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

int
VfsUnregisterPseudoFs(const char* __Path__)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return VfsUnmount(__Path__);
}

int
VfsSetDefaultFs(const char* __Name__)
{
    AcquireMutex(&VfsLock);
    if (!__Name__)
    {
        return -1;
    }
    long N = (long)strlen(__Name__);
    if (N >= (long)sizeof(__DefaultFs__))
    {
        return -1;
    }
    __builtin_memcpy(__DefaultFs__, __Name__, (size_t)(N + 1));
    ReleaseMutex(&VfsLock);
    return 0;
}

const char*
VfsGetDefaultFs(void)
{
    return __DefaultFs__;
}

int
VfsSetMaxName(long __Len__)
{
    AcquireMutex(&VfsLock);
    if (__Len__ < 1)
    {
        return -1;
    }
    __MaxName__ = __Len__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetMaxName(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return __MaxName__;
}

int
VfsSetMaxPath(long __Len__)
{
    AcquireMutex(&VfsLock);
    if (__Len__ < 1)
    {
        return -1;
    }
    __MaxPath__ = __Len__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetMaxPath(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return __MaxPath__;
}

int
VfsSetDirCacheLimit(long __Val__)
{
    AcquireMutex(&VfsLock);
    __DirCacheLimit__ = __Val__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetDirCacheLimit(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return __DirCacheLimit__;
}

int
VfsSetFileCacheLimit(long __Val__)
{
    AcquireMutex(&VfsLock);
    __FileCacheLimit__ = __Val__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetFileCacheLimit(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return __FileCacheLimit__;
}

int
VfsSetIoBlockSize(long __Val__)
{
    AcquireMutex(&VfsLock);
    __IoBlockSize__ = __Val__;
    ReleaseMutex(&VfsLock);
    return 0;
}

long
VfsGetIoBlockSize(void)
{
    AcquireMutex(&VfsLock);
    ReleaseMutex(&VfsLock);
    return __IoBlockSize__;
}