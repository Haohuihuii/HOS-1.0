#pragma once

#include "../common/type.h"

// Page manager
PhysicalAddress AllocateOnePage(MachineMode mode);
PhysicalAddress AllocatePagesContinuously(MachineMode mode, u32 pageCount);
void FreeOnePage(PhysicalAddress address);

PhysicalPageNumber GetPPNFromAddressFloor(PhysicalAddress address);
PhysicalPageNumber GetPPNFromAddressCeil(PhysicalAddress address);
PhysicalAddress GetAddressFromPPN(PhysicalPageNumber ppn);

void InitMemoryManager();

// Memory Check
void MemoryCheckout(PhysicalAddress* ardCountAddress);

// Buddy
PhysicalAddress Malloc(Size size);
void Free(PhysicalAddress address);

// Stable
PhysicalAddress StableMalloc(Size size);
void StableFree(PhysicalAddress address);

// Memory Mapping
void SetRootPageTableAddr(PhysicalAddress addr);
PhysicalAddress GetRootPageTableAddr();
void FlushTLB(VirtualAddress addr);
void InitializeMemoryMapping();
void InitPageTableEntry(PageTableEntry* pte, u32 nextPPN, Boolean user);
void MapPage(VirtualAddress addr);