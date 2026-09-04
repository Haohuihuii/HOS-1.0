#include "mod.h"

static ProcessManager processManager;
static PIDAllocator pidAllocator;

static PCB* fetchProcess();
static void runFirstProcess();
static PCB* idleProcess;
static void idle();
static Boolean isEmpty();

// public methods

void InitializeProcessManager() {
    processManager.Current = NULL;
    for (int i = 0; i < MAX_PROCESS_COUNT; i++) {
        processManager.RunnableProcesses[i] = NULL;
    }
    processManager.Front = processManager.Rear = 0;
    CreateKernelProcess(idle);
    idleProcess = fetchProcess();
}

PID AllocatePID() {
    for (i32 i = 0; i < MAX_PROCESS_COUNT / 32; i++) {
        for (i32 j = 0; j < 32; j++) {
            if ((pidAllocator.Bitmap[i] & (1 << j)) == 0) {
                pidAllocator.Bitmap[i] |= (1 << j);
                return i * 32 + j;
            }
        }
    }
}

void FreePID(PID pid) {
    pidAllocator.Bitmap[pid / 32] &= ~(1 << (pid % 32));
}

PCB* GetCurrentProcess() {
    return processManager.Current;
}

/// @brief 入队，进入就绪队列
/// @param process 
void AddProcess(PCB* process) {
    if ((processManager.Rear + 1) % MAX_PROCESS_COUNT == processManager.Front) {
        Panic("Queue is full");
    }

    processManager.RunnableProcesses[processManager.Rear] = process;
    processManager.Rear = (processManager.Rear + 1) % MAX_PROCESS_COUNT;
}

void Schedule() {
    // 如果就绪队列里没有进程，并且当前进程也不存在，说明还未初始化进程管理器，直接退出即可
    if (processManager.Current == NULL && isEmpty()) {
        return;
    }

    // 如果就绪队列里有进程，但是当前进程不存在，说明可以执行第一个进程
    if (processManager.Current == NULL) {
        runFirstProcess();
        return;
    }
    
    // 如果就绪队列里有进程，并且当前进程存在，说明可以执行切换
    PCB* current = processManager.Current;
    if (current->ID && current->Status != PROCESS_STATE_BLOCKED) {
        current->Status = PROCESS_STATE_RUNNABLE;
        AddProcess(current);
    }

    PCB* next = fetchProcess();
    // 下一个进程为用户进程时，需要设置TSS的ESP0为内核栈栈顶，ceil(按页对齐)
    if (next->Type == PROCESS_TYPE_USER) {
        SetTSSEsp0(((u32)next->KernelStackPointer + PageSize - 1) / PageSize * PageSize);
    }
    // 下一个进程的页表和当前进程的页表不同，需要切换页表
    if (next->RootPPN != processManager.Current->RootPPN) {
        SetRootPageTableAddr(GetAddressFromPPN(next->RootPPN));
    }
    next->Status = PROCESS_STATE_RUNNING;

    processManager.Current = next;
    SwitchProcess(current, next);
}

// static methods implement

static void runFirstProcess() {
    PCB* next = fetchProcess();
    // 下一个进程为用户进程时，需要设置TSS的ESP0为内核栈栈顶，ceil(按页对齐)
    if (next->Type == PROCESS_TYPE_USER) {
        SetTSSEsp0(((u32)next->KernelStackPointer + PageSize - 1) / PageSize * PageSize);
    }
    // 下一个进程的页表需要上CPU，CR3寄存器
    SetRootPageTableAddr(GetAddressFromPPN(next->RootPPN));
    next->Status = PROCESS_STATE_RUNNING;

    PCB unused;
    PCB* unusedPtr = &unused;
    processManager.Current = next;
    SwitchProcess(unusedPtr, next);
}

static PCB* fetchProcess() {
    if (isEmpty()) {
        Panic("Queue is empty");
    }

    PCB* result = processManager.RunnableProcesses[processManager.Front];
    processManager.RunnableProcesses[processManager.Front] = NULL;
    processManager.Front = (processManager.Front + 1 ) % MAX_PROCESS_COUNT;

    return result;
}

static void idle() {
    while (TRUE) {
        Schedule();
    }
}

static Boolean isEmpty() {
    return processManager.Front == processManager.Rear;
}