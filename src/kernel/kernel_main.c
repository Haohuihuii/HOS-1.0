#include "common/mod.h"
#include "lib/mod.h"
#include "console/mod.h"
#include "memory/mod.h"
#include "int/mod.h"
#include "process/mod.h"

void KernelMain() {
    // 清空屏幕，初始化控制台
    InitializeConsole();
    // 使用可变参数列表实现的Printf
    Printf("Hello, World! This is OS Printf\n");
    // 初始化内存管理
    InitMemoryManager();
    // 初始化中断异常处理
    InitializeInterrupt();
    // 初始化进程管理
    InitializeProcessManager();
    // 初始化全局描述符，TSS
    InitializeGDT();
    // 初始化虚拟分页
    InitializeMemoryMapping();
    // 注册缺页异常处理函数
    InitializePageFaultHandler();


    /*Printf("Before page fault test\n");

    volatile u32* test = (u32*)0x00800000;
    *test = 1234;

    Printf("After page fault test: %d\n", *test);*/

    //TestKernelProcessWithPaging();
    //TestUserProcessWithPageing();
    TestUserProcessExit();
}