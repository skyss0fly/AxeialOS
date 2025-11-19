#include <Dev.h>
#include <EveryType.h>
#include <Bus.h>
#include <Heaps.h>
#include <Logings.h>

/* -------------------------------------------------------------------------- */
/* Types and protocol glue                                                    */
/* -------------------------------------------------------------------------- */

typedef struct PciDevice
{
    int Bus;
    int Dev;
    int Func;

    unsigned short VendorId;
    unsigned short DeviceId;

    unsigned char ClassCode;
    unsigned char SubClass;
    unsigned char ProgIf;
    unsigned char Revision;

    unsigned char HeaderType; /* bit7 = multi-function */
} PciDevice;

typedef struct PciCtrlCtx
{
    PciDevice* Devices;
    long       DevCount;
    long       DevCap;
} PciCtrlCtx;

/* -------------------------------------------------------------------------- */
/* Low-level PCI config space I/O (legacy CF8/CFC)                            */
/* -------------------------------------------------------------------------- */

static inline unsigned int
PciMakeCfgAddr(int __Bus__, int __Dev__, int __Func__, int __Off__)
{
    return (unsigned int)(0x80000000u | (((unsigned int)__Bus__) << 16) |
                          (((unsigned int)__Dev__) << 11) | (((unsigned int)__Func__) << 8) |
                          ((unsigned int)(__Off__ & ~3)));
}

static inline void
PciOut32(unsigned short __Port__, unsigned int __Val__)
{
    __asm__ volatile("outl %0, %1" : : "a"(__Val__), "Nd"(__Port__));
}

static inline unsigned int
PciIn32(unsigned short __Port__)
{
    unsigned int __Ret__;
    __asm__ volatile("inl %1, %0" : "=a"(__Ret__) : "Nd"(__Port__));
    return __Ret__;
}

static unsigned int
PciCfgRead32(int __Bus__, int __Dev__, int __Func__, int __Off__)
{
    unsigned int __Addr__ = PciMakeCfgAddr(__Bus__, __Dev__, __Func__, __Off__);
    PciOut32(0xCF8, __Addr__);
    return PciIn32(0xCFC);
}

static void
PciCfgWrite32(int __Bus__, int __Dev__, int __Func__, int __Off__, unsigned int __Val__)
{
    unsigned int __Addr__ = PciMakeCfgAddr(__Bus__, __Dev__, __Func__, __Off__);
    PciOut32(0xCF8, __Addr__);
    PciOut32(0xCFC, __Val__);
}

static int
PciCfgRead(int __Bus__, int __Dev__, int __Func__, int __Off__, void* __Buf__, long __Len__)
{
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    long __Done__ = 0;
    while (__Done__ < __Len__)
    {
        int          __CurOff__ = __Off__ + (int)__Done__;
        unsigned int __Word__   = PciCfgRead32(__Bus__, __Dev__, __Func__, (__CurOff__ & ~3));
        int          __Shift__  = (__CurOff__ & 3) * 8;
        int          __Copy__ =
            (int)((__Len__ - __Done__) < (4 - (__CurOff__ & 3)) ? (__Len__ - __Done__)
                                                                : (4 - (__CurOff__ & 3)));

        unsigned char* __Dst__   = (unsigned char*)__Buf__ + __Done__;
        unsigned int   __Chunk__ = (__Word__ >> __Shift__);

        for (int __i__ = 0; __i__ < __Copy__; __i__++)
        {
            __Dst__[__i__] = (unsigned char)((__Chunk__ >> (8 * __i__)) & 0xFF);
        }

        __Done__ += __Copy__;
    }

    return (int)__Done__;
}

static int
PciCfgWrite(int __Bus__, int __Dev__, int __Func__, int __Off__, const void* __Buf__, long __Len__)
{
    if (!__Buf__ || __Len__ <= 0)
    {
        return -1;
    }

    long __Done__ = 0;
    while (__Done__ < __Len__)
    {
        int __CurOff__ = __Off__ + (int)__Done__;
        int __Copy__ =
            (int)((__Len__ - __Done__) < (4 - (__CurOff__ & 3)) ? (__Len__ - __Done__)
                                                                : (4 - (__CurOff__ & 3)));

        unsigned int __Word__ = PciCfgRead32(__Bus__, __Dev__, __Func__, (__CurOff__ & ~3));

        int                  __Shift__ = (__CurOff__ & 3) * 8;
        const unsigned char* __Src__   = (const unsigned char*)__Buf__ + __Done__;
        unsigned int         __Patch__ = 0;

        for (int __i__ = 0; __i__ < __Copy__; __i__++)
        {
            __Patch__ |= ((unsigned int)__Src__[__i__]) << (8 * __i__);
        }

        unsigned int __Mask__ = ((1u << (8 * __Copy__)) - 1u) << __Shift__;
        __Word__              = (__Word__ & ~__Mask__) | ((__Patch__ << __Shift__) & __Mask__);

        PciCfgWrite32(__Bus__, __Dev__, __Func__, (__CurOff__ & ~3), __Word__);
        __Done__ += __Copy__;
    }

    return (int)__Done__;
}

/* -------------------------------------------------------------------------- */
/* Enumeration                                                                */
/* -------------------------------------------------------------------------- */

static int
PciProbeFunc(PciCtrlCtx* __Ctx__, int __Bus__, int __Dev__, int __Func__)
{
    unsigned int   __VD__       = PciCfgRead32(__Bus__, __Dev__, __Func__, 0x00);
    unsigned short __VendorId__ = (unsigned short)(__VD__ & 0xFFFF);
    unsigned short __DeviceId__ = (unsigned short)((__VD__ >> 16) & 0xFFFF);

    if (__VendorId__ == 0xFFFF)
    {
        return 0; /* no device */
    }

    unsigned int  __ClassReg__  = PciCfgRead32(__Bus__, __Dev__, __Func__, 0x08);
    unsigned char __Revision__  = (unsigned char)(__ClassReg__ & 0xFF);
    unsigned char __ProgIf__    = (unsigned char)((__ClassReg__ >> 8) & 0xFF);
    unsigned char __SubClass__  = (unsigned char)((__ClassReg__ >> 16) & 0xFF);
    unsigned char __ClassCode__ = (unsigned char)((__ClassReg__ >> 24) & 0xFF);

    unsigned int  __Hdr__        = PciCfgRead32(__Bus__, __Dev__, __Func__, 0x0C);
    unsigned char __HeaderType__ = (unsigned char)((__Hdr__ >> 16) & 0xFF);

    if (__Ctx__->DevCount >= __Ctx__->DevCap)
    {
        long       __NewCap__ = (__Ctx__->DevCap == 0) ? 32 : (__Ctx__->DevCap * 2);
        PciDevice* __NewArr__ = (PciDevice*)KMalloc(sizeof(PciDevice) * __NewCap__);
        if (!__NewArr__)
        {
            PError("pci: KMalloc failed during grow\n");
            return -1;
        }

        /* copy old into new */
        for (long __i__ = 0; __i__ < __Ctx__->DevCount; __i__++)
        {
            __NewArr__[__i__] = __Ctx__->Devices[__i__];
        }

        if (__Ctx__->Devices)
        {
            KFree(__Ctx__->Devices);
        }

        __Ctx__->Devices = __NewArr__;
        __Ctx__->DevCap  = __NewCap__;
    }

    PciDevice* __D__  = &__Ctx__->Devices[__Ctx__->DevCount++];
    __D__->Bus        = __Bus__;
    __D__->Dev        = __Dev__;
    __D__->Func       = __Func__;
    __D__->VendorId   = __VendorId__;
    __D__->DeviceId   = __DeviceId__;
    __D__->ClassCode  = __ClassCode__;
    __D__->SubClass   = __SubClass__;
    __D__->ProgIf     = __ProgIf__;
    __D__->Revision   = __Revision__;
    __D__->HeaderType = __HeaderType__;

    PInfo("pci: %02x:%02x.%x vid=%04x did=%04x class=%02x/%02x prog=%02x rev=%02x\n",
          __Bus__,
          __Dev__,
          __Func__,
          __VendorId__,
          __DeviceId__,
          __ClassCode__,
          __SubClass__,
          __ProgIf__,
          __Revision__);

    return 1;
}

static int
PciEnumerate(PciCtrlCtx* __Ctx__)
{
    __Ctx__->DevCount = 0;

    for (int __Bus__ = 0; __Bus__ < 256; __Bus__++)
    {
        for (int __Dev__ = 0; __Dev__ < 32; __Dev__++)
        {
            /* probe function 0 first to check multi-function */
            int           __Found0__     = PciProbeFunc(__Ctx__, __Bus__, __Dev__, 0);
            unsigned int  __Hdr__        = PciCfgRead32(__Bus__, __Dev__, 0, 0x0C);
            unsigned char __HeaderType__ = (unsigned char)((__Hdr__ >> 16) & 0xFF);

            if (__HeaderType__ & 0x80)
            {
                for (int __Func__ = 1; __Func__ < 8; __Func__++)
                {
                    PciProbeFunc(__Ctx__, __Bus__, __Dev__, __Func__);
                }
            }
            else
            {
                (void)__Found0__;
            }
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Ioctl request structs                                                      */
/* -------------------------------------------------------------------------- */

typedef struct
{
    int Bus;
    int Dev;
    int Func;
} PciAddrReq;

typedef struct
{
    unsigned short VendorId;
    unsigned short DeviceId;
    int            Index; /* nth match */
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
    int Enable; /* 0/1 */
} PciBmReq;

/* -------------------------------------------------------------------------- */
/* Char driver ops                                                            */
/* -------------------------------------------------------------------------- */

static PciCtrlCtx PciCtx;
static CharBus*   PciBus = NULL;

static int
PciOpen(void* __CtrlCtx__)
{
    PInfo("pci: open (ctx=%p)\n", __CtrlCtx__);
    return 0;
}

static int
PciClose(void* __CtrlCtx__)
{
    PInfo("pci: close (ctx=%p)\n", __CtrlCtx__);
    return 0;
}

static long
PciRead(void* __CtrlCtx__, void* __Buf__, long __Len__)
{
    (void)__CtrlCtx__;
    (void)__Buf__;
    (void)__Len__;
    /* No streaming read semantics for /dev/pci; use ioctl */
    return 0;
}

static long
PciWrite(void* __CtrlCtx__, const void* __Buf__, long __Len__)
{
    (void)__CtrlCtx__;
    (void)__Buf__;
    /* No streaming write semantics for /dev/pci; use ioctl */
    return __Len__;
}

static int
PciFindByBdf(PciCtrlCtx* __Ctx__, int __Bus__, int __Dev__, int __Func__, PciDevice* __Out__)
{
    for (long __i__ = 0; __i__ < __Ctx__->DevCount; __i__++)
    {
        PciDevice* __D__ = &__Ctx__->Devices[__i__];
        if (__D__->Bus == __Bus__ && __D__->Dev == __Dev__ && __D__->Func == __Func__)
        {
            if (__Out__)
            {
                *__Out__ = *__D__;
            }
            return (int)__i__;
        }
    }
    return -1;
}

static int
PciFindByVendor(PciCtrlCtx*    __Ctx__,
                unsigned short __Vid__,
                unsigned short __Did__,
                int            __Index__,
                PciDevice*     __Out__)
{
    int __Seen__ = 0;
    for (long __i__ = 0; __i__ < __Ctx__->DevCount; __i__++)
    {
        PciDevice* __D__ = &__Ctx__->Devices[__i__];
        if (__D__->VendorId == __Vid__ && __D__->DeviceId == __Did__)
        {
            if (__Seen__ == __Index__)
            {
                if (__Out__)
                {
                    *__Out__ = *__D__;
                }
                return (int)__i__;
            }
            __Seen__++;
        }
    }
    return -1;
}

static int
PciIoctl(void* __CtrlCtx__, unsigned long __Cmd__, void* __Arg__)
{
    (void)__CtrlCtx__;

    switch (__Cmd__)
    {
        case PCI_GET_COUNT:
            {
                long* __OutCount__ = (long*)__Arg__;
                if (!__OutCount__)
                {
                    return -1;
                }
                *__OutCount__ = PciCtx.DevCount;
                return 0;
            }

        case PCI_GET_DEVICE:
            {
                PciAddrReq* __Req__ = (PciAddrReq*)__Arg__;
                if (!__Req__)
                {
                    return -1;
                }
                PciDevice* __Out__ =
                    (PciDevice*)(__Req__); /* caller provides space beyond Req or separate buffer */
                PciDevice __Tmp__;
                int       __Idx__ =
                    PciFindByBdf(&PciCtx, __Req__->Bus, __Req__->Dev, __Req__->Func, &__Tmp__);
                if (__Idx__ < 0)
                {
                    return -1;
                }
                *__Out__ = __Tmp__;
                return 0;
            }

        case PCI_GET_VD:
            {
                PciVendorReq* __Req__ = (PciVendorReq*)__Arg__;
                if (!__Req__)
                {
                    return -1;
                }
                PciDevice __Tmp__;
                int       __Idx__ = PciFindByVendor(
                    &PciCtx, __Req__->VendorId, __Req__->DeviceId, __Req__->Index, &__Tmp__);
                if (__Idx__ < 0)
                {
                    return -1;
                }
                /* Write back the device into the same buffer (or define separate out ptr) */
                *((PciDevice*)__Arg__) = __Tmp__;
                return 0;
            }

        case PCI_ENABLE_BM:
            {
                PciBmReq* __Req__ = (PciBmReq*)__Arg__;
                if (!__Req__)
                {
                    return -1;
                }
                /* Toggle Bus Master bit in Command register (offset 0x04, bit 2) */
                unsigned int __CmdReg__ =
                    PciCfgRead32(__Req__->Bus, __Req__->Dev, __Req__->Func, 0x04);
                unsigned short __Cmd16__ = (unsigned short)(__CmdReg__ & 0xFFFF);
                if (__Req__->Enable)
                {
                    __Cmd16__ |= (1u << 2);
                }
                else
                {
                    __Cmd16__ &= ~(1u << 2);
                }
                unsigned int __NewReg__ = (__CmdReg__ & 0xFFFF0000u) | __Cmd16__;
                PciCfgWrite32(__Req__->Bus, __Req__->Dev, __Req__->Func, 0x04, __NewReg__);
                return 0;
            }

        case PCI_RESYNC_CACHE:
            {
                /* Re-enumerate devices to reflect hot-plug or config changes */
                PciEnumerate(&PciCtx);
                return 0;
            }

        case PCI_READ_CFG:
            {
                PciCfgReq* __Req__ = (PciCfgReq*)__Arg__;
                if (!__Req__ || !__Req__->Buf || __Req__->Len <= 0)
                {
                    return -1;
                }
                int __Rc__ = PciCfgRead(__Req__->Bus,
                                        __Req__->Dev,
                                        __Req__->Func,
                                        __Req__->Off,
                                        __Req__->Buf,
                                        __Req__->Len);
                return (__Rc__ < 0) ? -1 : 0;
            }

        case PCI_WRITE_CFG:
            {
                PciCfgReq* __Req__ = (PciCfgReq*)__Arg__;
                if (!__Req__ || !__Req__->Buf || __Req__->Len <= 0)
                {
                    return -1;
                }
                int __Rc__ = PciCfgWrite(__Req__->Bus,
                                         __Req__->Dev,
                                         __Req__->Func,
                                         __Req__->Off,
                                         __Req__->Buf,
                                         __Req__->Len);
                return (__Rc__ < 0) ? -1 : 0;
            }

        default:
            {
                return -1;
            }
    }
}

/* -------------------------------------------------------------------------- */
/* Module wiring                                                              */
/* -------------------------------------------------------------------------- */

static int
PciInitContext(PciCtrlCtx* __Ctx__)
{
    __Ctx__->Devices  = NULL;
    __Ctx__->DevCount = 0;
    __Ctx__->DevCap   = 0;

    /* initial capacity */
    long __InitCap__ = 64;
    __Ctx__->Devices = (PciDevice*)KMalloc(sizeof(PciDevice) * __InitCap__);
    if (!__Ctx__->Devices)
    {
        PError("pci: KMalloc failed for device list\n");
        return -1;
    }
    __Ctx__->DevCap = __InitCap__;

    /* first enumeration */
    PciEnumerate(__Ctx__);
    return 0;
}

static void
PciFreeContext(PciCtrlCtx* __Ctx__)
{
    if (__Ctx__->Devices)
    {
        KFree(__Ctx__->Devices);
        __Ctx__->Devices = NULL;
    }
    __Ctx__->DevCount = 0;
    __Ctx__->DevCap   = 0;
}

static int
module_init(void)
{
    if (PciInitContext(&PciCtx) != 0)
    {
        return -1;
    }

    PciBus = (CharBus*)KMalloc(sizeof(CharBus));
    if (!PciBus)
    {
        PciFreeContext(&PciCtx);
        PError("pci: KMalloc failed for CharBus\n");
        return -1;
    }

    PciBus->Name      = "pci";
    PciBus->CtrlCtx   = &PciCtx;
    PciBus->Ops.Open  = PciOpen;
    PciBus->Ops.Close = PciClose;
    PciBus->Ops.Read  = PciRead;
    PciBus->Ops.Write = PciWrite;
    PciBus->Ops.Ioctl = PciIoctl;

    int __Rc__ = CharRegisterBus(PciBus, 12, 0);
    if (__Rc__ != 0)
    {
        PError("pci: CharRegisterBus failed (%d)\n", __Rc__);
        KFree(PciBus);
        PciBus = NULL;
        PciFreeContext(&PciCtx);
        return __Rc__;
    }

    PSuccess("pci: /dev/pci ready (%ld devices)\n", PciCtx.DevCount);
    return 0;
}

static int
module_exit(void)
{
    PInfo("pci: module_exit\n");
    if (PciBus)
    {
        KFree(PciBus);
        PciBus = NULL;
    }
    PciFreeContext(&PciCtx);
    return 0;
}