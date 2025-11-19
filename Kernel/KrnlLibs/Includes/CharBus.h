#pragma once

#include <AllTypes.h>
#include <DevFS.h>
#include <KHeap.h>
#include <KrnPrintf.h>
#include <String.h>
#include <VFS.h>

typedef enum CharIoProtocol
{
    /* 32-bit universal opcodes: [31:24]=Domain, [23:16]=Category, [15:0]=Op */

    /* Domain: Generic (0x01) */
    GenericPing       = 0x01010001,
    GenericGetVersion = 0x01010002,
    GenericGetCaps    = 0x01010003,

    /* Domain: Bus (0x02) — applies to PCI, USB, AHCI, NET, etc. */
    BusGetCount  = 0x02010001,
    BusGetInfo   = 0x02010002,
    BusEnumerate = 0x02010003,
    BusRescan    = 0x02010004,
    BusReset     = 0x02010005,

    /* Domain: Device (0x03) — target by index or address */
    DeviceGetInfo      = 0x03010001,
    DeviceGetByAddress = 0x03010002,
    DeviceGetByVendor  = 0x03010003,
    DeviceEnable       = 0x03010004,
    DeviceDisable      = 0x03010005,
    DeviceReset        = 0x03010006,

    /* Domain: Config (0x04) — universal config space or control plane */
    ConfigRead          = 0x04010001,
    ConfigWrite         = 0x04010002,
    ConfigMapRegion     = 0x04010003,
    ConfigUnmapRegion   = 0x04010004,
    ConfigGetAddressing = 0x04010005,

    /* Domain: Power (0x05) — D-states or link states across buses */
    PowerGetState = 0x05010001,
    PowerSetState = 0x05010002,

    /* Domain: DMA (0x06) — coherent buffers and BM toggles */
    DmaEnableBusMaster  = 0x06010001,
    DmaDisableBusMaster = 0x06010002,
    DmaMapBuffer        = 0x06010003,
    DmaUnmapBuffer      = 0x06010004,

    /* Domain: Interrupt (0x07) — line/MSI/MSI-X in a universal shape */
    IntGetMode = 0x07010001,
    IntSetMode = 0x07010002,
    IntEnable  = 0x07010003,
    IntDisable = 0x07010004,

    /* Domain: Link (0x08) — topology, bandwidth, link training */
    LinkGetTopology  = 0x08010001,
    LinkGetBandwidth = 0x08010002,
    LinkTrain        = 0x08010003,

    /* Domain: Network (0x09) — universal net ops mapped to devices */
    NetGetIfCount = 0x09010001,
    NetGetIfInfo  = 0x09010002,
    NetSetMac     = 0x09010003,
    NetUp         = 0x09010004,
    NetDown       = 0x09010005,
    NetTx         = 0x09010006,
    NetRx         = 0x09010007,

    /* Domain: Usb (0x0A) — still usable via universal device addressing */
    UsbGetDevCount  = 0x0A010001,
    UsbGetDevInfo   = 0x0A010002,
    UsbCtrlTransfer = 0x0A010003,
    UsbBulkTransfer = 0x0A010004,

    /* Domain: Storage (0x0B) — HBA/port-level control (AHCI/SCSI) */
    StorageGetAdapters = 0x0B010001,
    StorageGetInfo     = 0x0B010002,
    StorageResetBus    = 0x0B010003,

    /* Domain: Tty (0x0C) — serial-like endpoints across buses */
    TtySetBaud   = 0x0C010001,
    TtySetMode   = 0x0C010002,
    TtyGetStatus = 0x0C010003,
    TtyFlush     = 0x0C010004,

    /* Domain: Sensor (0x0D) — hwmon-style sensors via any bus */
    SensorGetCount  = 0x0D010001,
    SensorGetInfo   = 0x0D010002,
    SensorReadValue = 0x0D010003
} CharIoProtocol;

typedef struct CharBus
{
    const char* Name;    /* e.g., "pci", "ttyS0", "hid0" */
    void*       CtrlCtx; /* driver/controller-private context */
    CharDevOps  Ops;     /* driver ops (filled by caller) */

} CharBus;

int CharRegisterBus(CharBus* __Bus__, int __Major__, int __Minor__);
int CharMakeName(char* __Out__, long __Cap__, const char* __Prefix__, long __Index__);
int CharMakeSubName(char* __Out__, long __Cap__, const char* __Base__, long __SubIndex__);

KEXPORT(CharRegisterBus);
KEXPORT(CharMakeName);
KEXPORT(CharMakeSubName);