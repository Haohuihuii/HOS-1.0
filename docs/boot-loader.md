# GOS Boot Process

本文描述当前 GOS 从机器启动到进入 C 内核的完整启动流程。

当前启动链：

BIOS
→ boot.asm
→ loader.asm
→ 32 位保护模式
→ entry.asm
→ MemoryCheckout
→ KernelMain

---

## 1. BIOS 加载 Boot Sector

机器启动后，BIOS（Basic Input/Output System，基本输入输出系统）
完成基础硬件初始化，并从启动磁盘读取第一个扇区。

该扇区为 512 字节，被加载到物理地址：

0x7C00

随后 BIOS 将控制权转移到该地址。

需要注意：

0x7C00 是传统 PC BIOS 的启动约定，
不是 x86 CPU 本身规定的启动地址。

boot.asm 使用：

[org 0x7c00]

告诉 NASM 汇编器该程序运行时按照 0x7C00 作为地址基准。

Boot Sector 最后两个字节为：

0x55 0xAA

作为 BIOS 识别启动扇区的签名。

---

## 2. Boot Sector 加载 Loader

boot.asm 通过 ATA PIO 方式读取磁盘。

ATA（Advanced Technology Attachment，高级技术附件）
PIO（Programmed Input/Output，程序控制输入输出）

参数：

EDI = 0x500
ECX = 1
BL  = 3

含义：

- 从 LBA 1 开始读取
- 共读取 3 个扇区
- 写入物理内存 0x500

LBA（Logical Block Addressing，逻辑块寻址）从 0 开始编号。

因此当前磁盘布局为：

LBA 0     boot.bin
LBA 1~3   loader.bin
LBA 4...  os.bin

读取完成后执行：

jmp 0:0x500

将 CS 设置为 0，IP 设置为 0x500，
开始执行 Loader。

---

## 3. ATA PIO 磁盘读取

当前 Boot 和 Loader 都使用同一类 ATA PIO 读取逻辑。

主要端口：

0x1F0  Data
0x1F2  Sector Count
0x1F3  LBA bits 0~7
0x1F4  LBA bits 8~15
0x1F5  LBA bits 16~23
0x1F6  Device/Head + LBA bits 24~27
0x1F7  Command / Status

读取命令：

0x20 = READ SECTORS

程序等待：

BSY = 0
DRQ = 1

然后从 0x1F0 每次读取 16 bit。

每个磁盘扇区：

512 Byte = 256 × 16 bit

因此读取一个扇区需要执行 256 次 word 读取。

---

## 4. Loader 初始化

Loader 被加载到：

0x500

并使用：

[org 0x500]

作为其运行时地址基准。

Loader 开始时 CPU 仍处于 16 位实模式。

Loader 首先通过 BIOS 视频服务设置文本模式并打印启动信息。

当前打印函数使用：

INT 0x10

而不是直接写 VGA 文本显存。

VGA（Video Graphics Array，视频图形阵列）
文本显存物理地址为：

0xB8000

---

## 5. E820 内存探测

Loader 使用 BIOS INT 0x15 的 E820 功能获取物理内存布局。

主要输入：

EAX = 0xE820
EDX = 0x534D4150
ECX = 20
ES:DI = 输出缓冲区
EBX = continuation value

第一次查询：

EBX = 0

BIOS 每次返回一个 ARDS。

ARDS（Address Range Descriptor Structure，地址范围描述结构）
当前使用 20 字节格式：

- BaseAddrLow
- BaseAddrHigh
- LengthLow
- LengthHigh
- Type

其中：

Type = 1

表示该物理内存区域可供操作系统使用。

Loader 将返回结果连续存储在：

ards[]

同时使用：

ards_cnt

记录 ARDS 数量。

当 BIOS 返回：

EBX = 0

表示内存地图枚举结束。

---

## 6. 进入保护模式

完成 BIOS 信息收集后，Loader 开始进入 32 位保护模式。

主要步骤：

1. 关闭中断
2. 开启 A20
3. 加载 GDT
4. 设置 CR0.PE
5. 执行远跳转

---

### 6.1 开启 A20

开启 A20 后，系统可以正常访问 1MB 以上的物理地址，
避免历史兼容机制导致地址回绕。

当前代码通过端口 0x92 开启 A20。

---

### 6.2 加载 GDT

GDT（Global Descriptor Table，全局描述符表）
保存保护模式使用的段描述符。

当前 Loader 的 GDT 包含：

GDT[0]  Null Descriptor
GDT[1]  Kernel Code Segment
GDT[2]  Kernel Data Segment

代码段选择子：

0x08

解析：

Index = 1
TI = 0
RPL = 0

因此选择：

GDT[1]

数据段选择子：

0x10

解析：

Index = 2
TI = 0
RPL = 0

因此选择：

GDT[2]

TI（Table Indicator，表指示位）
表示选择 GDT 还是 LDT。

RPL（Requested Privilege Level，请求特权级）
表示该段选择子的请求特权级。

Loader 执行：

lgdt [gdt_pointer]

将 GDT 的：

- Base
- Limit

加载到 GDTR。

GDTR（Global Descriptor Table Register，全局描述符表寄存器）
告诉 CPU 当前 GDT 位于哪里以及有多大。

---

### 6.3 设置 CR0.PE

CR0（Control Register 0，控制寄存器 0）
中的 PE（Protection Enable，保护模式使能）位被设置为 1。

这使 CPU 开启保护模式机制。

随后 Loader 执行远跳转：

jmp code_segment_selector:entry_protect_mode

远跳转重新加载 CS，
使 CPU 使用 GDT 中新的代码段描述符继续执行。

---

## 7. 建立 32 位运行环境

entry_protect_mode 后面的代码使用：

[bits 32]

该指令是 NASM 汇编器指令，
用于告诉 NASM 按照 32 位默认操作数和地址大小生成机器码。

它本身不会让 CPU 进入保护模式。

进入保护模式依赖：

- CR0.PE = 1
- GDT
- 远跳转重新加载 CS
- 代码段描述符的 32 位属性

进入保护模式后：

DS = 0x10
ES = 0x10
FS = 0x10
GS = 0x10
SS = 0x10

随后：

ESP = 0x7000

建立 Loader 使用的 32 位栈。

---

## 8. Loader 加载 Kernel

Loader 再次使用 ATA PIO：

EDI = 0x7E00
ECX = 4
BL  = 200

含义：

从 LBA 4 开始读取 200 个扇区，
并写入物理地址 0x7E00。

最大加载量：

200 × 512 Byte
= 102400 Byte
≈ 100 KB

因此当前 Boot 协议要求 Kernel 二进制不能超过该加载范围，
除非同时修改 Loader。

---

## 9. 向 Kernel 传递内存地图

Loader 执行：

push ards_cnt

这里压入的不是 ARDS 数量，
而是 ards_cnt 变量本身的地址。

内存布局为：

ards_cnt
↓
+------------------+
| ARDS count       |
+------------------+
| ARDS #0          |
+------------------+
| ARDS #1          |
+------------------+
| ...              |
+------------------+

因此只需要传递 ards_cnt 的地址，
Kernel 就可以同时找到：

1. ARDS 数量
2. 紧随其后的 ARDS 数组

随后执行：

jmp 0x08:0x7E00

将控制权永久交给 Kernel。

---

## 10. entry.asm

Kernel 的汇编入口负责将启动阶段与 C 内核连接起来。

核心流程：

_start:
    call MemoryCheckout
    call KernelMain
    jmp $

Loader 进入 Kernel 前已经：

push ards_cnt

因此 Kernel 刚进入 _start 时：

[ESP] = ards_cnt 地址

执行：

call MemoryCheckout

后，call 指令首先压入返回地址。

此时：

[ESP]     = return address
[ESP + 4] = ards_cnt address

这正好符合当前 32 位 C 函数的栈参数布局。

因此：

MemoryCheckout(ardCountAddress)

可以直接收到 Loader 传递的 ards_cnt 地址。

---

## 11. Loader Memory Checkout 与 Kernel MemoryCheckout

两个函数作用不同。

Loader 的 memory_checkout：

BIOS E820
→ 获取物理内存地图
→ 保存 ards_cnt 和 ards[]

Kernel 的 MemoryCheckout：

ards_cnt + ards[]
→ 解析 BIOS 返回结果
→ 为 Kernel 后续物理内存管理提供基础

---

## 12. 进入 C Kernel

完成 MemoryCheckout 后：

call KernelMain

从这里开始，系统进入主要的 C 内核初始化流程。

因此整个启动路径为：

BIOS
→ Boot Sector
→ Loader
→ E820
→ A20
→ GDT
→ 32 位保护模式
→ 加载 Kernel
→ entry.asm
→ MemoryCheckout
→ KernelMain



end:
# Boot Learning Notes

## 已掌握

- BIOS 为什么把 Boot Sector 加载到 0x7C00
- Boot 如何通过 ATA PIO 加载 Loader
- ATA PIO 读取一个扇区的基本过程
- Loader 如何使用 BIOS E820 获取物理内存地图
- ARDS 和 ards_cnt 的关系
- A20 的作用
- GDT 与段描述符的作用
- 段选择子的 Index / TI / RPL
- lgdt 的作用
- CR0.PE 与保护模式的关系
- 为什么设置 PE 后还需要 far jump
- [bits 32] 与 CPU 模式切换的区别
- Loader 如何建立 32 位栈
- Loader 如何加载 Kernel 到 0x7E00
- Loader 如何通过栈向 Kernel 传递 ards_cnt 地址
- entry.asm 如何利用 C 调用约定调用 MemoryCheckout
- entry.asm 如何进入 KernelMain

## 我现在能解释的完整启动链

BIOS
→ boot.asm
→ Loader
→ E820
→ A20
→ GDT
→ CR0.PE
→ far jump
→ 32-bit protected mode
→ load Kernel
→ push ards_cnt
→ entry.asm
→ MemoryCheckout
→ KernelMain