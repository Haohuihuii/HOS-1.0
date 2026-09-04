#include "mod.h"
#include "../memory/mod.h"

static VirtualAddress getPageFaultAddress() {
    VirtualAddress addr;
    asm volatile ("movl %%cr2, %0" : "=r"(addr));
    return addr;
}

static void pageFaultHandler(u32 vector, u32 errorCode) {
    Assert(vector == 0x0E);

    VirtualAddress addr = getPageFaultAddress();
    /*Printf(
    "Page Fault: addr=0x%x error=0x%x\n",
    addr,
    errorCode
    );*/
    if (errorCode & 0x1) {
        Panic("Page protection fault");
    }

    if (addr == 0) {
        Panic("Null pointer exception");
    }

    if (addr < 0x400000) {
        Panic("System terminated");
    }

    MapPage(addr);
}

void InitializePageFaultHandler() {
    SetInterruptHandler(0x0E, pageFaultHandler);
}