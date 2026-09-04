## 1. Module Overview

当前 GOS 的中断模块主要完成：

- 建立 Kernel / User 的特权级环境。
- 建立 Interrupt Vector 到中断入口的映射。
- 处理 CPU Exception。
- 处理 8259A PIC 转发的 Hardware IRQ。
- 提供 `int 0x80` 用户态系统调用入口。
- 在中断发生时保存 CPU Context。
- 支持 Ring3 → Ring0 时切换到当前进程的 Kernel Stack。
- 中断处理完成后通过 `iret` 恢复原执行环境。

整体可以理解为：

```
Interrupt Source
    ↓
Vector
    ↓
IDT
    ↓
Assembly Entry
    ↓
Save Context
    ↓
C Handler
    ↓
Restore Context
    ↓
iret
```

------

## 2. GDT

GDT：

**Global Descriptor Table（全局描述符表）**

当前 GOS 使用的主要描述符为：

```
GDT[0]
→ Null Descriptor

GDT[1]
→ Kernel Code

GDT[2]
→ Kernel Data

GDT[3]
→ TSS

GDT[4]
→ User Code

GDT[5]
→ User Data
```

对应 Selector：

```
KernelCodeSegmentSelector
→ 0x08

KernelDataSegmentSelector
→ 0x10

TSSSegmentSelector
→ 0x18

UserCodeSegmentSelector
→ 0x23

UserDataSegmentSelector
→ 0x2B
```

这些定义位于 `src/kernel/gdt/type.h`。

------

## 3. Segment Selector

Segment Selector 的低 3 bit 用于：

```
bit 0 ~ 1
→ RPL

bit 2
→ TI

bit 3 ~ 15
→ Descriptor Index
```

因此：

```
Selector = Index << 3
```

之后再根据需要设置 RPL。

例如：

```
0x23

Index = 4
TI    = 0
RPL   = 3

→ GDT[4]
→ User Code Segment
```

------

## 4. CPL / DPL / RPL

### CPL

**Current Privilege Level（当前特权级）**

表示 CPU 当前正在执行代码的特权级。

```
CPL = 0
→ Kernel

CPL = 3
→ User
```

### DPL

**Descriptor Privilege Level（描述符特权级）**

记录在 Descriptor 中，用于描述该段或 Gate 的权限。

### RPL

**Requested Privilege Level（请求特权级）**

记录在 Segment Selector 的低两位中。

------

## 5. Flat Segmentation

当前 GOS 的 Kernel / User Code Segment 和 Data Segment 都使用接近平坦的地址空间：

```
Base
→ 0

Limit
→ 接近 4GB
```

因此当前 GOS 并不主要依赖 Segmentation 完成进程内存隔离。

真正的虚拟内存隔离主要由 Paging 完成。

GDT 在这里的重要作用主要是：

```
Kernel / User Privilege
Ring0 / Ring3
Code / Data Segment
TSS
```

------

## 6. TSS

TSS：

**Task State Segment（任务状态段）**

GOS 当前没有使用 x86 TSS 完成完整的 Hardware Task Switching。

当前最重要的字段为：

```
SS0
ESP0
```

初始化时：

```
TSS.SS0
→ KernelDataSegmentSelector
→ 0x10
```

并通过 `GDT[3]` 建立 TSS Descriptor。

随后执行：

```
ltr
```

将 `TSSSegmentSelector` 加载到 Task Register。



因此 CPU 可以通过：

```
TR
↓
GDT[3]
↓
TSS
```

找到当前 TSS。

------

## 7. TSS.ESP0

当 CPU 从：

```
Ring3
↓
Ring0
```

发生跨特权级中断时，需要从 User Stack 切换到 Kernel Stack。

CPU 会从 TSS 中读取：

```
SS0
ESP0
```

作为新的 Kernel Stack。

其中：

```
SS0
```

基本固定为 Kernel Data Segment。

而：

```
ESP0
```

必须随着当前 User Process 改变。

因为每个 Process 都拥有自己的 Kernel Stack。

------

## 8. Update ESP0 During Scheduling

`src/kernel/process/process.c` 在切换到 User Process 前：

```
if (next->Type == PROCESS_TYPE_USER) {
    SetTSSEsp0(
        ((u32)next->KernelStackPointer + PageSize - 1)
        / PageSize * PageSize
    );
}
```



这里将：

```
next->KernelStackPointer
```

向上按 4KB Page 对齐，得到当前 Kernel Stack 的高地址边界。

由于 x86 Stack：

```
High Address
↓
↓ Stack grows
↓
Low Address
```

因此高地址边界作为 Kernel Stack Top。

最终：

```
TSS.ESP0
→ Current User Process Kernel Stack Top
```

------

## 9. CR3 And ESP0

Process Scheduling 时还可能更新：

```
CR3
```

当前两者职责不同：

```
TSS.ESP0
→ 该进程从 Ring3 进入 Ring0 时使用哪个 Kernel Stack

CR3
→ 当前进程使用哪一套 Page Table
```

`process.c` 会根据 `RootPPN` 判断是否需要切换 Root Page Table。

------

## 10. IDT

IDT：

**Interrupt Descriptor Table（中断描述符表）**

当前定义：

```
INTERRUPT_COUNT
→ 256
```

每个 Interrupt Descriptor 主要记录：

```
Offset
Selector
Type
DPL
Present
```

中断发生后：

```
Vector
↓
IDTR
↓
IDT[Vector]
↓
Selector + Offset
↓
Interrupt Entry
```

------

## 11. Interrupt Gate

普通 Exception / IRQ 的 IDT Entry 使用：

```
Type = 1110
```

即：

```
32-bit Interrupt Gate
```

目标 Selector：

```
0x08
→ GDT[1]
→ Kernel Code
```

因此进入中断 Handler 后执行的是 Kernel Code。

------

## 12. IDT DPL

普通 Exception / Hardware IRQ：

```
DPL = 0
```

而：

```
IDT[0x80]
```

使用：

```
DPL = 3
```

因此 User Code 可以主动执行：

```
int 0x80
```

但：

```
Selector = 0x08
```

仍然表示进入 Kernel Code Segment。

可以理解为：

```
IDT.DPL
→ Who can enter this gate

Selector
→ Where the gate leads
```

------

## 13. Interrupt Sources

当前主要有三种 Interrupt Source。

### CPU Exception

例如：

```
Page Fault
```

由 CPU 自己检测并产生：

```
#PF
→ Vector 14
→ IDT[14]
```

不经过 PIC。

### Hardware IRQ

例如：

```
Keyboard
→ IRQ1
→ 8259A PIC
→ Vector 0x21
→ CPU
```

需要经过 PIC。

### Software Interrupt

例如：

```
int 0x80
```

由软件主动执行 `INT` 指令产生：

```
Vector 0x80
→ IDT[0x80]
```

不经过 PIC。

------

## 14. 8259A PIC

PIC：

**Programmable Interrupt Controller（可编程中断控制器）**

当前 GOS 使用 Master / Slave 两片 8259A：

```
Master PIC
→ IRQ0 ~ IRQ7

Slave PIC
→ IRQ8 ~ IRQ15
```

Slave PIC 连接在 Master PIC 的 IRQ2 上。

------

## 15. IRQ Remapping

PIC 初始化时重新映射 IRQ：

```
IRQ0 ~ IRQ7
→ 0x20 ~ 0x27

IRQ8 ~ IRQ15
→ 0x28 ~ 0x2F
```



原因是：

```
0x00 ~ 0x1F
```

已经被 x86 CPU Exception 使用。

因此重新映射以后：

```
0x00 ~ 0x1F
→ CPU Exception

0x20 ~ 0x2F
→ Hardware IRQ

0x80
→ System Call
```

------

## 16. PIC Interrupt Mask

PIC 初始化结束后：

```
Master Mask = 0xFF
Slave Mask  = 0xFF
```

即默认屏蔽所有 Hardware IRQ。

PIC Mask 中：

```
bit = 1
→ IRQ masked

bit = 0
→ IRQ enabled
```

------

## 17. SetInterrupt()

`SetInterrupt(vector)` 用于解除指定 IRQ 的屏蔽。

例如：

```
SetInterrupt(0x21)
```

首先：

```
0x21 - 0x20
→ IRQ1
```

然后清除 Master PIC Mask 的 bit 1：

```
bit1
1 → 0
```

从而允许 Keyboard IRQ1 通过 PIC。

------

## 18. PIC Mask And IF

Hardware IRQ 是否能够真正进入 CPU，需要经过两层控制。

```
Device IRQ
↓
PIC Mask
↓
CPU EFLAGS.IF
↓
CPU Interrupt Handler
```

### PIC Mask

控制：

```
某一条 IRQ
```

是否能够通过 PIC。

### EFLAGS.IF

控制：

```
CPU 是否全局响应 Maskable Hardware Interrupt
```

------

## 19. sti / cli

`sti`：

**Set Interrupt Flag**

```
EFLAGS.IF = 1
```

允许 CPU 接收 Maskable Hardware Interrupt。

`cli`：

**Clear Interrupt Flag**

```
EFLAGS.IF = 0
```

暂时禁止 CPU 接收 Maskable Hardware Interrupt。

当前代码还提供：

```
GetInterruptStatus()
RestoreInterruptStatus()
```

用于读取和恢复 IF 状态。

------

## 20. EOI

EOI：

**End Of Interrupt（中断结束通知）**

当前 PIC 没有使用 Automatic EOI。

因此 Hardware IRQ 处理完成后，需要主动通知 PIC。

`OuteralInterruptCompleted(vector)`：

```
Vector 0x20 ~ 0x27
→ Send EOI to Master PIC

Vector 0x28 ~ 0x2F
→ Send EOI to Slave PIC
→ Send EOI to Master PIC
```



Slave IRQ 需要通知两片 PIC，是因为路径为：

```
Slave PIC
↓
Master PIC IRQ2
↓
CPU
```

------

## 21. InterruptHandlerEntryTable

`src/kernel/int/interrupt_handler.asm` 中定义：

```
InterruptHandlerEntryTable
```

其中保存：

```
InterruptHandler_0x00
InterruptHandler_0x01
...
InterruptHandler_0x2F
```

这些 Assembly Interrupt Entry 的地址。

它主要用于 IDT 初始化：

```
InterruptHandlerEntryTable[Vector]
↓
Assembly Entry Address
↓
Write into IDT[Vector].Offset
```

运行时 CPU 不会再次查询该 Table。

------

## 22. InterruptHandlerList

`src/kernel/int/interrupt_entry.c` 中：

```
InterruptHandlerList
```

保存真正的 C Handler Address。

因此普通中断路径为：

```
IDT[Vector]
↓
InterruptHandler_0xXX
↓
InterruptHandlerList[Vector]
↓
C Handler
```

------

## 23. Interrupt Assembly Entry

普通 Exception / IRQ 通过：

```
InterruptHandlerMacro
```

生成 Assembly Entry。

主要工作为：

```
Normalize ErrorCode
↓
Save Segment Registers
↓
pushad
↓
Pass Vector + ErrorCode
↓
Call C Handler
↓
Restore Registers
↓
iret
```



------

## 24. ErrorCode Normalization

部分 x86 Exception 会自动压入 ErrorCode。

例如：

```
Page Fault
```

而普通 IRQ 等没有 CPU ErrorCode。

因此当前 GOS 对没有 ErrorCode 的情况：

```
push 0x88888888
```

使用 Fake ErrorCode 占位。

这样后续 Stack Layout 可以保持统一。

------

## 25. Why esp + 48

Assembly Entry 中：

```
push ds
push es
push fs
push gs
```

共：

```
4 × 4B
→ 16B
```

`pushad`：

```
8 × 4B
→ 32B
```

总计：

```
16B + 32B
→ 48B
```

因此：

```
[esp + 48]
```

正好能够访问统一后的 ErrorCode。

------

## 26. CPU Saved Context

如果 Interrupt 发生时没有跨特权级，CPU 会保存返回所需的核心现场：

```
EIP
CS
EFLAGS
```

如果发生：

```
Ring3
↓
Ring0
```

CPU 还需要通过 TSS 切换 Kernel Stack，并保存原 User Stack：

```
SS3
ESP3
EFLAGS
CS
EIP
```

对于拥有 CPU ErrorCode 的 Exception，还会继续压入 ErrorCode。

------

## 27. Software Saved Context

进入 GOS Assembly Entry 后，再由软件主动执行：

```
push ds
push es
push fs
push gs
pushad
```

因此需要区分：

```
CPU Automatically Saved Context
```

和：

```
GOS Assembly Manually Saved Context
```

`ds/es/fs/gs` 和 `pushad` 并不是 CPU 自动完成的。

------

## 28. InterruptContext

`src/kernel/process/type.h` 中定义：

```
InterruptContext
```

主要布局为：

```
Vector
EDI
ESI
EBP
ESP
EBX
EDX
ECX
EAX
GS
FS
ES
DS
ErrCode
EIP
CS
PSW
ESP3
SS3
```



该顺序与 Trap Path 构造出的实际 Stack Layout 对应。

因此：

```
InterruptContext Field Order
```

不能随意修改。

否则 C 代码读取的字段会与真实 Stack Data 错位。

------

## 29. int 0x80

User Process 执行：

```
int 0x80
```

之后：

```
Vector 0x80
↓
IDT[0x80]
↓
DPL = 3
↓
User is allowed to enter
↓
Selector = 0x08
↓
Kernel Code
```

如果当前处于 Ring3：

```
CPU reads TSS.SS0 / ESP0
↓
Switch to Kernel Stack
```

然后进入：

```
AllTrapsEntry
```

------

## 30. AllTrapsEntry

`src/kernel/int/trap_handler.asm`：

```
push 0x88888888
push ds
push es
push fs
push gs
pushad
push 0x80
```



最终形成与 `InterruptContext` 对应的 Context Layout。

系统调用参数约定为：

```
EAX
→ Syscall Number

EBX
→ Argument 1

ECX
→ Argument 2

EDX
→ Argument 3
```

随后调用：

```
TrapHandler
```



------

## 31. System Call Return Value

C 函数返回值位于：

```
EAX
```

但是之后需要执行：

```
popad
```

因此当前 GOS 先执行：

```
mov dword [esp + 8*4], eax
```



将当前 EAX 中的返回值写入：

```
Saved EAX Slot
```

之后：

```
popad
```

恢复出的 EAX 就是 System Call Return Value。

最终 User Program 可以从 EAX 中得到返回值。

------

## 32. RestoreContext

`RestoreContext`：

```
Remove Vector
↓
popad
↓
Restore gs/fs/es/ds
↓
Remove ErrorCode
↓
iret
```



------

## 33. iret

`iret`：

**Interrupt Return（中断返回）**

同特权级返回时主要恢复：

```
EIP
CS
EFLAGS
```

如果：

```
Ring0
↓
Ring3
```

还会继续恢复：

```
ESP3
SS3
```

因此能够完成：

```
Kernel Stack
↓
User Stack

Ring0
↓
Ring3
```

------

## 34. Page Fault Flow

当前已经确认的 Page Fault 中断链路：

```
User Memory Access
    ↓
Paging Translation Failure
    ↓
CPU generates #PF
    ↓
Vector = 14
    ↓
If Ring3 → Ring0
    ↓
TSS.SS0 / ESP0
    ↓
Switch to Kernel Stack
    ↓
CPU saves return context
    ↓
CPU pushes Page Fault ErrorCode
    ↓
IDTR
    ↓
IDT[14]
    ↓
InterruptHandler_0x0e
    ↓
Save ds/es/fs/gs
    ↓
pushad
    ↓
InterruptHandlerList[14]
    ↓
PageFaultHandler
    ↓
CR2
    ↓
MapPage()
    ↓
Create Missing Mapping
    ↓
Return
    ↓
Restore Context
    ↓
iret
    ↓
Continue User Program
```

------

## 35. Keyboard Interrupt Flow

当前 Keyboard IRQ 链路：

```
Keyboard
    ↓
IRQ1
    ↓
Master 8259A PIC
    ↓
IRQ1 → Vector 0x21
    ↓
CPU
    ↓
IDTR
    ↓
IDT[0x21]
    ↓
InterruptHandler_0x21
    ↓
Fake ErrorCode
    ↓
Save Context
    ↓
InterruptHandlerList[0x21]
    ↓
Keyboard Handler
    ↓
EOI
    ↓
Restore Context
    ↓
iret
```

------

## 36. System Call Flow

当前 `int 0x80` 基本链路：

```
User Process
    ↓
int 0x80
    ↓
IDT[0x80]
    ↓
DPL = 3
    ↓
Selector = Kernel Code
    ↓
Ring3 → Ring0
    ↓
TSS.SS0 / ESP0
    ↓
Kernel Stack
    ↓
AllTrapsEntry
    ↓
InterruptContext
    ↓
TrapHandler
    ↓
Save Return Value into Saved EAX
    ↓
RestoreContext
    ↓
iret
    ↓
Ring0 → Ring3
    ↓
User Process
```

------



## 37. Interrupt Architecture

当前 GOS 中断模块整体可以总结为：

```
                    GDT
                     ↓
               Privilege Level
                     ↓
                    TSS
                 SS0 / ESP0


CPU Exception       Hardware IRQ       Software INT
     ↓                   ↓                  ↓
   Vector              8259A              Vector
     ↓                   ↓                  ↓
     └─────────────── Vector ──────────────┘
                         ↓
                        IDT
                         ↓
                 Assembly Entry
                         ↓
                  Save Context
                         ↓
                InterruptHandlerList
                         ↓
                     C Handler
                         ↓
              Hardware IRQ → EOI
                         ↓
                 Restore Context
                         ↓
                       iret
                         ↓
                Previous Execution
```

------

## 38. Key Takeaways

当前中断与特权级模块最核心的职责划分为：

```
GDT
→ Kernel / User Privilege Environment

TSS
→ Ring3 → Ring0 Kernel Stack Switching

IDT
→ Vector → Interrupt Entry

8259A PIC
→ Hardware IRQ → Vector

Assembly Entry
→ Save / Restore CPU Context

InterruptHandlerList
→ Dispatch to C Handler

C Handler
→ Actual Interrupt Logic

EOI
→ Finish Hardware IRQ

iret
→ Restore Previous Execution Context
```

因此整个模块的核心思想不是某一个单独的数据结构，而是：

> **不同来源的事件先被转换成 Interrupt Vector，再由 IDT 统一进入内核；汇编层负责保存机器现场，C 层负责处理具体逻辑，最后通过 `iret` 恢复到中断发生之前的执行状态。**