[bits 32]
section .text

; void SwitchProcess(PCB* current, PCB* next);
; 汇编压栈顺序
global SwitchProcess
SwitchProcess:
    push ebp
    push ebx
    push esi
    push edi

    mov eax, [esp + 4*5]; currentPCB
    mov [eax], esp;  save new sp to currentPCB

    mov eax, [esp + 4*6]; next nextPCB
    mov esp, [eax];  switch esp to nextPCB
 
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret; pop eip, return eip

;编译以后大致发生：
;传参数
;↓
;call SwitchProcess
;↓
;CPU 自动保存返回地址
;↓
;进入 switch.asm
