#include "../lib/mod.h"
#include "mod.h"

// global variables
void*  InterruptHandlerList[INTERRUPT_COUNT];
static HashTable* ExceptionMessage;


// static methods

static void initExceptionInfoMap();
static void defaultExceptionHandler(u32 vector);
static void defaultOuteralInterruptHandler(u32 vector);
static void initDefaultInterruptHandler();
static void initProgrammableInterruptController();


// public methods

void InitializeInterrupt() {
    initProgrammableInterruptController();
    //初始化8259A可编程中断控制器（PIC）
    InitializeIDT();
    initExceptionInfoMap();
    initDefaultInterruptHandler();
}

void SetInterruptHandler(u32 vector, void* handler) {
    InterruptHandlerList[vector] = handler;
}

// 接触IRQ(vector)的屏蔽，允许中断
void SetInterrupt(u32 vector) {
    Assert(vector >= EXCEPTION_COUNT && vector < EXCEPTION_COUNT + OUTERAL_INTERRUPT_COUNT);
    vector -= EXCEPTION_COUNT;

    if (vector < 8) {
        WriteByte(0x21, ReadByte(0x21) & ~(1 << vector)); // 主片
    } else {
        WriteByte(0xA1, ReadByte(0xA1) & ~(1 << (vector - 8)));
    }
}

void OuteralInterruptCompleted(u32 vector) {
    if (vector >= 0x28) {
        WriteByte(0xA0, 0x20); // 从片
    }
    WriteByte(0x20, 0x20);
    //这个中断结束了，你可以继续处理后续 IRQ。
}

u8 GetInterruptStatus() {
    asm volatile ("pushf");
    //把EFLAGS.IF压栈
    asm volatile ("pop %eax");
    //读到EAX
    asm volatile ("and $0x200, %eax");
    //10 0000 0000
    asm volatile ("shr $9, %eax");
    //CPU是否允许可屏蔽中断
}

void RestoreInterruptStatus(u8 status) {
    if (status) {
        asm volatile ("sti");
        //set interrupt flag 允许CPU相应可屏蔽中断
    } else {
        asm volatile ("cli");
    }
    //恢复IF状态
}

// static methods implementation

static void defaultExceptionHandler(u32 vector) {
    Error("Exception{%d}: %s", vector, ExceptionMessage->Get(ExceptionMessage, vector));
    Suspend();
}

static void defaultOuteralInterruptHandler(u32 vector) {
    OuteralInterruptCompleted(vector);
    Trace("Outeral interrupt{%d} invoked", vector);
}

static void initDefaultInterruptHandler() {
    for (i32 i = 0; i < EXCEPTION_COUNT; i++) {
         InterruptHandlerList[i] = defaultExceptionHandler;
    }

    for (i32 i = 0; i < OUTERAL_INTERRUPT_COUNT; i++) {
         InterruptHandlerList[i + EXCEPTION_COUNT] = defaultOuteralInterruptHandler;
    }
}

// 初始化可编程中断控制器
static void initProgrammableInterruptController() {
    /*
    ref: https://blog.csdn.net/longintchar/article/details/79439466
        在8259A可以正常工作之前，必须首先设置初始化命令字 ICW (Initialization Command Words)寄存器组的内容。
        而在其工作过程中，则可以使用写入操作命令字 OCW (Operation Command Words)寄存器组来随时设置和管理8259A的工作方式。
    */
    // ICW1
    WriteByte(0x20, 0x11); // 表示中断请求是边沿触发、多片 8259A 级联并且需要发送 ICW4
    WriteByte(0xA0, 0x11);
    //ICW1 开始初始化两片 8259A，并指定它们以级联方式工作。
    // ICW2
    WriteByte(0x21, 0x20); // 表示主片中断请求0~7级对应的中断号是 0x20~0x27
    WriteByte(0xA1, 0x28); // 表示从片中断请求8~15级对应的中断号是 0x28~0x2F

    // ICW3
    WriteByte(0x21, 0x04); // 表示主芯片的 IR2 引脚连接一个从芯片
    //0000 0100    IPQ2
    WriteByte(0xA1, 0x02); // 从芯片的 ICW3 被设置为 0x02，即其标识号为2。表示此从片连接到主片的IR2引脚。

    // ICW4
    WriteByte(0x21, 0x01); // 表示 8259A 芯片被设置成普通全嵌套、非缓冲、非自动结束中断方式，并且用于 8086 及其兼容系统。
    //非自动结束中断  --> 一个 IRQ 处理完以后，操作系统需要主动告诉 PIC：“这次中断处理完了。”
    WriteByte(0xA1, 0x01);
    // OCW1: OCW1 用于对 8259A 中中断屏蔽寄存器 IMR 进行读/写操作。地址线A0需为1。
    WriteByte(0x21, 0xff); // 初始屏蔽所有中断  先把 PIC 配置好，但是所有硬件 IRQ 默认关闭。
    WriteByte(0xA1, 0xff);
}

static void initExceptionInfoMap() {
    ExceptionMessage = NewMap("u32", "string");
    ExceptionMessage->Put(ExceptionMessage, 0,  "Divide Error");
    ExceptionMessage->Put(ExceptionMessage, 1,  "Debug");
    ExceptionMessage->Put(ExceptionMessage, 2,  "NMI Interrupt");
    ExceptionMessage->Put(ExceptionMessage, 3,  "Breakpoint");
    ExceptionMessage->Put(ExceptionMessage, 4,  "Overflow");
    ExceptionMessage->Put(ExceptionMessage, 5,  "BOUND Range Exceeded");
    ExceptionMessage->Put(ExceptionMessage, 6,  "Invalid Opcode");
    ExceptionMessage->Put(ExceptionMessage, 7,  "Device Not Available");
    ExceptionMessage->Put(ExceptionMessage, 8,  "Double Fault");
    ExceptionMessage->Put(ExceptionMessage, 9,  "Coprocessor Segment Overrun");
    ExceptionMessage->Put(ExceptionMessage, 10, "Invalid TSS");
    ExceptionMessage->Put(ExceptionMessage, 11, "Segment Not Present");
    ExceptionMessage->Put(ExceptionMessage, 12, "Stack-Segment Fault");
    ExceptionMessage->Put(ExceptionMessage, 13, "General Protection");
    ExceptionMessage->Put(ExceptionMessage, 14, "Page Fault");
    ExceptionMessage->Put(ExceptionMessage, 15, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 16, "x87 FPU Floating-Point Error");
    ExceptionMessage->Put(ExceptionMessage, 17, "Alignment Check");
    ExceptionMessage->Put(ExceptionMessage, 18, "Machine Check");
    ExceptionMessage->Put(ExceptionMessage, 19, "SIMD Floating-Point Exception");
    ExceptionMessage->Put(ExceptionMessage, 20, "Virtualization Exception");
    ExceptionMessage->Put(ExceptionMessage, 21, "Control Protection Exception");
    ExceptionMessage->Put(ExceptionMessage, 22, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 23, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 24, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 25, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 26, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 27, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 28, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 29, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 30, "Reserved");
    ExceptionMessage->Put(ExceptionMessage, 31, "Reserved");
}