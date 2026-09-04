#include "mod.h"

extern void RestoreContext();

/// @brief 让栈指针指向Interrupt Context方便返回用户态
static void restore();


void CreateKernelProcess(void* entry) {
    PCB* process = (PCB*)Malloc(sizeof(PCB));
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
    SwitchContext* context = (SwitchContext*)stack;
    context->EIP = (u32)entry;
    context->EBP = 0;
    context->ESI = 0;
    context->EDI = 0;
    context->EBX = 0;
    process->KernelStackPointer = (PhysicalAddress*)stack;

    AddProcess(process);
}

void CreateUserProcess(void* entry) {
    PCB* process = (PCB*)Malloc(sizeof(PCB));
    process->ID = AllocatePID();
    process->Status = PROCESS_STATE_RUNNABLE;
    u32 stack = AllocateOnePage(KernelMode) + PageSize;
    process->Type = PROCESS_TYPE_USER;
    // 构造用户专属页表，复制内核页表的前4MB等页号映射
    u32 userRootPPN = GetPPNFromAddressFloor(AllocateOnePage(KernelMode));
    process->RootPPN = userRootPPN;
    PhysicalAddress userRootAddr = GetAddressFromPPN(userRootPPN);
    MemoryCopy(
        (void*)userRootAddr,
        (void*)GetRootPageTableAddr(),
        PageSize
    );

    // 为用户栈（UserStackTop = 0x10000000, 256MB处）建立页表映射
    // 内核等值映射只覆盖 0-4MB，用户栈在 256MB 处超出范围，需手动建立 PDE/PTE
    // 注意：x86 栈向下增长！ESP=0x10000000 时第一个 push 写入 0x0FFFFFFC，
    //       属于页面 0x0FFFF000（PDE[63], PTE[1023]），而非 0x10000000
    {
        u32* pdeArray = (u32*)userRootAddr;
        //userRootAddr -> 当前用户进程 Page Directory 的起始地址
        u32 stackPageBase = UserStackTop - PageSize;               // 0x0FFFF000
        u32 pdeIndex = stackPageBase >> 22;                       // = 63
        u32 pteIndex = (stackPageBase >> 12) & 0x3FF;            // = 1023

        // 页目录中为该 4MB 区域创建 PDE，指向新分配的二级页表
        if (!(pdeArray[pdeIndex] & 1)) {
            u32 secondPT = AllocateOnePage(KernelMode);
            MemoryFree((void*)secondPT, PageSize);
            u32 secondaryPPN = secondPT >> 12;
            pdeArray[pdeIndex] = 0x007 | (secondaryPPN << 12);     // P=1, R/W=1, U/S=1 允许用户态访问
        }

        // 二级页表中为用户栈页面创建 PTE
        // 注意：必须用 KernelMode 分配栈物理页，因为 KernelMode 返回 4MB 以内的地址，
        // 内核等值映射可以访问；UserMode 返回 4MB 以上的地址，内核无法直接清零
        u32* pteArray = (u32*)(pdeArray[pdeIndex] & 0xFFFFF000);
        u32 stackPhys = AllocateOnePage(KernelMode);
        u32 stackPPN = stackPhys >> 12;
        pteArray[pteIndex] = 0x007 | (stackPPN << 12);            // P=1, R/W=1, U/S=1
    }
    /*
    Virtual Address
0x0FFFF000
      │
      ▼
PDE[63]
      │
      ▼
Page Table 63
      │
      ▼
PTE[1023]
      │
      ▼
Physical Page
0x00130000
    */
    stack -= sizeof(InterruptContext);
    InterruptContext* trapContext = (InterruptContext*)stack;
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
    SwitchContext* context = (SwitchContext*)stack;
    context->EIP = restore;
    context->EBP = 0;
    context->ESI = 0;
    context->EDI = 0;
    context->EBX = 0;
    process->KernelStackPointer = (PhysicalAddress*)stack;

    AddProcess(process);
}

static void restore() {
    PCB* current = GetCurrentProcess();
    u32 stack = ((u32)current->KernelStackPointer + PageSize - 1) / PageSize * PageSize;
    stack -= sizeof(InterruptContext);
    asm volatile ("movl %0, %%esp" : : "m"(stack));
    asm volatile ("jmp RestoreContext");
}
