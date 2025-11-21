#pragma once

#include <Dev.h>
#include <EveryType.h>

typedef struct FirmwareBlob
{
    const unsigned char* Data; /**< Pointer to blob contents (read‑only) */
    long                 Size; /**< Size of the blob in bytes */
} FirmwareBlob;

typedef enum FirmwareOrigin
{
    FirmOriginNONE,
    FirmOriginBootImg, /**< Built into initramfs (/firmblobs) */
    FirmOriginRootFS   /**< Loaded from rootfs (/lib/firmware) */
} FirmwareOrigin;

typedef struct FirmwareDesc
{
    const char*    Name;   /**< Logical blob name (no path) */
    FirmwareOrigin Origin; /**< Source hint */
} FirmwareDesc;

typedef struct FirmwareHandle
{
    FirmwareBlob       Blob;    /**< The immutable payload */
    FirmwareDesc       Desc;    /**< Descriptor used for retrieval */
    const char*        Mime;    /**< Optional MIME/format hint */
    const char*        Vendor;  /**< Optional vendor tag */
    const char*        Version; /**< Optional version string */
    const DeviceEntry* Dev;     /**< Optional device context */
} FirmwareHandle;

#define FirmInitramfsPrefix "/firmblobs"
#define FirmRootfsPrefix    "/lib/firmware"

int FirmRequest(FirmwareHandle**    __OutHandle__,
                const FirmwareDesc* __Desc__,
                const DeviceEntry*  __Dev__);
int FirmRelease(FirmwareHandle* __Handle__);
int FirmResolvePath(const FirmwareDesc* __Desc__, char* __OutPath__, long __OutLen__);
const unsigned char* FirmData(const FirmwareHandle* __Handle__);
long                 FirmSize(const FirmwareHandle* __Handle__);