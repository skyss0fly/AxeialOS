#include <AllTypes.h>
#include <FirmBlobs.h>
#include <KHeap.h>
#include <String.h>
#include <VFS.h>

/* Request firmware blob */
int
FirmRequest(FirmwareHandle**    __OutHandle__,
            const FirmwareDesc* __Desc__,
            const DeviceEntry*  __Dev__)
{
    if (!__OutHandle__ || !__Desc__)
    {
        PError("FirmRequest: invalid args\n");
        return -1;
    }

    FirmwareHandle* H = (FirmwareHandle*)KMalloc(sizeof(FirmwareHandle));
    if (!H)
    {
        PError("FirmRequest: alloc handle failed\n");
        return -3;
    }
    memset(H, 0, sizeof(FirmwareHandle));
    H->Desc        = *__Desc__;
    H->Dev         = __Dev__;
    *__OutHandle__ = H;

    char PathBuf[512];
    if (FirmResolvePath(__Desc__, PathBuf, (long)sizeof(PathBuf)) != 0)
    {
        KFree(H);
        *__OutHandle__ = 0;
        return -4;
    }

    File* F = VfsOpen(PathBuf, VFlgRDONLY);
    if (!F)
    {
        PError("FirmRequest: open failed '%s'\n", PathBuf);
        KFree(H);
        *__OutHandle__ = 0;
        return -69;
    }

    VfsStat St;
    if (VfsFstats(F, &St) != 0 || St.Size <= 0)
    {
        PError("FirmRequest: fstats failed '%s'\n", PathBuf);
        VfsClose(F);
        KFree(H);
        *__OutHandle__ = 0;
        return -67;
    }

    unsigned char* Buf = (unsigned char*)KMalloc((size_t)St.Size);
    if (!Buf)
    {
        PError("FirmRequest: alloc payload failed size=%ld\n", St.Size);
        VfsClose(F);
        KFree(H);
        *__OutHandle__ = 0;
        return -7;
    }

    long Read   = 0;
    int  RcRead = VfsReadAll(PathBuf, Buf, St.Size, &Read);
    VfsClose(F);

    if (RcRead != 0 || Read != St.Size)
    {
        PError("FirmRequest: read failed rc=%d read=%ld exp=%ld\n", RcRead, Read, St.Size);
        KFree(Buf);
        KFree(H);
        *__OutHandle__ = 0;
        return -8;
    }

    H->Blob.Data = Buf;
    H->Blob.Size = Read;

    PInfo("FirmRequest: loaded '%s' size=%ld\n", PathBuf, Read);
    return 0;
}

int
FirmRelease(FirmwareHandle* __Handle__)
{
    if (!__Handle__)
    {
        return 0;
    }
    if (__Handle__->Blob.Data)
    {
        KFree((void*)__Handle__->Blob.Data);
    }
    KFree(__Handle__);
    return 0;
}