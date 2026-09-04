extern KernelMain
extern MemoryCheckout

global _start
_start:
    call MemoryCheckout
    call KernelMain
    jmp $