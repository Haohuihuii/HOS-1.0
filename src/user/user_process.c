#include "stdlib.h"

void user_process() {
    while(1) {
        int a = 32;
        a = 2 + 3;
        a = 12 + 5;
        SyscallTest();
    }
}