[org 0x7c00] ; 加载到内存的0x7c00，声明，不是要求

mov edi, 0x500 ; 加载到内存0x500中
mov ecx, 1 ; 从第二块开始加载 LBA_0是boot.bin自己
mov bl, 3 ; 加载三块
call read_disk

jmp 0:0x500 ;CS:IP

ud2;故意产生 Invalid Opcode 异常

read_disk:
    ; edi - 准备加载到的内存地址起址
    ; ecx - 开始扇区号
    ; bl -  扇区数
    pushad; 保存寄存器eax, ecx, edx, ebx, esp, ebp, esi, edi

    mov dx, 0x1f2 ; 扇区个数寄存器
    mov al, bl
    out dx, al  ;ATA port 0x1F2 ← 3

    mov al, cl   ;0x1f3 <- 1
    inc dx; 0x1f3
    out dx, al; low 8 bits of start sector

    shr ecx, 8
    mov al, cl
    inc dx; 0x1f4
    out dx, al; middle 8 bits of start sector

    shr ecx, 8
    mov al, cl
    inc dx; 0x1f5
    out dx, al; high 8 bits of start sector

    shr ecx, 8
    and cl, 0b1111

    inc dx; 0x1f6
    mov al, 0b1110_0000 ; 高4位
    or al, cl
    out dx, al

    inc dx; 0x1f7
    mov al, 0x20; set read mode
    out dx, al

    xor ecx, ecx ; ecx = 0
    mov cl, bl

.read:
    push cx
    call .waits
    call .reads
    pop cx 
    loop .read

    popad
    ; popa

    ret

.waits:
    mov dx, 0x1f7
    .check:
        nop
        nop
        nop ; a little delay
        in al, dx
        and al, 0b1000_1000
        cmp al, 0b0000_1000
        jnz .check
    ret

.reads:
    mov dx, 0x1f0 ;ATA数据寄存器
    mov cx, 256; 256 words per sector
    .readw:
        nop
        nop
        nop
        in ax, dx
        mov [edi], ax
        add edi, 2
        loop .readw
    ret

times 510 - ($ - $$) db 0
db 0x55, 0xaa