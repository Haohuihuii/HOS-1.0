#include "mod.h"

/// @brief 内核页表的页目录（根页表）PPN
static u32 KernelRootPPN;


void InitPageTableEntry(PageTableEntry* pte, u32 nextPPN, Boolean user);
void EnablePaging();
void DisablePaging();
static PageTableEntry* findPTE(VirtualAddress addr);//只find
static PageTableEntry* findPTECreate(VirtualAddress addr);//find 缺少时自动创建


void SetRootPageTableAddr(PhysicalAddress addr) {
    Assert(addr % PageSize == 0);
    asm volatile ("movl %0, %%cr3":: "r"(addr));
}

PhysicalAddress GetRootPageTableAddr() {
    u32 cr3;
    asm volatile ("movl %%cr3, %0" : "=r"(cr3));
    //mov addr, cr3   读取 CR3（Control Register 3，3号控制寄存器）
    return cr3;
}

void FlushTLB(VirtualAddress addr) {
    addr = GetAddressFromPPN(GetPPNFromAddressFloor(addr)); // 按页对齐
    asm volatile ("invlpg (%0)":: "r"(addr));
}

void InitializeMemoryMapping() {
    KernelRootPPN = GetPPNFromAddressFloor(
        AllocateOnePage(KernelMode)
        //1024 * 4B = 4KB   KernelRootPPN是页目录所在物理页编号
    );

    //   让 pte 指向 Kernel 页目录的第一个 PDE。
    PageTableEntry* pte = (PageTableEntry*)GetAddressFromPPN(KernelRootPPN);
    PhysicalAddress kernelSecondPageTable = AllocateOnePage(KernelMode);
    InitPageTableEntry(pte, GetPPNFromAddressFloor(kernelSecondPageTable), TRUE);

    // 初始化内核二级页表，映射内核的所有物理地址(0-4MB)
    pte = (PageTableEntry*)kernelSecondPageTable;
    for (u32 i = 0; i < 1024; i++) {
        if (i == 0) continue; // 我们认为0-0x1000的虚拟地址是空指针，不映射
        InitPageTableEntry(pte + i, i, TRUE);
    }

    // 告知CPU根页表的物理地址
    SetRootPageTableAddr(GetAddressFromPPN(KernelRootPPN));
    // 通知CPU开启分页功能
    EnablePaging();
}

// static methods implementation
void MapPage(VirtualAddress addr) {
    DisablePaging();
    findPTECreate(addr);
    EnablePaging();
    FlushTLB(addr);
}

void InitPageTableEntry(PageTableEntry* pte, u32 nextPPN, Boolean user) {
    pte->Present = 1;
    pte->Write = 1;
    pte->User = user;
    pte->PageWriteThrough = 0;
    pte->PageCacheDisable = 0;
    pte->Access = 0;
    pte->Dirty = 0;
    pte->Pat = 0;
    pte->Global = 0;
    pte->Reversed = 0;
    pte->NextPPN = nextPPN;
}

void EnablePaging() {
    asm volatile ("movl %cr0, %eax");
    asm volatile ("orl $0x80000000, %eax");
    asm volatile ("movl %eax, %cr0");
    //开启CR0的第31位 分页使能位
}

void DisablePaging() {
    asm volatile ("movl %cr0, %eax");
    asm volatile ("andl $0x7FFFFFFF, %eax");
    asm volatile ("movl %eax, %cr0");
}

static PageTableEntry* findPTE(VirtualAddress addr) {
    PageTableEntry* rootPTE =
        (PageTableEntry*)GetRootPageTableAddr();

    u32 firstIndex = addr >> 22;
    u32 secondIndex = (addr >> 12) & 0x3ff;

    PageTableEntry* pte = rootPTE + firstIndex;

    if (pte->Present == 0) {
        return NULL;
    }

    return (PageTableEntry*)GetAddressFromPPN(pte->NextPPN)
        + secondIndex;
}

static PageTableEntry* findPTECreate(VirtualAddress addr) {
    PageTableEntry* rootPTE =
        (PageTableEntry*)GetRootPageTableAddr();

    u32 firstIndex = addr >> 22;
    u32 secondIndex = (addr >> 12) & 0x3ff;

    PageTableEntry* pte = rootPTE + firstIndex;

    if (pte->Present == 0) {
        PhysicalAddress secondPageTable =
            AllocateOnePage(UserMode);

        InitPageTableEntry(
            pte,
            GetPPNFromAddressFloor(secondPageTable),
            TRUE
        );
    }

    pte =
        (PageTableEntry*)GetAddressFromPPN(pte->NextPPN)
        + secondIndex;

    if (pte->Present == 0) {
        PhysicalAddress page =
            AllocateOnePage(UserMode);

        InitPageTableEntry(
            pte,
            GetPPNFromAddressFloor(page),
            TRUE
        );
    }

    return pte;
}
