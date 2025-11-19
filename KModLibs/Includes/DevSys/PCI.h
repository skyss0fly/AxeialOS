#pragma once

#ifdef __Kernel__
#    include <AllTypes.h>
#else
#    include <EveryType.h>
#endif

typedef struct PciDevice
{
    uint8_t  Bus;
    uint8_t  Dev;
    uint8_t  Func;
    uint16_t VendorId;
    uint16_t DeviceId;
    uint8_t  ClassCode;
    uint8_t  SubClass;
    uint8_t  ProgIf;
    uint8_t  Revision;
    uint8_t  HeaderType;
    uint8_t  MultiFunction;
    uint8_t  PrimaryBus;
    uint8_t  SecondaryBus;
    uint8_t  SubordinateBus;
    uint16_t Command;
    uint16_t Status;
    uint32_t Bars[6];
    uint8_t  BarTypes[6];
    uint32_t BarSizes[6];
    uint8_t  InterruptLine;
    uint8_t  InterruptPin;
    uint8_t  CapPtr;
    uint8_t  MsiCapOffset;
    uint8_t  MsixCapOffset;
    uint8_t  PcieCapOffset;
    uint8_t  PmCapOffset;
} PciDevice;

typedef struct PciCtrlCtx
{
    PciDevice* Devices;
    uint32_t   DevCount;
    uint32_t   DevCap;
    uint8_t    UseEcam;
    uint64_t   EcamBase;
    uint32_t   EcamStrideBus;
    uint32_t   EcamStrideDev;
    uint32_t   EcamStrideFunc;
    uint32_t   EcamStrideOff;
} PciCtrlCtx;

typedef struct
{
    int Bus;
    int Dev;
    int Func;
} PciAddrReq;

typedef struct
{
    uint16_t VendorId;
    uint16_t DeviceId;
    int      Index;
} PciVendorReq;

typedef struct
{
    int   Bus;
    int   Dev;
    int   Func;
    int   Off;
    int   Len;
    void* Buf;
} PciCfgReq;

typedef struct
{
    int Bus;
    int Dev;
    int Func;
    int Enable;
} PciBmReq;

typedef struct
{
    int      Bus;
    int      Dev;
    int      Func;
    int      Index;
    uint32_t BarValue;
    uint32_t BarSize;
    uint8_t  BarType;
} PciBarReq;

typedef struct
{
    int Bus;
    int Dev;
    int Func;
    int Mode;
} PciIntReq;

typedef struct
{
    int Bus;
    int Dev;
    int Func;
    int DState;
} PciPowerReq;
