#include "mod.h"

static u32 SystemCall(u32 syscallNum, u32 arg1, u32 arg2, u32 arg3) {
    u32 result;

    asm volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(syscallNum),
          "b"(arg1),
          "c"(arg2),
          "d"(arg3)
        : "memory","cc"
    );
    return result;
}

void SyscallTest() {
    SystemCall(SYSCALL_TEST, 1, 2, 3);
}

u32 Fork() {
    return SystemCall(SYSCALL_FORK, 0, 0, 0);
}

void Yield() {
    SystemCall(SYSCALL_YIELD, 0, 0, 0);
}

void Exit(i32 exitCode) {
    SystemCall(SYSCALL_EXIT, exitCode, 0, 0);
}