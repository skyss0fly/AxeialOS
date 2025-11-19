#include <AllTypes.h>
#include <KHeap.h>
#include <KrnPrintf.h>
#include <RamFs.h>
#include <String.h>

const VnodeOps __RamVfsOps__ = {
    .Open     = RamVfsOpen,     /**< Open file/directory handle */
    .Close    = RamVfsClose,    /**< Close file handle and free resources */
    .Read     = RamVfsRead,     /**< Read data from file */
    .Write    = RamVfsWrite,    /**< Write to file (not implemented - read-only) */
    .Lseek    = RamVfsLseek,    /**< Seek to position within file */
    .Ioctl    = RamVfsIoctl,    /**< I/O control operations (not implemented) */
    .Stat     = RamVfsStat,     /**< Get file/directory metadata */
    .Readdir  = RamVfsReaddir,  /**< Read directory entries */
    .Lookup   = RamVfsLookup,   /**< Lookup child by name in directory */
    .Create   = RamVfsCreate,   /**< Create new file in directory */
    .Unlink   = RamVfsUnlink,   /**< Remove file (not implemented) */
    .Mkdir    = RamVfsMkdir,    /**< Create new directory */
    .Rmdir    = RamVfsRmdir,    /**< Remove directory (not implemented) */
    .Symlink  = RamVfsSymlink,  /**< Create symlink (not implemented) */
    .Readlink = RamVfsReadlink, /**< Read symlink target (not implemented) */
    .Link     = RamVfsLink,     /**< Create hard link (not implemented) */
    .Rename   = RamVfsRename,   /**< Rename/move file (not implemented) */
    .Chmod    = RamVfsChmod,    /**< Change permissions (no-op) */
    .Chown    = RamVfsChown,    /**< Change ownership (no-op) */
    .Truncate = RamVfsTruncate, /**< Truncate file (not implemented) */
    .Sync     = RamVfsSync,     /**< Synchronize file (no-op) */
    .Map      = RamVfsMap,      /**< Memory map file (not implemented) */
    .Unmap    = RamVfsUnmap     /**< Unmap memory (not implemented) */
};

const SuperOps __RamVfsSuperOps__ = {
    .Sync    = RamVfsSuperSync,    /**< Sync filesystem to storage (no-op) */
    .StatFs  = RamVfsSuperStatFs,  /**< Get filesystem statistics */
    .Release = RamVfsSuperRelease, /**< Release superblock resources */
    .Umount  = RamVfsSuperUmount   /**< Unmount filesystem (no-op) */
};

int
RamFsRegister(void)
{
    static FsType __RamFsType__ = {.Name = "ramfs", .Mount = RamFsMountImpl, .Priv = 0};

    if (VfsRegisterFs(&__RamFsType__) != 0)
    {
        PError("RamFS: VfsRegisterFs failed\n");
        return -1;
    }

    PSuccess("RamFS: Registered with VFS\n");
    return 0;
}

Superblock*
RamFsMountImpl(const char* __Dev__, const char* __Opts__)
{
    (void)__Dev__;
    (void)__Opts__;

    if (!RamFS.Root)
    {
        PError("RamFS: Root not initialized\n");
        return 0;
    }

    Superblock* Sb = (Superblock*)KMalloc(sizeof(Superblock));
    if (!Sb)
    {
        PError("RamFS: Sb alloc failed\n");
        return 0;
    }

    Vnode* Root = (Vnode*)KMalloc(sizeof(Vnode));
    if (!Root)
    {
        PError("RamFS: Root vnode alloc failed\n");
        KFree(Sb);
        return 0;
    }

    RamVfsPrivNode* Priv = (RamVfsPrivNode*)KMalloc(sizeof(RamVfsPrivNode));
    if (!Priv)
    {
        PError("RamFS: Priv alloc failed\n");
        KFree(Root);
        KFree(Sb);
        return 0;
    }

    Priv->Node   = RamFS.Root;
    Root->Type   = VNodeDIR;
    Root->Ops    = &__RamVfsOps__;
    Root->Sb     = Sb;
    Root->Priv   = Priv;
    Root->Refcnt = 1;

    Sb->Type  = 0;
    Sb->Dev   = 0;
    Sb->Flags = 0;
    Sb->Root  = Root;
    Sb->Ops   = &__RamVfsSuperOps__;
    Sb->Priv  = 0;

    PDebug("RamFS: Superblock created\n");
    return Sb;
}

int
RamVfsOpen(Vnode* __Node__, File* __File__)
{
    if (!__Node__ || !__File__)
    {
        PError("RamFS: Open invalid args\n");
        return -1;
    }

    RamVfsPrivNode* PN = (RamVfsPrivNode*)__Node__->Priv;
    if (!PN || !PN->Node)
    {
        PError("RamFS: Open missing priv\n");
        return -1;
    }

    if (PN->Node->Type == RamFSNode_Directory)
    {
        __File__->Node   = __Node__;
        __File__->Offset = 0;
        __File__->Refcnt = 1;
        __File__->Priv   = 0;
        return 0;
    }

    if (PN->Node->Type == RamFSNode_File)
    {
        RamVfsPrivFile* PF = (RamVfsPrivFile*)KMalloc(sizeof(RamVfsPrivFile));
        if (!PF)
        {
            PError("RamFS: Open file priv alloc failed\n");
            return -1;
        }

        PF->Node   = PN->Node;
        PF->Offset = 0;

        __File__->Node   = __Node__;
        __File__->Offset = 0;
        __File__->Refcnt = 1;
        __File__->Priv   = PF;
        return 0;
    }

    PError("RamFS: Open unknown node type\n");
    return -1;
}

int
RamVfsClose(File* __File__)
{
    if (!__File__)
    {
        return -1;
    }

    if (__File__->Priv)
    {
        KFree(__File__->Priv);
        __File__->Priv = 0;
    }

    return 0;
}

long
RamVfsRead(File* __File__, void* __Buf__, long __Len__)
{
    if (!__File__ || !__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    RamVfsPrivFile* PF = (RamVfsPrivFile*)__File__->Priv;
    if (!PF || !PF->Node)
    {
        return -1;
    }

    size_t Got = RamFSRead(PF->Node, (size_t)PF->Offset, __Buf__, (size_t)__Len__);
    if (Got > 0)
    {
        PF->Offset += (long)Got;
        __File__->Offset += (long)Got;
        return (long)Got;
    }

    return 0;
}

long
RamVfsWrite(File* __File__, const void* __Buf__, long __Len__)
{
    (void)__File__;
    (void)__Buf__;
    (void)__Len__;
    return -1;
}

long
RamVfsLseek(File* __File__, long __Off__, int __Whence__)
{
    if (!__File__)
    {
        return -1;
    }

    RamVfsPrivFile* PF   = (RamVfsPrivFile*)__File__->Priv;
    long            Size = 0;

    if (PF && PF->Node && PF->Node->Type == RamFSNode_File)
    {
        Size = (long)PF->Node->Size;
    }

    long Base = 0;
    if (__Whence__ == VSeekSET)
    {
        Base = 0;
    }
    else if (__Whence__ == VSeekCUR)
    {
        Base = __File__->Offset;
    }
    else if (__Whence__ == VSeekEND)
    {
        Base = Size;
    }
    else
    {
        return -1;
    }

    long New = Base + __Off__;
    if (New < 0)
    {
        New = 0;
    }
    if (PF && PF->Node && New > (long)PF->Node->Size)
    {
        New = (long)PF->Node->Size;
    }

    __File__->Offset = New;
    if (PF)
    {
        PF->Offset = New;
    }
    return New;
}

int
RamVfsIoctl(File* __File__, unsigned long __Cmd__, void* __Arg__)
{
    (void)__File__;
    (void)__Cmd__;
    (void)__Arg__;
    return -1;
}

int
RamVfsStat(Vnode* __Node__, VfsStat* __Out__)
{
    if (!__Node__ || !__Out__)
    {
        return -1;
    }

    RamVfsPrivNode* PN = (RamVfsPrivNode*)__Node__->Priv;
    if (!PN || !PN->Node)
    {
        return -1;
    }

    __Out__->Ino        = (long)(uintptr_t)PN->Node;
    __Out__->Size       = (PN->Node->Type == RamFSNode_File) ? (long)PN->Node->Size : 0;
    __Out__->Blocks     = 0;
    __Out__->BlkSize    = 0;
    __Out__->Nlink      = 1;
    __Out__->Rdev       = 0;
    __Out__->Dev        = 0;
    __Out__->Flags      = 0;
    __Out__->Type       = __Node__->Type;
    __Out__->Perm.Mode  = 0;
    __Out__->Perm.Uid   = 0;
    __Out__->Perm.Gid   = 0;
    __Out__->Atime.Sec  = 0;
    __Out__->Atime.Nsec = 0;
    __Out__->Mtime.Sec  = 0;
    __Out__->Mtime.Nsec = 0;
    __Out__->Ctime.Sec  = 0;
    __Out__->Ctime.Nsec = 0;
    return 0;
}

long
RamVfsReaddir(Vnode* __Dir__, void* __Buf__, long __BufLen__)
{
    if (!__Dir__ || !__Buf__ || __BufLen__ <= 0)
    {
        return -1;
    }

    RamVfsPrivNode* PN = (RamVfsPrivNode*)__Dir__->Priv;
    if (!PN || !PN->Node)
    {
        return -1;
    }

    if (PN->Node->Type != RamFSNode_Directory)
    {
        return -1;
    }

    long       Max = __BufLen__;
    RamFSNode* Tmp[RamFSMaxChildren];
    uint32_t   Cnt = RamFSListChildren(PN->Node, Tmp, RamFSMaxChildren);

    long       Wrote = 0;
    VfsDirEnt* DE    = (VfsDirEnt*)__Buf__;

    for (uint32_t I = 0; I < Cnt && Wrote < Max; I++)
    {
        RamFSNode*  C  = Tmp[I];
        long        N  = 0;
        const char* Nm = C->Name;
        while (Nm[N] && N < 255)
        {
            DE[Wrote].Name[N] = Nm[N];
            N++;
        }
        DE[Wrote].Name[N] = 0;
        DE[Wrote].Type    = (C->Type == RamFSNode_Directory) ? VNodeDIR : VNodeFILE;
        DE[Wrote].Ino     = (long)(uintptr_t)C;
        Wrote++;
    }

    return Wrote; /* return count of entries */
}

Vnode*
RamVfsLookup(Vnode* __Dir__, const char* __Name__)
{
    if (!__Dir__ || !__Name__)
    {
        return 0;
    }

    RamVfsPrivNode* PN = (RamVfsPrivNode*)__Dir__->Priv;
    if (!PN || !PN->Node)
    {
        return 0;
    }

    if (PN->Node->Type != RamFSNode_Directory)
    {
        return 0;
    }

    char        Path[512];
    long        L  = 0;
    const char* DN = PN->Node->Name ? PN->Node->Name : "/";
    while (DN[L] && L < 511)
    {
        Path[L] = DN[L];
        L++;
    }
    if (L == 0 || Path[L - 1] != '/')
    {
        Path[L++] = '/';
    }
    long N = 0;
    while (__Name__[N] && L + N < 511)
    {
        Path[L + N] = __Name__[N];
        N++;
    }
    Path[L + N] = 0;

    RamFSNode* Child = RamFSLookup(RamFS.Root, Path);
    if (!Child)
    {
        return 0;
    }

    Vnode* V = (Vnode*)KMalloc(sizeof(Vnode));
    if (!V)
    {
        return 0;
    }

    RamVfsPrivNode* Priv = (RamVfsPrivNode*)KMalloc(sizeof(RamVfsPrivNode));
    if (!Priv)
    {
        KFree(V);
        return 0;
    }

    Priv->Node = Child;
    V->Type    = (Child->Type == RamFSNode_Directory) ? VNodeDIR : VNodeFILE;
    V->Ops     = &__RamVfsOps__;
    V->Sb      = __Dir__->Sb;
    V->Priv    = Priv;
    V->Refcnt  = 1;

    return V;
}

int
RamVfsCreate(Vnode* __Dir__, const char* __Name__, long __Flags__, VfsPerm __Perm__)
{
    (void)__Flags__;
    (void)__Perm__;
    if (!__Dir__ || !__Name__)
    {
        return -1;
    }
    RamVfsPrivNode* PN = (RamVfsPrivNode*)__Dir__->Priv;
    if (!PN || !PN->Node)
    {
        return -1;
    }
    if (PN->Node->Type != RamFSNode_Directory)
    {
        return -1;
    }

    char* Path = RamFSJoinPath(PN->Node->Name ? PN->Node->Name : "/", __Name__);
    if (!Path)
    {
        return -1;
    }

    RamFSNode* Leaf = RamFSAttachPath(RamFS.Root, Path, RamFSNode_File, 0, 0);
    KFree(Path);
    return Leaf ? 0 : -1;
}

int
RamVfsUnlink(Vnode* __Dir__, const char* __Name__)
{
    (void)__Dir__;
    (void)__Name__;
    return -1;
}

int
RamVfsMkdir(Vnode* __Dir__, const char* __Name__, VfsPerm __Perm__)
{
    (void)__Perm__;
    if (!__Dir__ || !__Name__)
    {
        return -1;
    }
    RamVfsPrivNode* PN = (RamVfsPrivNode*)__Dir__->Priv;
    if (!PN || !PN->Node)
    {
        return -1;
    }
    if (PN->Node->Type != RamFSNode_Directory)
    {
        return -1;
    }

    char* Path = RamFSJoinPath(PN->Node->Name ? PN->Node->Name : "/", __Name__);
    if (!Path)
    {
        return -1;
    }

    RamFSNode* Leaf = RamFSAttachPath(RamFS.Root, Path, RamFSNode_Directory, 0, 0);
    KFree(Path);

    return Leaf ? 0 : -1;
}

int
RamVfsRmdir(Vnode* __Dir__, const char* __Name__)
{
    (void)__Dir__;
    (void)__Name__;
    return -1;
}

int
RamVfsSymlink(Vnode* __Dir__, const char* __Name__, const char* __Target__, VfsPerm __Perm__)
{
    (void)__Dir__;
    (void)__Name__;
    (void)__Target__;
    (void)__Perm__;
    return -1;
}

int
RamVfsReadlink(Vnode* __Node__, VfsNameBuf* __Buf__)
{
    (void)__Node__;
    (void)__Buf__;
    return -1;
}

int
RamVfsLink(Vnode* __Dir__, Vnode* __Src__, const char* __Name__)
{
    (void)__Dir__;
    (void)__Src__;
    (void)__Name__;
    return -1;
}

int
RamVfsRename(Vnode*      __OldDir__,
             const char* __OldName__,
             Vnode*      __NewDir__,
             const char* __NewName__,
             long        __Flags__)
{
    (void)__OldDir__;
    (void)__OldName__;
    (void)__NewDir__;
    (void)__NewName__;
    (void)__Flags__;
    return -1;
}

int
RamVfsChmod(Vnode* __Node__, long __Mode__)
{
    (void)__Node__;
    (void)__Mode__;
    return 0;
}

int
RamVfsChown(Vnode* __Node__, long __Uid__, long __Gid__)
{
    (void)__Node__;
    (void)__Uid__;
    (void)__Gid__;
    return 0;
}

int
RamVfsTruncate(Vnode* __Node__, long __Len__)
{
    (void)__Node__;
    (void)__Len__;
    return -1;
}

int
RamVfsSync(Vnode* __Node__)
{
    (void)__Node__;
    return 0;
}

int
RamVfsMap(Vnode* __Node__, void** __Out__, long __Off__, long __Len__)
{
    (void)__Node__;
    (void)__Out__;
    (void)__Off__;
    (void)__Len__;
    return -1;
}

int
RamVfsUnmap(Vnode* __Node__, void* __Addr__, long __Len__)
{
    (void)__Node__;
    (void)__Addr__;
    (void)__Len__;
    return -1;
}

int
RamVfsSuperSync(Superblock* __Sb__)
{
    (void)__Sb__;
    return 0;
}

int
RamVfsSuperStatFs(Superblock* __Sb__, VfsStatFs* __Out__)
{
    if (!__Sb__ || !__Out__)
    {
        return -1;
    }
    __Out__->TypeId  = RamFSMagic;
    __Out__->Bsize   = 0;
    __Out__->Blocks  = 0;
    __Out__->Bfree   = 0;
    __Out__->Bavail  = 0;
    __Out__->Files   = 0;
    __Out__->Ffree   = 0;
    __Out__->Namelen = 255;
    __Out__->Flags   = 0;
    return 0;
}

void
RamVfsSuperRelease(Superblock* __Sb__)
{
    if (!__Sb__)
    {
        return;
    }
    if (__Sb__->Root)
    {
        RamVfsPrivNode* PN = (RamVfsPrivNode*)__Sb__->Root->Priv;
        if (PN)
        {
            KFree(PN);
        }
        KFree(__Sb__->Root);
        __Sb__->Root = 0;
    }
    KFree(__Sb__);
}

int
RamVfsSuperUmount(Superblock* __Sb__)
{
    (void)__Sb__;
    return 0;
}

int
BootMountRamFs(const void* __Initrd__, size_t __Len__)
{
    if (!__Initrd__ || __Len__ == 0)
    {
        PError("Boot: initrd invalid\n");
        return -1;
    }

    /* Parse the cpio archive into RamFS structures */
    if (!RamFSMount(__Initrd__, __Len__))
    {
        PError("Boot: RamFSMount failed\n");
        return -1;
    }

    /* Register RamFS driver with VFS */
    if (RamFsRegister() != 0)
    {
        PError("Boot: RamFsRegister failed\n");
        return -1;
    }

    /* Mount RamFS into VFS namespace at / */
    if (!VfsMount(0, "/", "ramfs", VMFlgNONE, 0))
    {
        PError("Boot: VfsMount ramfs failed\n");
        return -1;
    }

    PSuccess("Boot: RamFS mounted at /bootimg\n");
    return 0;
}
