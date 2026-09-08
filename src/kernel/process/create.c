#include "mod.h"
#include "../lib/method.h"

extern void RestoreContext();

/// @brief 让栈指针指向Interrupt Context方便返回用户态
static void restore();
static void copyPageTableRecursion(u32 childRootPPN, u32 parentRootPPN);


void CreateKernelProcess(void *entry) {
    PCB *process = (PCB *)Malloc(sizeof(PCB));
    process->ID = AllocatePID();
    process->Status = PROCESS_STATE_RUNNABLE;
    u32 stack = AllocateOnePage(KernelMode) + PageSize;
    process->Type = PROCESS_TYPE_KERNEL;
    // 复制内核态常驻页表内容，前4MB的等值映射
    u32 kernelProcessRootPPN = GetPPNFromAddressFloor(AllocateOnePage(KernelMode));
    process->RootPPN = kernelProcessRootPPN;
    MemoryCopy(
        GetAddressFromPPN(kernelProcessRootPPN),
        GetRootPageTableAddr(),
        PageSize
    );

    stack -= sizeof(SwitchContext);
    SwitchContext *context = (SwitchContext *)stack;
    context->EIP = (u32)entry;
    context->EBP = 0;
    context->ESI = 0;
    context->EDI = 0;
    context->EBX = 0;
    process->KernelStackPointer = (PhysicalAddress *)stack;

    AddProcess(process);
}

void CreateUserProcess(void *entry) {
    PCB *process = (PCB *)Malloc(sizeof(PCB));
    process->ID = AllocatePID();
    process->Status = PROCESS_STATE_RUNNABLE;
    u32 stack = AllocateOnePage(KernelMode) + PageSize;
    process->Type = PROCESS_TYPE_USER;
    // 构造用户专属页表，复制内核页表的前4MB等页号映射
    u32 userRootPPN = GetPPNFromAddressFloor(AllocateOnePage(KernelMode));
    process->RootPPN = userRootPPN;
    MemoryCopy(
        GetAddressFromPPN(userRootPPN),
        GetRootPageTableAddr(),
        PageSize
    );

    stack -= sizeof(InterruptContext);
    InterruptContext *trapContext = (InterruptContext *)stack;
    trapContext->Vector = 0x80;
    trapContext->ErrCode = 0x88888888;
    trapContext->EDI = 0;
    trapContext->ESI = 0;
    trapContext->EBP = 0;
    trapContext->ESP = 0;
    trapContext->EBX = 0;
    trapContext->EDX = 0;
    trapContext->ECX = 0;
    trapContext->EAX = 0;
    trapContext->GS = UserDataSegmentSelector;
    trapContext->FS = UserDataSegmentSelector;
    trapContext->ES = UserDataSegmentSelector;
    trapContext->DS = UserDataSegmentSelector;
    trapContext->EIP = (u32)entry;
    trapContext->CS = UserCodeSegmentSelector;
    trapContext->PSW = 0x202; // 0000 0010 0000 0010
    trapContext->SS3 = UserDataSegmentSelector;
    trapContext->ESP3 = UserStackTop;

    stack -= sizeof(SwitchContext);
    SwitchContext *context = (SwitchContext *)stack;
    context->EIP = restore;
    context->EBP = 0;
    context->ESI = 0;
    context->EDI = 0;
    context->EBX = 0;
    process->KernelStackPointer = (PhysicalAddress *)stack;

    AddProcess(process);
}

PID ForkProcess() {
    PCB *parent = GetCurrentProcess();
    Assert(parent->Type == PROCESS_TYPE_USER);

    PCB *child = (PCB *)Malloc(sizeof(PCB));
    child->ID = AllocatePID();
    child->ParentID = parent->ID;
    child->Status = parent->Status;
    child->Type = parent->Type;

    // 复制内核栈
    u32 stack = AllocateOnePage(KernelMode) + PageSize;
    MemoryCopy(stack - PageSize, GetAddressFromPPN(GetPPNFromAddressFloor(parent->KernelStackPointer)), PageSize);
    // 复制页表
    u32 childRootPPN = GetPPNFromAddressFloor(AllocateOnePage(KernelMode));
    copyPageTableRecursion(childRootPPN, parent->RootPPN);
    child->RootPPN = childRootPPN;

    stack -= sizeof(InterruptContext);
    InterruptContext *ctx = (InterruptContext *)stack;
    ctx->EAX = 0; // 子进程应该返回0

    stack -= sizeof(SwitchContext);
    SwitchContext *sctx = (SwitchContext *)stack;
    sctx->EIP = restore;
    sctx->EBP = sctx->ESI = sctx->EDI = sctx->EBX = 0;

    child->KernelStackPointer = (PhysicalAddress *)stack;

    AddProcess(child);
    Schedule();
    return child->ID;
}

static void restore() {
    PCB *current = GetCurrentProcess();
    u32 stack = ((u32)current->KernelStackPointer + PageSize - 1) / PageSize * PageSize;
    stack -= sizeof(InterruptContext);
    asm volatile ("movl %0, %%esp" : : "m"(stack));
    asm volatile ("jmp RestoreContext");
}

static void copyPageTableRecursion(u32 childRootPPN, u32 parentRootPPN) {
    // 1. 复制根页表
    MemoryCopy(
        GetAddressFromPPN(childRootPPN),
        GetAddressFromPPN(parentRootPPN),
        PageSize
    );

    // 2. 复制第二级页表
    DisablePaging();
    PageTableEntry *childPTE = (PageTableEntry *)GetAddressFromPPN(childRootPPN);
    PageTableEntry *parentPTE = (PageTableEntry *)GetAddressFromPPN(parentRootPPN);
    for (Size i = 1; i < 1024; i++) {
        if (parentPTE[i].Present == 0) continue;

        u32 parentPPN = parentPTE[i].NextPPN;
        u32 childPPN = GetPPNFromAddressFloor(AllocateOnePage(UserMode));
        childPTE[i].NextPPN = childPPN;
        MemoryCopy(
            GetAddressFromPPN(childPPN),
            GetAddressFromPPN(parentPPN),
            PageSize
        );

        // 3. 复制页帧，数据页
        PageTableEntry *secondChildPTE = (PageTableEntry *)GetAddressFromPPN(childPPN);
        PageTableEntry *secondParentPTE = (PageTableEntry *)GetAddressFromPPN(parentPPN);
        for (Size j = 0; j < 1024; j++) {
            if (secondParentPTE[j].Present == 0) continue;

            u32 parentPFN = secondParentPTE[j].NextPPN;
            u32 childPFN = GetPPNFromAddressFloor(AllocateOnePage(UserMode));
            secondChildPTE[j].NextPPN = childPFN;
            MemoryCopy(
                GetAddressFromPPN(childPFN),
                GetAddressFromPPN(parentPFN),
                PageSize
            );
        }
    }
    EnablePaging();
}