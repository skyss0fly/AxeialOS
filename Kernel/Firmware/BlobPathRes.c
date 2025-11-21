#include <FirmBlobs.h>

/* Resolve descriptor to absolute path */
int
FirmResolvePath(const FirmwareDesc* __Desc__, char* __OutPath__, long __OutLen__)
{
    if (!__Desc__ || !__Desc__->Name || !__OutPath__ || __OutLen__ <= 0)
    {
        PError("FirmResolvePath: invalid args\n");
        return -1;
    }

    const char* Prefix = 0;
    switch (__Desc__->Origin)
    {
        case FirmOriginBootImg:
            {
                Prefix = FirmInitramfsPrefix;
                break;
            }
        case FirmOriginRootFS:
            {
                Prefix = FirmRootfsPrefix;
                break;
            }
        default:
            {
                PError("FirmResolvePath: bad origin\n");
                return -2;
            }
    }

    char tmp[512];
    if (VfsJoinPath(Prefix, __Desc__->Name, tmp, (long)sizeof(tmp)) != 0)
    {
        PError("FirmResolvePath: join failed\n");
        return -3;
    }

    if (VfsRealpath(tmp, __OutPath__, __OutLen__) != 0)
    {
        PError("FirmResolvePath: realpath failed '%s'\n", tmp);
        return -4;
    }

    return 0;
}
