#include "common/mod.h"
#include "lib/mod.h"
#include "console/mod.h"
#include "memory/mod.h"
#include "int/mod.h"
#include "process/mod.h"

void A() {
    while (TRUE) {
        PrintWithColor(GREEN, "This is A process\n");
        Schedule();
    }
}
void B() {
    while (TRUE) {
        PrintWithColor(YELLOW, "This is B process\n");
        Schedule();
    }
}
void C() {
    while (TRUE) {
        PrintWithColor(BLUE, "This is C process\n");
        Schedule();
    }
}

void TestKernelProcessWithPaging(){
    CreateKernelProcess(A);
    CreateKernelProcess(B);
    CreateKernelProcess(C);
    Schedule();
}

extern void user_process();
void TestUserProcessWithPageing() {
    CreateUserProcess(user_process);
    Schedule();
}