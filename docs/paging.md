# GOS Paging

## 1. 模块概述

GOS 使用 Intel 80386 风格的 32 位二级分页机制。

Paging（分页）模块负责：

- 创建内核 Page Directory（页目录）；
- 创建 Page Table（二级页表）；
- 建立 Virtual Address（虚拟地址）到 Physical Address（物理地址）的映射；
- 使用 CR3（Control Register 3，3号控制寄存器）切换地址空间；
- 为用户进程提供独立页目录；
- 处理 Page Fault（缺页异常）；
- 实现基础 Demand Paging（按需分页）；
- 在修改映射后刷新 TLB（Translation Lookaside Buffer，地址转换后备缓冲器）。

主要代码位于：

```text
src/kernel/memory/memory_mapping.c
src/kernel/int/page.c
src/kernel/int/interrupt_handler.asm
src/kernel/int/interrupt_entry.c
src/kernel/kernel_main.c
```

---

# 2. 分页总体结构

GOS 使用二级分页。

一个 32 位 Virtual Address 被拆分为：

```text
31                    22 21                    12 11                0
+-----------------------+------------------------+-------------------+
|   PDE Index 10 bit    |   PTE Index 10 bit    | Offset 12 bit     |
+-----------------------+------------------------+-------------------+
```

其中：

```text
PDE Index
→ Page Directory Entry（页目录项）下标

PTE Index
→ Page Table Entry（页表项）下标

Offset
→ 4KB 页内部偏移
```

完整地址转换：

```text
Virtual Address
      │
      ▼
PDE Index
      │
      ▼
Page Directory
      │
      ▼
     PDE
      │
      ▼
 Page Table
      │
      ▼
PTE Index
      │
      ▼
     PTE
      │
      ▼
Physical Page
      │
      ▼
Physical Page Base + Offset
      │
      ▼
Physical Address
```

---

# 3. PDE、PTE 与页表大小

一个 PDE（Page Directory Entry，页目录项）：

```text
4 Byte
```

一个 PTE（Page Table Entry，页表项）：

```text
4 Byte
```

Page Directory 中有：

```text
1024 个 PDE
```

所以：

```text
1024 × 4 Byte
=
4096 Byte
=
4KB
```

一张 Page Table 同样包含：

```text
1024 个 PTE
```

因此：

```text
一张 Page Directory = 4KB
一张 Page Table     = 4KB
一个 PDE            = 4 Byte
一个 PTE            = 4 Byte
```

---

# 4. 地址空间覆盖范围

一个 PTE 映射一个：

```text
4KB Physical Page
```

一张 Page Table 有 1024 个 PTE：

```text
1024 × 4KB
=
4MB
```

所以：

```text
一个 PDE
→ 一张 Page Table
→ 管理 4MB Virtual Address Space
```

一张 Page Directory 有 1024 个 PDE：

```text
1024 × 4MB
=
4GB
```

因此二级分页可以覆盖整个 32 位地址空间。

---

# 5. PageTableEntry

GOS 使用同一个 `PageTableEntry` 结构描述 PDE 和 PTE。

主要字段：

```text
Present
Write
User
NextPPN
```

其中：

```text
Present = 1
→ 条目有效

Present = 0
→ 条目无效 / 映射不存在
```

```text
Write = 1
→ 允许写
```

```text
User = 1
→ 用户态允许访问
```

`NextPPN` 中保存 PPN（Physical Page Number，物理页号）。

作为 PDE 时：

```text
PDE.NextPPN
→ 二级 Page Table 的 PPN
```

作为 PTE 时：

```text
PTE.NextPPN
→ 最终 Physical Page 的 PPN
```

---

# 6. PPN 与物理地址

由于页面大小：

```text
4KB = 2^12 Byte
```

因此：

```text
PhysicalAddress = PPN << 12
```

例如：

```text
PPN = 0x120
```

对应：

```text
PhysicalAddress = 0x00120000
```

GOS 提供：

```c
GetAddressFromPPN()
GetPPNFromAddressFloor()
GetPPNFromAddressCeil()
```

用于 PPN 与 Physical Address 之间的转换。

---

# 7. 内核初始分页

分页初始化函数：

```c
InitializeMemoryMapping();
```

位于：

```text
src/kernel/memory/memory_mapping.c
```

初始化过程：

```text
AllocateOnePage(KernelMode)
        │
        ▼
创建 Page Directory
        │
        ▼
KernelRootPPN
        │
        ▼
AllocateOnePage(KernelMode)
        │
        ▼
创建第一张 Page Table
        │
        ▼
PDE[0] → Page Table 0
        │
        ▼
初始化 PTE
        │
        ▼
CR3 ← Page Directory
        │
        ▼
CR0.PG = 1
        │
        ▼
Paging Enabled
```

---

# 8. 内核 Identity Mapping

第一张 Page Table 采用：

```text
Virtual Page i
→ Physical Page i
```

即 Identity Mapping（恒等映射）。

因此低地址区域基本满足：

```text
Virtual Address
≈
Physical Address
```

第一张 Page Table 覆盖：

```text
0x00000000
~
0x003FFFFF
```

即低 4MB。

其中第 0 个 4KB 页面故意不映射：

```text
0x00000000
~
0x00000FFF
```

用于帮助发现 Null Pointer（空指针）访问。

---

# 9. CR0、CR2、CR3

GOS 分页主要涉及三个控制寄存器。

## 9.1 CR0

CR0（Control Register 0，0号控制寄存器）的 bit 31：

```text
PG（Paging，分页使能位）
```

```text
CR0.PG = 0
→ Paging Disabled

CR0.PG = 1
→ Paging Enabled
```

---

## 9.2 CR2

CR2（Control Register 2，2号控制寄存器）用于 Page Fault。

发生 Page Fault 后：

```text
CR2
→ 保存导致异常的 Virtual Address
```

可以记成：

```text
CR2：哪里出错了
```

---

## 9.3 CR3

CR3（Control Register 3，3号控制寄存器）保存：

```text
当前 Page Directory 的物理地址
```

因此：

```text
CR3
→ 当前地址空间的根
```

切换 CR3 即可切换不同 Page Directory。

---

# 10. 用户进程地址空间

不同用户进程拥有自己的 Page Directory。

例如：

```text
Process A
CR3 → Page Directory A

Process B
CR3 → Page Directory B
```

因此相同 Virtual Address：

```text
0x10000000
```

在两个进程中可以得到：

```text
Process A
Virtual 0x10000000
→ Physical Page A

Process B
Virtual 0x10000000
→ Physical Page B
```

这构成进程地址空间隔离的基础。

---

# 11. 用户栈映射

当前用户栈顶部：

```text
0x10000000
```

x86 栈向低地址增长，因此栈顶下方第一页：

```text
0x0FFFF000
~
0x0FFFFFFF
```

对应：

```text
PDE Index = 63
PTE Index = 1023
```

映射结构：

```text
User Page Directory
        │
        ▼
     PDE[63]
        │
        ▼
User Stack Page Table
        │
        ▼
    PTE[1023]
        │
        ▼
User Stack Physical Page
```

---

# 12. TLB

TLB（Translation Lookaside Buffer，地址转换后备缓冲器）用于缓存：

```text
Virtual Page
→ Physical Page
```

这样 CPU 不必每次都完整查询 PDE 和 PTE。

当页表发生变化以后，旧 TLB 记录可能失效。

GOS 提供：

```c
FlushTLB(VirtualAddress addr);
```

内部使用：

```asm
invlpg
```

使指定 Virtual Page 的 TLB 缓存失效。

---

# 13. Page Fault

Page Fault（缺页异常）对应 x86 异常：

```text
Vector = 0x0E
```

汇编入口：

```asm
InterruptHandlerMacro 0x0e, 1
```

第二个参数 `1` 表示：

```text
该异常由 CPU 自动压入 Error Code
```

---

# 14. Page Fault 注册

GOS 在：

```text
src/kernel/int/page.c
```

实现：

```c
InitializePageFaultHandler();
```

初始化时：

```text
InitializePageFaultHandler
        │
        ▼
SetInterruptHandler(0x0E, pageFaultHandler)
        │
        ▼
InterruptHandlerList[14]
        │
        ▼
pageFaultHandler
```

最终 14 号异常被连接到 Page Fault C Handler。

---

# 15. 中断汇编入口

异常进入：

```text
src/kernel/int/interrupt_handler.asm
```

首先保存执行现场：

```asm
push ds
push es
push fs
push gs
pushad
```

然后将：

```text
vector
errorCode
```

作为参数传递给 C Handler。

处理完成后：

```asm
popad
pop gs
pop fs
pop es
pop ds
```

恢复执行现场。

最后：

```asm
iret
```

返回异常发生位置。

---

# 16. Error Code 的统一处理

并不是所有异常都由 CPU 自动提供 Error Code。

GOS 定义：

```text
ErrCodeMagic = 0x88888888
```

对没有 CPU Error Code 的异常：

```text
push ErrCodeMagic
```

因此不同异常的栈结构保持统一。

---

# 17. Page Fault Error Code

CR2 告诉：

```text
哪里出错
```

Page Fault Error Code 告诉：

```text
为什么出错
```

当前重点使用低三位：

```text
bit 2       bit 1       bit 0
 U/S         W/R          P
```

其中：

```text
P = 0
→ Page Not Present

P = 1
→ Protection Fault
```

```text
W/R = 0
→ Read

W/R = 1
→ Write
```

```text
U/S = 0
→ Supervisor / Kernel

U/S = 1
→ User
```

---

# 18. 当前 Page Fault 策略

当前 Handler 逻辑：

```text
Page Fault
      │
      ▼
读取 CR2
      │
      ▼
检查 Error Code
      │
      ▼
P == 1 ?
  /       \
Yes       No
 │         │
 ▼         ▼
Protection  Page Not Present
Fault             │
 │                ▼
Panic         MapPage(addr)
```

目前：

```text
真正缺页
→ 自动建立映射

Protection Fault
→ Panic
```

这样可以避免权限错误不断重新触发 Page Fault。

---

# 19. MapPage

公开接口：

```c
MapPage(VirtualAddress addr);
```

位于：

```text
src/kernel/memory/memory_mapping.c
```

当前流程：

```text
MapPage(addr)
      │
      ▼
disablePaging()
      │
      ▼
findPTECreate(addr)
      │
      ▼
enablePaging()
      │
      ▼
FlushTLB(addr)
```

---

# 20. findPTE

`findPTE(addr)` 负责：

```text
Virtual Address
      │
      ▼
计算 PDE Index
      │
      ▼
找到 PDE
      │
      ▼
PDE 不存在？
 ├─ Yes → NULL
 └─ No
      │
      ▼
进入 Page Table
      │
      ▼
找到对应 PTE
```

该函数只负责查找，不主动创建映射。

---

# 21. findPTECreate

`findPTECreate(addr)` 负责：

```text
Virtual Address
       │
       ▼
计算 PDE / PTE Index
       │
       ▼
找到 PDE
       │
       ▼
PDE Present == 0 ?
   │
   ├── Yes
   │     │
   │     ▼
   │ AllocateOnePage(UserMode)
   │     │
   │     ▼
   │ 创建 Page Table
   │
   ▼
进入 Page Table
       │
       ▼
找到 PTE
       │
       ▼
PTE Present == 0 ?
   │
   ├── Yes
   │     │
   │     ▼
   │ AllocateOnePage(UserMode)
   │     │
   │     ▼
   │ 创建 Physical Page
   │
   ▼
Mapping Ready
```

因此它可能调用两次：

```c
AllocateOnePage(UserMode);
```

第一次用于：

```text
Page Table
```

第二次用于：

```text
真正的用户 Physical Page
```

---

# 22. Demand Paging

Demand Paging（按需分页）的核心思想：

```text
真正访问的时候再分配
```

当前 GOS：

```text
程序访问 Virtual Address
        │
        ▼
页面不存在
        │
        ▼
Page Fault
        │
        ▼
缺 PDE？
→ 创建 Page Table
        │
        ▼
缺 PTE？
→ Allocate Physical Page
        │
        ▼
建立映射
        │
        ▼
返回原程序
```

因此可以简化成：

```text
缺什么
→ 补什么
```

---

# 23. 完整 Page Fault 流程

```text
             CPU Access Virtual Address
                        │
                        ▼
                   Page Missing
                        │
                        ▼
                 Page Fault #14
                        │
            ┌───────────┴───────────┐
            │                       │
            ▼                       ▼
     CR2 = Fault Addr         Error Code
            │                 Fault Reason
            │                       │
            └───────────┬───────────┘
                        ▼
              InterruptHandler_0x0e
                        │
                        ▼
                  Save Registers
                        │
                        ▼
        pageFaultHandler(vector,errorCode)
                        │
                        ▼
                  Check P Bit
                  /          \
               P=1            P=0
                │              │
                ▼              ▼
        Protection Fault    Not Present
                │              │
              Panic            ▼
                            MapPage
                               │
                               ▼
                        findPTECreate
                         /          \
                   Missing PDE    Missing PTE
                       │              │
                       ▼              ▼
                Create Page     Allocate Page
                    Table
                       \              /
                        └──────┬──────┘
                               ▼
                          Mapping Ready
                               │
                               ▼
                           Flush TLB
                               │
                               ▼
                       Restore Registers
                               │
                               ▼
                              iret
                               │
                               ▼
                       Retry Instruction
                               │
                               ▼
                            Success
```

---

# 24. 运行时验证

测试代码曾临时访问：

```c
volatile u32* test = (u32*)0x00800000;
*test = 1234;
```

因为：

```text
0x00800000 = 8MB
```

而初始内核映射主要覆盖低 4MB，因此第一次访问产生 Page Fault。

实际输出：

```text
Before page fault test
Page Fault: addr=0x800000 error=0x2
After page fault test: 1234
```

其中：

```text
errorCode = 0x2 = 010b
```

表示：

```text
P   = 0
→ 页面不存在

W/R = 1
→ 写操作

U/S = 0
→ 内核态
```

与测试行为一致。

---

# 25. GDB 验证

通过 GDB（GNU Debugger，GNU 调试器）设置：

```gdb
break pageFaultHandler
break MapPage
```

实际观察：

```text
CR2 = 0x00800000
CR3 = 0x00112000
MapPage addr = 0x00800000
```

说明：

```text
CPU Fault Address
        │
        ▼
CR2 = 0x00800000
        │
        ▼
pageFaultHandler
        │
        ▼
MapPage(addr = 0x00800000)
```

整条 Page Fault 地址传递正确。

最后：

```text
After page fault test: 1234
```

证明映射建立以后：

```text
iret
→ 重试原来的访存指令
→ 成功
```

---

# 26. 当前能力

目前 GOS Paging 已具备：

```text
32 位二级分页
Page Directory
Page Table
Kernel Identity Mapping
独立用户 Page Directory 基础
CR3 地址空间切换
TLB 刷新
Page Fault
Page Fault Error Code
基础 Demand Paging
```

---

# 27. 当前未实现

当前仍未实现：

```text
Copy-On-Write（写时复制）
Swap（交换空间）
Page Replacement（页面置换）
File Mapping（文件映射）
完整 Virtual Memory Area 管理
完整 Unmap / Address Space Destroy
用户进程级异常终止
```

这些属于后续虚拟内存和进程管理阶段。