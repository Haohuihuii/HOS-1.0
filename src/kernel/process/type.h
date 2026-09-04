#pragma once

#include "../common/type.h"

typedef enum ProcessState {
    PROCESS_STATE_RUNNABLE,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED
} ProcessState;

typedef enum ProcessType {
    PROCESS_TYPE_USER,
    PROCESS_TYPE_KERNEL
} ProcessType;

typedef struct PCB {
    PhysicalAddress* KernelStackPointer;
    PID ID;
    ProcessState Status;
    ProcessType Type;
    u32 RootPPN;
} PCB;

// PID allocator
#define MAX_PROCESS_COUNT 1024
typedef struct PIDAllocator {
    u32 Bitmap[MAX_PROCESS_COUNT / 32];
} PIDAllocator;

// Process Manager
typedef struct ProcessManager {
    PCB* Current;
    PCB* RunnableProcesses[MAX_PROCESS_COUNT];
    u32 Front;
    u32 Rear;
} ProcessManager;

// Process Need

#define PageSize 4096

// Context
typedef struct SwitchContext {
    u32 EDI;
    u32 ESI;
    u32 EBX;
    u32 EBP;
    u32 EIP;
} SwitchContext;

typedef struct InterruptContext {
    u32 Vector;  // 中断向量号
    u32 EDI;
    u32 ESI;
    u32 EBP;
    u32 ESP;
    u32 EBX;
    u32 EDX;
    u32 ECX;
    u32 EAX;
    u32 GS;
    u32 FS;
    u32 ES;
    u32 DS;
    u32 ErrCode; // 错误码，默认0x88888888
    u32 EIP;  // 程序计数器
    u32 CS;
    u32 PSW; // 程序状态字 EFlags
    u32 ESP3; // 用户态栈
    u32 SS3;  // 用户态栈段选择子
} InterruptContext;

#define UserStackTop 0x10000000