#include <IDT.h>
#include <Timer.h>

void
IrqHandler(InterruptFrame* __Frame__)
{
    /*Handle APIC Timer on IRQ0 - Vector 32 is the first IRQ (IRQ0)*/
    if (__Frame__->IntNo == 32)
    {
        TimerHandler(__Frame__); /*Dispatch to timer subsystem*/
        return;                  /*APIC handles its own EOI, no need for PIC EOI*/
    }

    /*Legacy PIC interrupts - Handle EOI (End of Interrupt) signaling*/
    /*If interrupt came from slave PIC (vectors 40-47), send EOI to slave first*/
    if (__Frame__->IntNo >= 40)
    {
        __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0xA0));
    }
    /*Always send EOI to master PIC to acknowledge the interrupt*/
    __asm__ volatile("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
}
