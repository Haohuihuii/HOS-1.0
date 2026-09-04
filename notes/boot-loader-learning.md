[org 0x7c00]: 它告诉汇编器：本段代码最终会被放到物理地址 0x7C00 运行，因此所有标签的地址都按基址 0x7C00 计算。真正把它放到 0x7C00 的是 BIOS。
edi,exc,bl --> read_disk的参数
[org 0x7c00] = 告诉汇编器这段 Boot 代码实际会被放在 0x7C00，从而正确计算需要运行时地址的符号。
磁盘上的 Loader → boot.asm 搬到内存 0x500 → CPU 跳到 0x500 执行 Loader。
jmp 0:0x500
physical=CS*16+IP  --> 0*16+0x500 --> 0x500

经典 Primary ATA I/O ports：
0x1F0   Data
0x1F1   Error / Features
0x1F2   Sector Count
0x1F3   LBA 0~7
0x1F4   LBA 8~15
0x1F5   LBA 16~23
0x1F6   Drive/Head + LBA 24~27
0x1F7   Status / Command

cl：
ECX  32 bit
┌───────────────────────────────┐
│           ECX                 │
└───────────────────────────────┘

低 16 位叫 CX

CX:
┌───────────────┐
│   CH  │  CL   │
│ 8 bit│ 8 bit │
└───────────────┘

到/home/haohuihui/gos/src/bootloader/boot.asm 的50行为止：
告诉硬盘：
----------------
我要读 3 个 sector
起始 LBA = 1
使用 LBA 模式
开始 READ SECTORS

About:
.read:
    push cx
    call .waits
    call .reads
    pop cx 
    loop .read

    popad
    ; popa

    ret
同一个寄存器被内外两层循环复用，所以先保护！

1000 1000
│      │
│      └── bit 3 = DRQ
│
└───────── bit 7 = BSY
必须硬盘不忙，而且数据已经准备好

为什么mov cx,256?
AX = 16 bits = 2 bytes 一个sector:512 bytes.





                  BIOS
                    │
            load LBA 0
                    ↓
               0x7C00
              boot.asm
                    │
                    │
                    │ EDI = 0x500
                    │ ECX = 1
                    │ BL  = 3
                    ▼
               read_disk
                    │
          ┌─────────┴──────────┐
          │                    │
      ATA 0x1F2            sector count
      ATA 0x1F3-6          LBA
      ATA 0x1F7            READ 0x20
          │
          ▼
      wait BSY=0
      wait DRQ=1
          │
          ▼
       0x1F0
     read 256 words
          │
          ▼
       [EDI]
          │
          │ × 3 sectors
          ▼
Memory 0x500 ~ 0xAFF
        loader
          │
          ▼
     jmp 0:0x500
          │
          ▼
      loader.asm


--------------------------loader分界线--------------------------------------
10 = 0x0A = LF = Line Feed
13 = 0x0D = CR = Carriage Return
->
LF → 往下一行
CR → 回到这一行最左边

E820内存探测：
BIOS E820需要一个ES:DI来告诉BIOS 把查询到的ARDS写到哪个内存地址。
调用 E820 时必须提供 "SMAP" 这个签名，用来确认双方讲的是这套接口。
.check:
    mov eax, 0xe820
    int 0x15
->
GOS Loader

EAX = E820h
EBX = continuation
ECX = 20
EDX = "SMAP"
ES:DI = buffer
     │
     │ INT 15h
     ▼
    BIOS
     │
     ▼
查询硬件内存地图
     │
     ▼
把一个 ARDS 写到 ES:DI

[ards_cnt]：这个地址里面存的值。

              memory_checkout
                     │
                     ▼
                   ES = 0
                     │
                     ▼
                DI → ards
                     │
                     ▼
                ECX = 20
                EDX = "SMAP"
                EBX = 0
                     │
                     ▼
            EAX = 0xE820
               INT 0x15
                     │
             ┌───────┴───────┐
             ↓               ↓
           BIOS           CF=1 ?
             │               │
             │              failed
             ↓
      写入一个 20B ARDS
             │
             ▼
       ards_cnt++
       DI += 20
             │
             ▼
         EBX == 0 ?
          /       \
        否         是
        │          │
        └─ 再查    ▼
                完成

ards
 ↓
[ARDS0][ARDS1][ARDS2]...

从16位实模式进入32位保护模式：

关中断
 ↓
打开 A20
 ↓
加载 GDT
 ↓
CR0.PE = 1
 ↓
far jump
 ↓
正式进入保护模式代码

开启 A20，确保能正常访问 1MB 以上内存。


CR0.PE = 1
    ↓
保护模式机制开启
    ↓
far jump
    ↓
CS = 0x08
    ↓
从 GDT 加载代码段 descriptor
    ↓
开始真正按 32-bit protected mode 代码运行
真正让CPU进入保护模式的：CR0.PE+far jump+32-bit code descriptor

lgdt [gdt_pointer]：把 GDT 地址和大小装进 GDTR。

建立 32 位段环境
      ↓
建立 Loader 栈
      ↓
把 Kernel 读进内存
      ↓
把 E820 信息入口留给 Kernel
      ↓
跳入 Kernel



boot.asm
 │
 │ ATA PIO
 ▼
Loader → 0x500
 │
 │
 ▼
设置视频模式
 │
 ▼
打印 Loader 信息
 │
 ▼
BIOS E820
 │
 ├─→ ards_cnt
 └─→ ards[]
 │
 ▼
关闭中断
 │
 ▼
开启 A20
 │
 ▼
加载 GDT
 │
 ▼
CR0.PE = 1
 │
 ▼
far jump
 │
 ▼
32 位保护模式代码
 │
 ├─ DS/ES/FS/GS/SS = 0x10
 │
 └─ ESP = 0x7000
 │
 ▼
ATA PIO
 │
 │ LBA 4 开始读 200 sectors
 ▼
Kernel → 0x7E00
 │
 ▼
push ards_cnt 地址
 │
 ▼
far jump 0x08:0x7E00
 │
 ▼
Kernel Entry

jmp 0x08:0x7E00 最终修改了哪两个关键寄存器？ : CS,EIP

没有给 MemoryCheckout 写参数，为什么 C 函数却能收到 Loader 的 ards_cnt 地址？
ESP
 ↓
┌──────────────────────┐
│ 返回地址              │ ← [ESP]
├──────────────────────┤
│ ards_cnt 地址         │ ← [ESP + 4]
└──────────────────────┘
（栈往低地址方向增长！）

传统C栈调用方式：
[ESP]     → 返回地址
[ESP + 4] → 第 1 个参数
[ESP + 8] → 第 2 个参数
[ESP +12] → 第 3 个参数
...
也就是说：
[ESP]     = call 自动压入的返回地址
[ESP + 4] = Loader 提前压进去的 ards_cnt 地址


from BIOS to Kernel:
BIOS
 ↓
0x7C00 boot.asm
 ↓
ATA PIO 读取 Loader
 ↓
0x500 loader.asm
 ↓
BIOS E820 获取内存地图
 ↓
A20
 ↓
GDT（全局描述符表）
 ↓
CR0.PE
 ↓
保护模式
 ↓
32 位执行环境
 ↓
ATA PIO 读取 Kernel
 ↓
Kernel → 0x7E00
 ↓
push ards_cnt 地址
 ↓
entry.asm::_start
 ↓
MemoryCheckout
 ↓
建立物理内存管理基础
 ↓
KernelMain
 ↓
进入 C Kernel
-----------------------------------------------------------------------
questions:
1. BIOS 为什么从 0x7C00 开始执行

这是 x86 的硬件约定。BIOS 启动时执行自检，然后把磁盘第 1 扇区（512 字节）读到物理地址 0x7C00，检查该扇区最后两个字节是否为 0x55 0xAA（boot.asm 第 89 行 db 0x55, 0xaa 就是它），满足则 jmp 0x7C00。
Legacy BIOS 通常把可启动设备的首个 512B boot sector 加载到物理地址 0x7C00，验证 0x55AA 签名后转交控制权。

2. bootloader 如何加载 loader

boot.asm 的 read_disk 用 IDE PIO（Programmed I/O）方式读盘：向端口 0x1F2~0x1F7 写命令（扇区数、起始扇区 LBA、读命令 0x20），然后轮询 0x1F7 端口等待磁盘就绪（.waits），最后从 0x1F0 按 16 位（word）连续读数据到目标内存（.reads）。本代码中：edi=0x500（目标）、ecx=1（第2扇区起）、bl=3（读3个扇区），即把磁盘扇区 2~4 的 loader 读到 0x500，然后 jmp 0:0x500。

3. loader 为什么位于 0x500

0x500 属于实模式下的常规内存低端区，位于 0x400~0x500 的 BIOS 数据区（BDA）之上、0x7C00 的 MBR 之下，这段区域没有会被 MBR/BIOS 立即覆盖的内容，是加载第二阶段引导程序的惯例位置。boot.asm 第 3 行 mov edi, 0x500 直接写死这个地址。

4. E820 内存探测如何工作，结果如何传给 kernel

loader 的 memory_checkout 循环调用 int 0x15，eax=0xE820，让 BIOS 返回一段内存区间描述（ARDS：BaseAddress/Length/Type 共 20 字节），ebx 作为游标直到为 0 表示枚举完。每条记录存入 ards 数组，条数记在 ards_cnt。

传递方式（loader.asm 第 127 行）：push ards_cnt——压入的是 ards_cnt 这个标签的地址（不是值）。然后 jmp 0x7E00（不是 call）。entry.asm 的 call MemoryCheckout 时，这个地址正好成为栈上的第一个参数，被 MemoryCheckout(PhysicalAddress* ardCountAddress) 消费，通过它找到 ards_cnt 和紧随其后的 ards 数组。

5. A20 为什么必须开启

8086 的 20 位地址线有"回绕"问题：实模式段式地址 0xFFFF:0xFFFF 会访问到 0x10FFEF，若 A20 未开，第 20 位被强制为 0，实际访问 0x0FFEF，导致内存访问错误。保护模式地址可达 4GB，必须让第 20 位生效。loader.asm 第 100-102 行通过键盘控制器端口 0x92 置位 A20 门。

6. loader 中临时 GDT 的作用

保护模式的分段机制要求 CPU 通过 GDT 查段描述符。loader 进入保护模式前必须 lgdt 加载一个 GDT，否则 jmp 到保护模式后无法解析段。这个 GDT 只有 3 个段（空段、代码段、数据段，loader.asm 第 261-281 行），仅够引导用。注意它只是临时的——内核里 gdt.c 的 InitializeGDT 会重建一个 6 段的正式 GDT 并再次 lgdt。

7. CR0.PE 的作用

CR0 第 0 位 PE（Protection Enable）是实模式/保护模式的总开关。loader.asm 第 106-108 行：读出 CR0、or 1、写回，即置 PE=1，CPU 从此以保护模式的分段+分页语义执行。注意此时分页还没开（PG 位未置），只有分段。

8. 为什么进入保护模式需要 far jump

置 CR0.PE 后，CPU 的 CS 仍是实模式的旧值（段基址=段值×16）。必须用 jmp dword code_segment_selector:entry_protect_mode（loader.asm 第 110 行）做一次远跳转：既刷新 CS 为保护模式的代码段选择子（0x08→GDT[1]），又把 EIP 设为新入口。远跳转会强制 CPU 重新加载 CS，清空流水线中的实模式指令。

9. 进入保护模式以后为什么重新加载段寄存器

远跳转只刷新了 CS。DS/ES/FS/GS/SS 仍是实模式的旧段寄存器（值还停留在实模式），里面是"段值×16"的旧语义，必须全部换成保护模式的数据段选择子，否则任何数据访问都会错乱。loader.asm 第 114-119 行逐个重载 DS/ES/FS/GS/SS = data_segment_selector(0x10→GDT[2])。

10. 为什么 ESP 设置为 0x7000

loader.asm 第 121 行 mov esp, 0x7000。0x7000 位于 loader(0x500) 之上、内核(0x7E00) 之下，处于不会与引导代码冲突的空闲常规内存区，作为临时内核栈顶足够用（引导阶段栈需求小）。内核正式运行后，进程会建立各自独立的内核栈页（AllocateOnePage(KernelMode)）。

11. kernel 为什么加载到 0x7E00

当前 GOS 为了简化 early boot，选择把内核链接地址设为 0x7E00，与 loader 的磁盘加载目标保持一致。这个地址避开了位于 0x500 的二阶段 loader 和 0x7000 附近的临时栈，同时恰好位于 boot sector 的 0x7C00~0x7DFF 之后。这是本项目设计，不是 x86 的强制要求

12. entry.asm 如何调用 C 函数

extern KernelMain / extern MemoryCheckout 声明外部符号，call MemoryCheckout / call KernelMain 直接调用。链接时 ld 把 entry.o 放第一个（-Ttext 0x7E00），C 函数的地址由链接器解析填入 call 的 rel32 偏移。C 函数的参数/返回值遵循 32 位 cdecl 约定（栈传参、eax 返回值）。注意 entry 里 call MemoryCheckout 时栈上已有 loader 压入的 ards_cnt 地址作为参数。

13. MemoryCheckout 为什么在 KernelMain 前执行

MemoryCheckout（frame.c 中）做两件事：初始化帧分配器的 bitmap，并把 E820 探测到的可用内存逐页标记为空闲。这是物理内存管理的地基——KernelMain 第 3 步 InitMemoryManager（Buddy）要调 AllocatePagesContinuously 从帧分配器预支页，第 7 步 InitializeMemoryMapping（分页）要分配页表页。所以必须在所有内存分配发生之前，先把"哪些物理页空闲"这件事搞清楚。

14. 为什么 CR0.PE=1 后还要 far jump？

CR0.PE=1 打开保护模式机制，far jump 则重新加载 CS，使 CPU 使用 GDT 中的新代码段继续执行。

