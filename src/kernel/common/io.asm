[bits 32]

section .text

; 对应的C语言函数声明：u8 ReadByte(u16 port);
global ReadByte
ReadByte:
    push edx
    push ebp
    mov ebp, esp

    xor eax, eax; eax = 0
    mov edx, [ebp + 4*3]; edx = port
    in al, dx; al = result
    nop
    nop
    nop

    leave
    pop edx
    ret

; 对应的C语言函数声明：void WriteByte(u16 port, u8 value);
global WriteByte
WriteByte:
    push edx
    push ebp
    mov ebp, esp

    mov eax, [ebp + 4*4]; eax = value
    mov edx, [ebp + 4*3]; edx = port
    out dx, al
    nop
    nop
    nop

    leave
    pop edx
    ret