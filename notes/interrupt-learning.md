分段权限
    ↓
分页权限
    ↓
真正访问物理内存

1010 → 可执行代码段
0010 → 可写数据段

DPL 0最高3最低
CPL Current Privilege Level
CPL 通常由当前 CS 段选择子的最低两位决定。

16位的Segment Selector
15                     3  2   1 0
+-----------------------+---+-----+
|        Index          |TI | RPL |
+-----------------------+---+-----+

00100011
0x08 → Kernel Code
0x10 → Kernel Data
0x18 → TSS

TI = 0 → GDT
TI = 1 → LDT

InitializeGDT
│
├─ 设置 GDTR
│
├─ GDT[0] Null
│
├─ GDT[1] Kernel Code, DPL=0
│
├─ GDT[2] Kernel Data, DPL=0
│
├─ GDT[3] TSS
│
├─ GDT[4] User Code, DPL=3
│
└─ GDT[5] User Data, DPL=3

每个进程都有自己的内核栈，所以进程切换以后，要把 TSS.ESP0 改成当前进程的内核栈顶。

中断前：

用户栈
    ↑
   ESP


发生 Ring3 → Ring0 中断


CPU 查 TSS
   │
   ├── SS0
   └── ESP0
        │
        ▼

内核栈
┌─────────────┐
│ 用户态 SS    │
│ 用户态 ESP   │
│ EFLAGS      │
│ 用户态 CS    │
│ 用户态 EIP   │
│ ...         │
└─────────────┘
       ↑
      ESP


TSS.ESP0
=
用户态发生中断进入内核时
CPU 要切换到的“当前进程内核栈顶”

IDT = Interrupt Descriptor Table，中断描述符表 -> 某个中断/异常发生以后，CPU 应该跳到哪个处理入口。

GDT
↓
“段是什么？”

IDT
↓
“中断来了跳哪？”

Selector
↓
新的 CS

Offset
↓
新的 EIP



发生 vector 14
        ↓
CPU 查 IDT[14]
        ↓
拿到 Selector = 0x08
        ↓
解析 0x08
        ↓
GDT[1]
        ↓
Kernel Code Segment



为什么 IDT[0x80]的DPL是3？
普通异常/硬件中断门
DPL = 0
系统调用门
DPL = 3
用户程序运行在CPL=3， 如果DPL设置为0， 用户态就不能合法调用， 但CPU自己产生的异常是可以的

IDT.DPL
主要防的是：
“你有没有资格用 INT n 主动敲这个门？”
不是：
“CPU 能不能因为异常/硬件事件进入这个门？”

DPL=谁能敲门，Selector=门后面通向哪里

InitializeIDT
│
├─ 设置 IDT[0~47]
│    │
│    ├─ handler = 汇编入口
│    ├─ Selector = 0x08
│    ├─ Type = Interrupt Gate
│    ├─ DPL = 0
│    └─ Present = 1
│
├─ 设置 IDT[0x80]
│    │
│    ├─ handler = AllTrapsEntry
│    ├─ Selector = 0x08
│    ├─ Type = Interrupt Gate
│    ├─ DPL = 3
│    └─ Present = 1
│
├─ IDTR.Base = IDT
├─ IDTR.Limit = sizeof(IDT)-1
│
└─ lidt


          vector = 14
               │
               ▼
             IDTR
               │
               ▼
            IDT[14]
          /          \
         /            \
Selector=0x08        Offset
     │                 │
     ▼                 │
   GDT[1]              │
     │                 │
Kernel Code            │
     │                 │
     └──────┬──────────┘
            ▼

    CS:EIP 指向
InterruptHandler_0x0e

IDT.Offset → 中断入口代码地址

DPL 主要限制软件主动 int n，不是阻止 CPU 自己产生异常或硬件中断。

                 第一层：汇编入口
IDT[14]
   ↓
InterruptHandlerEntryTable[14]
   ↓
InterruptHandler_0x0e
   ↓
保存 CPU 现场
   ↓

                 第二层：真正的 C handler
InterruptHandlerList[14]
   ↓
PageFaultHandler / 默认异常处理函数

保存现场： 中断处理程序马上要执行很多代码，会修改 CPU 寄存器；但是中断结束以后，原程序还要接着运行。

                高地址

        ┌──────────────┐
        │ EFLAGS       │  ← CPU
        ├──────────────┤
        │ CS           │  ← CPU
        ├──────────────┤
        │ EIP          │  ← CPU
        ├──────────────┤
        │ ErrCode      │  ← CPU或GOS补
        ├──────────────┤
        │ DS           │
        ├──────────────┤
        │ ES           │
        ├──────────────┤
        │ FS           │
        ├──────────────┤
        │ GS           │
        ├──────────────┤
        │ EAX          │
        ├──────────────┤
        │ ECX          │
        ├──────────────┤
        │ EDX          │
        ├──────────────┤
        │ EBX          │
        ├──────────────┤
        │ saved ESP    │
        ├──────────────┤
        │ EBP          │
        ├──────────────┤
        │ ESI          │
        ├──────────────┤
ESP →   │ EDI          │
        └──────────────┘

                低地址


中断返回：iret 会根据 CPU 之前保存的中断现场恢复这些状态。


① 中断发生
        ↓
② CPU 根据 vector 查 IDT
        ↓
③ CPU 自动保存
   EIP / CS / EFLAGS
   [+ ErrorCode]
        ↓
④ 跳进 InterruptHandler_0xXX
        ↓
⑤ 没有错误码就补 0x88888888
        ↓
⑥ push ds/es/fs/gs + pushad
   保存现场
        ↓
⑦ 从 InterruptHandlerList[vector]
   找真正的 C handler
        ↓
⑧ handler(vector, errorCode)
        ↓
⑨ popad + 恢复段寄存器
        ↓
⑩ 删除 errorCode
        ↓
⑪ iret
        ↓
⑫ 回到被打断的代码

为什么SS0初始化设置一次，但是ESP0要不停修改？所有进程进入内核以后
使用的栈段都可以是 Kernel Data Segment
不同进程有不同的内核栈。

调度一个用户进程之前，需要同时准备两个完全不同的环境：
TSS.ESP0 → 它以后进入内核时用哪张内核栈
CR3 → 它运行时使用哪套虚拟地址空间
字段顺序不是为了好看，而是必须和真实汇编栈布局严格匹配。
向上对齐得到的是栈页高地址边界/栈顶，不是页起始地址。

PIC 收到之后，再告诉 CPU：有一个硬件中断来了，并把对应的中断向量交给 CPU。

硬件设备
↓
IRQ
↓
8259A PIC
↓
中断向量 vector
↓
CPU
↓
IDT[vector]

        CPU
         ▲
         │
      主 8259A
   IRQ0 ... IRQ7
         │
         │ IRQ2
         ▼
      从 8259A
   IRQ8 ... IRQ15


0xFF = 11111111   8bit全1  对于PIC 的IMR bit=0 -> 屏蔽该IRQ

第一道门：PIC
IRQ1 没被 mask
某一条硬件中断开不开
第二道门：CPU
EFLAGS.IF = 1
CPU接不接受全局可屏蔽中断

                ┌───────────────┐
                │      GDT      │
                │ Ring0 / Ring3 │
                └───────┬───────┘
                        │
                        │ GDT[3]
                        ▼
                ┌───────────────┐
                │      TSS      │
                │ SS0 / ESP0    │
                └───────────────┘


CPU异常                 硬件IRQ                软件int
#PF                     键盘IRQ1               int 0x80
 │                         │                      │
 │                         ▼                      │
 │                     8259A PIC                 │
 │                         │                      │
 │                       0x21                     │
 │                         │                      │
 └──────────────┬──────────┴──────────────┬──────┘
                ▼                         ▼
              vector                    vector
                │
                ▼
              IDTR
                │
                ▼
           IDT[vector]
                │
       ┌────────┴─────────┐
       │                  │
  普通异常/IRQ         int 0x80
       │                  │
       ▼                  ▼
InterruptHandler_xx   AllTrapsEntry
       │                  │
       │             必要时TSS切栈
       │                  │
       ▼                  ▼
       保存CPU执行现场
                │
                ▼
          C Handler
                │
       ┌────────┴────────┐
       │                 │
   硬件IRQ            异常/系统调用
       │                 │
      EOI                │
       └────────┬────────┘
                ▼
           恢复现场
                │
                ▼
              iret
                │
                ▼
        回到被打断的程序


InterruptHandlerEntryTable
→ 汇编入口地址表，初始化 IDT 时使用

InterruptHandlerList
→ C Handler 地址表，真正处理中断时由汇编查询



## 1. Segment Selector 为什么要左移 3 位

一开始容易把：

```
Index << 3
```

理解成单纯的乘 8。

后来明确：

```
bit 0 ~ 1
→ RPL

bit 2
→ TI

bit 3 ~ 15
→ Descriptor Index
```

因此 Descriptor Index 必须左移 3 位，为 `TI + RPL` 留出低 3 bit。

例如：

```
0x23
→ Index = 4
→ TI = 0
→ RPL = 3
→ GDT[4]
→ User Code
```

------

## 2. CPL / DPL / RPL 容易混淆

学习过程中一开始容易把三者都理解成“权限等级”。

后来区分为：

```
CPL
→ CPU 当前正在以什么特权级运行

DPL
→ Descriptor / Interrupt Gate 本身规定的特权级

RPL
→ Segment Selector 中携带的请求特权级
```

其中在 IDT 中尤其需要注意：

```
IDT.DPL
```

主要决定软件执行 `int n` 时是否有资格主动进入该 Interrupt Gate。

------

## 3. IDT.DPL 和 Selector 不是一回事

一开始容易认为：

```
DPL = 3
```

就意味着中断处理程序也在 Ring3 运行。

实际上 `IDT[0x80]`：

```
DPL = 3
Selector = 0x08
```

分别表示：

```
DPL = 3
→ User 有资格主动执行 int 0x80

Selector = 0x08
→ 真正进入 Kernel Code Segment
```

可以记成：

> **DPL 决定谁能敲门，Selector 决定门后去哪里。**

------

## 4. CPU Exception 不受 IDT.DPL 的软件调用规则限制

一开始容易产生疑问：

> Page Fault 发生在 Ring3，但 `IDT[14].DPL = 0`，为什么还能进入内核？

后来明确：

```
IDT.DPL
```

主要限制的是：

```
int n
```

这种软件主动调用 Interrupt Gate 的行为。

Page Fault 是 CPU 自己检测并产生的 Exception，因此不会因为 User CPL=3、Gate DPL=0 就无法进入 Handler。

------

## 5. InterruptHandlerEntryTable 和 InterruptHandlerList 容易混淆

学习过程中多次容易把这两个 Table 当成同一层。

最终区分为：

```
InterruptHandlerEntryTable
→ 保存 Assembly Entry Address

InterruptHandlerList
→ 保存真正的 C Handler Address
```

初始化阶段：

```
InterruptHandlerEntryTable[14]
↓
InterruptHandler_0x0e 地址
↓
写入 IDT[14].Offset
```

运行阶段：

```
CPU
↓
IDT[14]
↓
直接进入 InterruptHandler_0x0e
↓
InterruptHandlerList[14]
↓
PageFaultHandler
```

需要特别注意：

> CPU 运行时不会在查完 IDT 后再去查询 `InterruptHandlerEntryTable`。

------

## 6. Entry Table 本身不会保存现场

学习答题时曾把：

```
InterruptHandlerEntryTable
```

说成负责现场保存和恢复。

后来修正：

```
InterruptHandlerEntryTable
```

只是保存 Assembly Entry Address。

真正执行：

```
push ds/es/fs/gs
pushad
call C Handler
popad
iret
```

的是：

```
InterruptHandler_0xXX
```

这些实际的 Assembly Entry。

------

## 7. CPU 自动保存和 Assembly 主动保存要区分

这是这一模块中比较容易混淆的地方。

跨特权级中断发生时，CPU 自动保存的是返回所需要的核心 Context，例如：

```
SS3
ESP3
EFLAGS
CS
EIP
```

某些 Exception 还会自动保存：

```
ErrorCode
```

而：

```
push ds
push es
push fs
push gs
pushad
```

是 GOS 自己的 Assembly Code 主动执行的。

因此：

```
CPU Saved Context
≠
GOS Software Saved Context
```

------

## 8. 为什么要补 0x88888888

一开始知道是为了“统一”，但没有完全理解统一的具体意义。

x86 只有部分 Exception 会自动压 ErrorCode，例如 Page Fault。

对于没有 ErrorCode 的情况，GOS：

```
push 0x88888888
```

补一个占位值。

这样后续 Stack Layout 始终保持一致。

例如保存：

```
4 Segment Registers
→ 16B

pushad
→ 32B
```

以后：

```
[esp + 48]
```

就可以始终访问统一的 ErrorCode Slot。

如果不补，占有 ErrorCode 和没有 ErrorCode 的 Interrupt 就需要分别处理不同 Stack Layout。

------

## 9. pushad 后的 Stack Order 容易看反

`pushad` 的压栈顺序和最终从当前 ESP 向高地址查看时的顺序不同。

执行完成后，从当前 ESP 向高地址看：

```
EDI
ESI
EBP
Saved ESP
EBX
EDX
ECX
EAX
```

因此在 `AllTrapsEntry` 中：

```
[esp + 8 * 4]
```

正好对应：

```
Saved EAX
```

不是简单因为“pushad 有八个寄存器”，而是因为 EAX 的 Slot 相对当前 ESP 正好位于 `32B` 偏移处。

------

## 10. System Call Return Value 为什么要写回 Saved EAX

`TrapHandler` 返回以后：

```
EAX
→ C Function Return Value
```

但紧接着需要：

```
popad
```

如果直接 `popad`，旧的 Saved EAX 会覆盖当前的 Return Value。

因此先：

```
mov dword [esp + 8*4], eax
```

把 Return Value 写入 Saved EAX Slot。

之后：

```
popad
```

恢复出的 EAX 就是：

```
System Call Return Value
```

用户程序返回后即可从 EAX 中取得结果。

------

## 11. TSS 不是保存完整 Process Context

一开始容易把 TSS 理解成：

> 保存当前进程整个运行现场。

当前 GOS 并没有使用 TSS 完成完整的 Hardware Task Switching。

在这个项目中最重要的是：

```
TSS.SS0
TSS.ESP0
```

主要用于：

```
Ring3
↓
Ring0
```

时找到 Kernel Stack。

------

## 12. 为什么 SS0 基本不变，而 ESP0 要不断更新

后来明确：

```
SS0
→ Kernel Data Segment
```

所有进程进入 Kernel 后基本都可以使用同一个 Kernel Data Segment。

但：

```
ESP0
→ Current User Process Kernel Stack Top
```

每个 Process 有自己的 Kernel Stack。

因此切换 User Process 时必须同步更新：

```
TSS.ESP0
```

否则新进程进入内核后可能错误地使用上一个进程的 Kernel Stack。

------

## 13. Kernel Stack 的页对齐方向容易弄反

学习过程中曾把：

```
(address + PageSize - 1) / PageSize * PageSize
```

说成计算 Page Start。

实际上这是：

```
Align Up
→ 向上按页对齐
```

得到的是该 Stack Page 的高地址边界。

原因是 x86 Stack：

```
High Address
↓
↓ grows
↓
Low Address
```

因此 Kernel Stack Top 位于高地址方向。

------

## 14. ESP3 / SS3 为什么必须保存

一开始理解为：

> Page Fault 不能在 User Stack 上处理，所以要保存 User Stack。

更准确的理解是：

> 只要发生 Ring3 → Ring0 的跨特权级 Interrupt / Exception，CPU 就需要切换到 Kernel Stack。

既然切走了，就必须记录原来的：

```
ESP3
SS3
```

这样之后：

```
iret
```

才能重新恢复 User Stack。

因此这并不是 Page Fault 特有的行为。

------

## 15. iret 和 ret 的区别

普通：

```
ret
```

主要恢复函数返回地址。

而：

```
iret
```

恢复的是 Interrupt Context。

同特权级时主要恢复：

```
EIP
CS
EFLAGS
```

Ring0 → Ring3 时还需要恢复：

```
ESP
SS
```

因此 `iret` 可以同时完成：

```
Kernel → User

Kernel Stack → User Stack
```

------

## 16. PIC Mask 和 CPU IF 是两道不同的开关

一开始容易把：

```
SetInterrupt()
sti
```

都理解成“开中断”。

实际上：

```
SetInterrupt(0x21)
→ PIC 层面解除 IRQ1 Mask
```

而：

```
sti
```

是：

```
CPU EFLAGS.IF = 1
```

全局允许 CPU 响应 Maskable Hardware Interrupt。

可以理解为两道门：

```
IRQ
↓
PIC Mask
↓
CPU IF
↓
Interrupt Handler
```

------

## 17. PIC Mask 中 0 和 1 的含义容易反

当前 PIC Mask：

```
bit = 1
→ Masked / Disabled

bit = 0
→ Enabled
```

因此：

```
ReadByte(0x21) & ~(1 << vector)
```

是在把对应 bit 清零，从而：

```
解除该 IRQ 的屏蔽
```

------

## 18. EFLAGS.IF 的 bit 编号

IF 位于：

```
EFLAGS bit 9
```

计算机 bit 通常从 0 编号，因此：

```
bit 9
```

如果按普通人的“第几位”从 1 开始数，则是：

```
第 10 位
```

对应：

```
0x200
= 1 << 9
```

------

## 19. 为什么 Slave PIC Interrupt 要发送两次 EOI

一开始只记住：

```
0x21 在 Master
0x2E 在 Slave
```

后来进一步明确原因：

Slave PIC 并不是直接连接 CPU，而是：

```
Slave PIC
↓
Master PIC IRQ2
↓
CPU
```

因此 Slave IRQ 完成后需要：

```
EOI → Slave PIC
EOI → Master PIC
```

而 Master 自己的 IRQ 只需要通知 Master。

------

## 20. IRQ 和 Vector 不是同一个概念

例如：

```
Keyboard
→ IRQ1
```

但 PIC Remapping 后交给 CPU 的是：

```
Vector 0x21
```

因此：

```
IRQ
→ Hardware Interrupt Request Number

Vector
→ CPU / IDT 使用的 Interrupt Number
```

当前项目中：

```
Vector = IRQ + 0x20
```

适用于 8259A 重映射后的 IRQ0 ~ IRQ15。

------

## 21. 三种 Interrupt Source 要分开

最终形成的区分：

```
Page Fault
→ CPU Exception
→ CPU 产生 Vector 14
→ 不经过 PIC
Keyboard
→ Hardware IRQ1
→ PIC 映射为 Vector 0x21
→ 经过 PIC
int 0x80
→ Software Interrupt
→ 软件主动产生 Vector 0x80
→ 不经过 PIC
```

虽然来源不同，但得到 Vector 后都最终进入：

```
IDT[Vector]
```

------

## 22. 当前最重要的整体认识

学习完本模块以后，中断不再理解成单独的：

```
IDT
```

或者：

```
Interrupt Handler
```

而是一整条协作链：

```
Interrupt Source
    ↓
Vector
    ↓
IDT
    ↓
Privilege Transition
    ↓
TSS / Kernel Stack
    ↓
CPU Saved Context
    ↓
Assembly Saved Context
    ↓
C Handler
    ↓
Restore Context
    ↓
iret
```

其中 Hardware IRQ 还额外包含：

```
Device
↓
8259A PIC
↓
EOI
```

这一整体链路是本次中断模块学习中最重要的收获。