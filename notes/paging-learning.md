# GOS Paging 学习笔记

> 本文记录我对 GOS 分页机制、用户地址空间、Page Fault（缺页异常）和 Demand Paging（按需分页）的学习过程与理解。
>
> 重点不是单纯记录代码，而是弄清楚：
>
> - 为什么需要分页；
> - 虚拟地址是如何转换成物理地址的；
> - PDE / PTE 分别是什么；
> - CR0 / CR2 / CR3 分别负责什么；
> - 用户进程为什么可以拥有自己的地址空间；
> - Page Fault 是怎样发生、处理并返回的；
> - GOS 当前的 Demand Paging 是怎样实现的。

---

# 1. Frame Allocator 和 Paging 的区别

物理内存管理和分页不是同一件事。

## 1.1 Frame Allocator

Frame Allocator（页框分配器）解决：

> 哪些物理页空闲，哪些物理页已经被使用？

例如：

```text
AllocateOnePage(UserMode)
        ↓
找到一个空闲的 4KB Physical Page
        ↓
返回这个物理页的地址
```

它管理的是：

```text
Physical Memory
```

也就是机器真正存在的内存。

---

## 1.2 Paging

Paging（分页）解决：

> 一个程序使用的 Virtual Address（虚拟地址），究竟应该映射到哪个 Physical Address（物理地址）？

流程：

```text
Virtual Address
        ↓
Paging
        ↓
Physical Address
```

因此可以简单记成：

```text
Frame Allocator
→ 管理“物理内存有没有空位”

Paging
→ 管理“虚拟地址应该去哪里”
```

整体关系：

```text
             Physical Memory
                    │
                    ▼
             Frame Allocator
                    │
             AllocateOnePage
                    │
                    ▼
              Physical Page
                    ▲
                    │
                  Paging
                    │
                    ▲
              Virtual Address
```

---

# 2. 为什么需要虚拟地址

程序运行时使用的地址，不一定直接等于真实物理地址。

程序看到的是：

```text
Virtual Address
```

CPU 最终访问的是：

```text
Physical Address
```

中间通过页表完成转换：

```text
Virtual Address
        ↓
Page Directory
        ↓
Page Table
        ↓
Physical Page
        ↓
Physical Address
```

这样做以后，不同进程可以使用相同的虚拟地址，但是映射到不同物理页。

例如：

```text
Process A
Virtual 0x10000000
        ↓
Physical 0x00600000
```

另一个进程：

```text
Process B
Virtual 0x10000000
        ↓
Physical 0x00900000
```

所以：

> 虚拟地址相同，不代表实际访问的是同一块物理内存。

---

# 3. 32 位虚拟地址如何拆分

GOS 当前运行在 32 位 x86 环境。

一个虚拟地址：

```text
32 bit
```

被拆成：

```text
31                    22 21                    12 11                0
+-----------------------+------------------------+-------------------+
|   PDE Index 10 bit    |   PTE Index 10 bit    | Offset 12 bit     |
+-----------------------+------------------------+-------------------+
```

分别表示：

```text
前 10 bit
→ PDE（Page Directory Entry，页目录项）下标

中间 10 bit
→ PTE（Page Table Entry，页表项）下标

最后 12 bit
→ Offset（页内偏移）
```

---

# 4. 为什么 Offset 是 12 bit

GOS 的页面大小：

```text
4KB
```

也就是：

```text
4KB
= 4096 Byte
= 2^12 Byte
```

因此，如果已经找到某个 4KB Physical Page，还需要 12 bit 表示：

> 在这一页内部访问哪个字节。

所以：

```text
低 12 bit
=
Offset
```

例如：

```text
Virtual Address = 0x08001234
```

其中：

```text
0x234
```

就是页内 Offset。

它不会被写入 PTE。

PTE 只负责找到：

```text
Physical Page Base
```

最后 CPU 再计算：

```text
Physical Page Base + Offset
```

得到最终物理地址。

---

# 5. PDE 和 PTE 到底多大

学习过程中最容易混淆的一点：

> 一个 PDE 不是 4KB，一个 PTE 也不是 4KB。

实际上：

```text
一个 PDE = 4 Byte
一个 PTE = 4 Byte
```

真正占 4KB 的是整张表。

---

## 5.1 Page Directory

Page Directory（页目录）包含：

```text
1024 个 PDE
```

因此：

```text
1024 × 4 Byte
=
4096 Byte
=
4KB
```

所以：

```text
一张 Page Directory = 4KB
```

---

## 5.2 Page Table

一张 Page Table（二级页表）包含：

```text
1024 个 PTE
```

因此：

```text
1024 × 4 Byte
=
4096 Byte
=
4KB
```

所以：

```text
一张 Page Table = 4KB
```

最终记忆：

```text
PDE             = 4 Byte
PTE             = 4 Byte
Page Directory  = 4KB
Page Table      = 4KB
```

---

# 6. 两级分页的完整结构

整个地址转换过程：

```text
                   CR3
                    │
                    ▼
          +--------------------+
          |   Page Directory   |
          |      4KB           |
          +--------------------+
                    │
              PDE[firstIndex]
                    │
                    ▼
          +--------------------+
          |     Page Table     |
          |       4KB          |
          +--------------------+
                    │
             PTE[secondIndex]
                    │
                    ▼
          +--------------------+
          |   Physical Page    |
          |       4KB          |
          +--------------------+
                    │
                 + Offset
                    │
                    ▼
             Physical Address
```

因此：

```text
PDE
→ 找 Page Table

PTE
→ 找真正 Physical Page
```

这是最核心的区别。

---

# 7. 一个 PDE 能管理多少地址

一条 PTE 映射：

```text
4KB
```

一张 Page Table 有：

```text
1024 个 PTE
```

因此：

```text
1024 × 4KB
=
4MB
```

所以：

```text
一个 PDE
→ 指向一张 Page Table
→ 管理 4MB Virtual Address Space
```

Page Directory 又有：

```text
1024 个 PDE
```

于是：

```text
1024 × 4MB
=
4GB
```

正好覆盖一个 32 位地址空间：

```text
0x00000000
~
0xFFFFFFFF
```

---

# 8. PageTableEntry 结构

GOS 使用 `PageTableEntry` 同时描述 PDE 和 PTE。

它总共：

```text
32 bit = 4 Byte
```

可以粗略理解为：

```text
31                              12 11                  0
+--------------------------------+----------------------+
|        NextPPN 20 bit          |       Flags          |
+--------------------------------+----------------------+
```

重要字段包括：

```text
Present
Write
User
NextPPN
```

---

## 8.1 Present

```text
Present = 1
→ 当前条目有效

Present = 0
→ 当前条目不存在 / 没有映射
```

---

## 8.2 Write

```text
Write = 1
→ 允许写

Write = 0
→ 不允许写
```

---

## 8.3 User

```text
User = 1
→ 用户态允许访问

User = 0
→ 仅 Supervisor（内核态）访问
```

---

## 8.4 NextPPN

PPN（Physical Page Number，物理页号）。

当结构作为 PDE 使用：

```text
PDE.NextPPN
→ 下一层 Page Table 的物理页号
```

当结构作为 PTE 使用：

```text
PTE.NextPPN
→ 最终 Physical Page 的物理页号
```

因此：

```text
同一个字段
在 PDE 和 PTE 中含义不同
```

---

# 9. PPN 和物理地址的关系

PPN 不是完整物理地址。

因为一个页：

```text
4KB = 2^12
```

所以物理页起始地址：

```text
Physical Address
=
PPN << 12
```

例如：

```text
PPN = 0x120
```

那么：

```text
Physical Address
=
0x120 << 12
=
0x00120000
```

GOS 中：

```c
GetAddressFromPPN(ppn)
```

负责：

```text
PPN
→ Physical Address
```

反过来：

```c
GetPPNFromAddressFloor(address)
```

负责：

```text
Physical Address
→ PPN
```

---

# 10. GOS 初始内核分页

主要文件：

```text
src/kernel/memory/memory_mapping.c
```

初始化函数：

```c
InitializeMemoryMapping();
```

总体流程：

```text
AllocateOnePage(KernelMode)
        ↓
得到 4KB Physical Page
        ↓
作为 Page Directory
        ↓
KernelRootPPN 保存它的 PPN

        ↓

AllocateOnePage(KernelMode)
        ↓
再得到 4KB Physical Page
        ↓
作为第一张 Page Table

        ↓

PDE[0]
→ 第一张 Page Table

        ↓

初始化 Page Table

        ↓

CR3
→ Page Directory

        ↓

CR0.PG = 1
→ 开启 Paging
```

---

# 11. 内核初始 Identity Mapping

GOS 初始化第一张 Page Table 时：

```text
PTE[i]
→ Physical Page i
```

因此：

```text
Virtual Page i
→ Physical Page i
```

这称为：

```text
Identity Mapping（恒等映射）
```

也就是：

```text
Virtual Address ≈ Physical Address
```

例如：

```text
Virtual  0x00123000
        ↓
Physical 0x00123000
```

---

## 11.1 为什么只映射低 4MB

一张 Page Table 可以映射：

```text
4MB
```

当前初始化只创建第一张 Page Table：

```text
PDE[0]
→ Page Table 0
```

因此初始主要覆盖：

```text
0x00000000
~
0x003FFFFF
```

也就是低 4MB。

---

## 11.2 为什么第 0 页不映射

初始化代码会跳过：

```text
PTE[0]
```

所以：

```text
0x00000000
~
0x00000FFF
```

没有映射。

目的：

> 让 Null Pointer（空指针）附近的非法访问更容易被发现，而不是静默访问物理地址 0。

---

# 12. CR3：当前地址空间的根

CR3（Control Register 3，3号控制寄存器）保存：

> 当前 Page Directory 的物理地址。

可以理解成：

```text
CR3
 ↓
当前 Page Directory
 ↓
当前虚拟地址空间
```

因此修改 CR3：

```text
实际上就是切换页目录
```

而切换页目录：

```text
实际上就是切换地址空间
```

---

# 13. 为什么不同进程可以使用相同虚拟地址

假设：

```text
Process A RootPPN = A
Process B RootPPN = B
```

运行 A：

```text
CR3
↓
A 的 Page Directory
```

于是：

```text
Virtual 0x10000000
→ Physical Page A
```

切换到 B：

```text
CR3
↓
B 的 Page Directory
```

同一个：

```text
Virtual 0x10000000
```

可能变成：

```text
Virtual 0x10000000
→ Physical Page B
```

所以：

```text
相同 Virtual Address
+
不同 Page Directory
=
不同 Physical Address
```

---

# 14. CR0：开启和关闭分页

CR0（Control Register 0，0号控制寄存器）的 bit 31 是：

```text
PG（Paging，分页使能位）
```

含义：

```text
CR0.PG = 0
→ Paging Disabled

CR0.PG = 1
→ Paging Enabled
```

GOS 中：

```c
enablePaging();
```

本质：

```text
CR0 |= 0x80000000
```

因为：

```text
0x80000000
```

就是把 bit 31 设置成 1。

关闭分页：

```c
disablePaging();
```

本质：

```text
CR0 &= 0x7FFFFFFF
```

把 bit 31 清零。

---

# 15. TLB 是什么

TLB（Translation Lookaside Buffer，地址转换后备缓冲器）是 CPU 用来缓存地址转换结果的结构。

假设 CPU 查页表得到：

```text
Virtual Page A
→ Physical Page B
```

CPU 可以把结果缓存到 TLB。

以后再次访问：

```text
Virtual Page A
```

就不一定重新完整查询：

```text
PDE
→ PTE
```

而是直接使用缓存结果。

---

# 16. 为什么修改页表以后要刷新 TLB

假设原来：

```text
Virtual Page A
→ Physical Page B
```

CPU 已经缓存。

后来内核修改 PTE：

```text
Virtual Page A
→ Physical Page C
```

如果 TLB 中还保留旧结果：

```text
Virtual Page A
→ Physical Page B
```

CPU 就可能继续使用错误映射。

所以 GOS：

```c
FlushTLB(addr);
```

内部执行：

```asm
invlpg
```

作用：

```text
让 addr 所在虚拟页的 TLB 缓存失效
```

于是下次访问：

```text
重新查询 PDE / PTE
```

---

# 17. 用户进程自己的 Page Directory

每个用户进程可以拥有自己的 Page Directory。

创建用户地址空间时，可以复制 Kernel Page Directory 的部分 PDE。

需要注意：

> 复制 PDE 不等于复制整个 Page Table。

如果：

```text
Kernel PDE[0]
→ Page Table K
```

复制 PDE 后：

```text
User PDE[0]
→ Page Table K
```

两者可以共享同一张 Page Table。

结构：

```text
Kernel Page Directory
        │
        └── PDE[0] ──────┐
                          │
                          ▼
                   Kernel Page Table
                          ▲
                          │
User Page Directory       │
        │                 │
        └── PDE[0] ───────┘
```

因此内核低地址映射可以被不同地址空间共享。

---

# 18. 用户栈映射

当前用户栈顶部：

```text
0x10000000
```

x86 栈向低地址增长。

如果：

```text
ESP = 0x10000000
```

执行：

```text
push 4 Byte
```

那么：

```text
ESP = 0x0FFFFFFC
```

因此需要映射：

```text
0x0FFFF000
~
0x0FFFFFFF
```

这一整页。

---

## 18.1 用户栈对应 PDE / PTE

对：

```text
0x0FFFF000
```

拆分后：

```text
PDE Index = 63
PTE Index = 1023
```

因此：

```text
Page Directory
      ↓
PDE[63]
      ↓
User Stack Page Table
      ↓
PTE[1023]
      ↓
User Stack Physical Page
```

这说明：

> 用户栈使用的是虚拟地址，真正的存储位置由页表决定。

---

# 19. Page Fault 是什么

Page Fault（缺页异常）对应：

```text
Exception Vector = 0x0E
```

也就是：

```text
14 号异常
```

当 CPU 使用虚拟地址访问内存，但页表转换过程中发现问题，就可能产生 Page Fault。

主要包括：

```text
页面不存在
```

或者：

```text
页面存在，但是权限不允许
```

---

# 20. CR2：哪里出了问题

发生 Page Fault 后，CPU 会把：

> 导致异常的 Virtual Address

保存到：

```text
CR2（Control Register 2，2号控制寄存器）
```

所以：

```text
CR2
=
哪里出错了
```

例如测试：

```c
volatile u32* test = (u32*)0x00800000;
*test = 1234;
```

发生 Page Fault 时，通过 GDB（GNU Debugger，GNU 调试器）观察：

```text
CR2 = 0x00800000
```

与实际访问地址完全一致。

---

# 21. Page Fault Error Code：为什么出错

只有 CR2 不够。

CR2 告诉：

```text
哪个地址出错
```

Page Fault Error Code（缺页异常错误码）告诉：

```text
为什么出错
```

第一阶段重点理解低 3 bit：

```text
bit 2       bit 1       bit 0
 U/S         W/R          P
```

---

## 21.1 P 位

```text
P = 0
→ 页面不存在

P = 1
→ 页面已经存在，但发生 Protection Fault（保护错误）
```

这一位最重要。

---

## 21.2 W/R 位

W/R（Write/Read，写/读）：

```text
0
→ 读取导致异常

1
→ 写入导致异常
```

---

## 21.3 U/S 位

U/S（User/Supervisor，用户态/内核态）：

```text
0
→ Supervisor，也就是内核态访问

1
→ User，也就是用户态访问
```

---

# 22. CR2 和 Error Code 的区别

最简洁的记忆：

```text
CR2
→ 哪里错了？

Error Code
→ 为什么错了？
```

两个信息结合：

```text
Page Fault
     │
     ├── CR2
     │    ↓
     │   Fault Address
     │
     └── Error Code
          ↓
         Fault Reason
```

---

# 23. Page Fault 汇编入口

主要文件：

```text
src/kernel/int/interrupt_handler.asm
```

Page Fault 的入口：

```asm
InterruptHandlerMacro 0x0e, 1
```

其中：

```text
0x0E
→ Page Fault Vector

1
→ CPU 会自动压入 Error Code
```

---

# 24. 为什么其他异常需要 ErrCodeMagic

并不是所有 CPU 异常都会自动产生 Error Code。

为了让不同异常的栈结构尽量统一，GOS 对没有错误码的异常：

```asm
push ErrCodeMagic
```

其中：

```text
ErrCodeMagic = 0x88888888
```

所以统一成：

```text
有 CPU Error Code
→ 使用真实 Error Code

没有 CPU Error Code
→ 使用 ErrCodeMagic 占位
```

这样汇编 Handler 可以使用同一种处理流程。

---

# 25. 为什么要保存寄存器

异常发生时，CPU 原本正在执行某段程序。

汇编入口执行：

```asm
push ds
push es
push fs
push gs
pushad
```

作用：

> 保存异常发生前的执行现场。

其中：

```text
DS = Data Segment，数据段寄存器
ES = Extra Segment，附加段寄存器
FS / GS = 附加段寄存器
```

`pushad` 会保存 32 位通用寄存器。

这样 Page Fault Handler 执行期间即使使用寄存器，也不会破坏原程序现场。

---

# 26. Error Code 为什么在 [esp + 48]

保存：

```text
ds
es
fs
gs
```

总共：

```text
4 × 4 Byte
=
16 Byte
```

`pushad` 保存：

```text
8 × 4 Byte
=
32 Byte
```

因此：

```text
16 + 32
=
48 Byte
```

所以保存完成后：

```text
ESP
↓
刚保存的寄存器数据
...
[ESP + 48]
↓
原始 Error Code
```

因此：

```asm
push dword [esp + 48]
```

就是：

> 把 Error Code 再作为 C 函数参数压栈。

---

# 27. 为什么是 push errorCode 再 push vector

C 函数：

```c
pageFaultHandler(u32 vector, u32 errorCode)
```

当前使用的 `cdecl（C Declaration，C语言常见调用约定）`中，参数从右向左压栈。

因此：

```asm
push errorCode
push vector
call pageFaultHandler
```

最终 C 语言看到：

```text
第一个参数 vector
第二个参数 errorCode
```

所以：

```asm
push dword [esp + 48]
push %1
```

等价于：

```c
pageFaultHandler(vector, errorCode);
```

---

# 28. C Handler 返回以后为什么 add esp, 8

调用之前额外压了两个参数：

```text
errorCode
→ 4 Byte

vector
→ 4 Byte
```

一共：

```text
8 Byte
```

所以调用结束：

```asm
add esp, 8
```

把这两个为了调用 C Handler 临时压入的参数移除。

注意：

> 这并不是删除 CPU 最开始压入的那个 Error Code。

CPU 原来的 Error Code 还在更下面。

---

# 29. 为什么还要恢复寄存器

接下来：

```asm
popad
pop gs
pop fs
pop es
pop ds
```

恢复之前保存的 CPU 执行现场。

如果漏掉这一步直接 `iret`：

> CPU 会把普通寄存器内容错误地当成 EIP / CS / EFLAGS 等返回信息。

我们实际调试时就曾因为漏掉恢复现场，而触发：

```text
Exception{13}: General Protection
```

这说明异常栈必须严格保持正确结构。

---

# 30. 为什么之后还有 add esp, 4

恢复寄存器以后：

```asm
add esp, 4
```

删除的是：

```text
CPU 自动产生的 Error Code
```

或者：

```text
ErrCodeMagic
```

然后栈顶才回到 CPU 原始异常返回现场。

最后：

```asm
iret
```

才能正确返回。

完整流程：

```text
CPU Page Fault
       ↓
Error Code 入栈
       ↓
汇编保存寄存器
       ↓
push errorCode
push vector
       ↓
调用 C Handler
       ↓
add esp, 8
删除 C 参数
       ↓
恢复寄存器
       ↓
add esp, 4
删除原 Error Code
       ↓
iret
```

---

# 31. pageFaultHandler

文件：

```text
src/kernel/int/page.c
```

核心流程：

```text
pageFaultHandler(vector, errorCode)
          ↓
Assert(vector == 0x0E)
          ↓
读取 CR2
          ↓
得到 Fault Address
          ↓
检查 errorCode
          ↓
P == 1 ?
   ├── Yes
   │     ↓
   │ Protection Fault
   │     ↓
   │   Panic
   │
   └── No
         ↓
    页面不存在
         ↓
    检查特殊地址
         ↓
      MapPage
```

---

# 32. 为什么不能所有 Page Fault 都 MapPage

假设一个页面：

```text
Present = 1
```

说明：

```text
页面已经存在
```

但用户没有权限写。

CPU：

```text
Page Fault
Error Code.P = 1
```

如果 Handler 不检查 Error Code，而直接：

```text
MapPage(addr)
```

那么 `findPTECreate()` 会发现：

```text
PDE Present = 1
PTE Present = 1
```

所以：

```text
什么也不创建
```

然后：

```text
iret
↓
重新执行原指令
↓
还是没有权限
↓
再次 Page Fault
↓
再次 MapPage
↓
还是没有修改
↓
无限循环
```

因此当前增强后的处理策略：

```text
P = 0
→ Demand Paging

P = 1
→ Protection Fault
→ Panic
```

避免无限 Page Fault。

---

# 33. Demand Paging 是什么

Demand Paging（按需分页）的核心：

> 不提前给所有虚拟地址分配物理内存，而是在真正访问的时候，需要什么就补什么。

最简单的理解：

```text
缺什么
→ 补什么
```

例如：

```text
PDE 不存在
→ 创建 Page Table

PTE 不存在
→ 分配真正 Physical Page
```

因此：

```text
访问虚拟地址
       ↓
第一次发现没有映射
       ↓
Page Fault
       ↓
现场创建映射
```

这就是：

```text
Demand
=
真正需要时再分配
```

---

# 34. MapPage 的流程

文件：

```text
src/kernel/memory/memory_mapping.c
```

当前实现：

```text
MapPage(addr)
      ↓
disablePaging()
      ↓
findPTECreate(addr)
      ↓
enablePaging()
      ↓
FlushTLB(addr)
```

---

# 35. 为什么当前 GOS 会关闭 Paging 再修改页表

当前 GOS 代码会直接把：

```text
Physical Address
```

转换为 C 指针并访问 Page Table。

为了让这种实现简单工作，当前设计：

```text
关闭 Paging
↓
直接按物理地址访问页表
↓
修改完成
↓
重新开启 Paging
```

但是需要明确：

> “Paging 开启后不能修改 Page Table”不是 x86 的硬件规定。

真实操作系统通常：

```text
Paging 保持开启
       ↓
把 Page Table 映射到自己的虚拟地址空间
       ↓
直接修改 Page Table
       ↓
刷新 TLB
```

所以 GOS 当前做法属于教学实现上的简化。

---

# 36. findPTECreate

文件：

```text
src/kernel/memory/memory_mapping.c
```

核心逻辑：

```text
Virtual Address addr
        ↓
firstIndex = addr >> 22
        ↓
得到 PDE Index
        ↓
secondIndex = (addr >> 12) & 0x3ff
        ↓
得到 PTE Index
```

然后：

```text
找到 PDE[firstIndex]
        ↓
Present == 0 ?
        │
        ├── Yes
        │     ↓
        │ AllocateOnePage(UserMode)
        │     ↓
        │ 创建新的 Page Table
        │     ↓
        │ PDE 指向 Page Table
        │
        └── No
              ↓
         使用已有 Page Table
```

接着：

```text
进入 Page Table
        ↓
找到 PTE[secondIndex]
        ↓
Present == 0 ?
        │
        ├── Yes
        │     ↓
        │ AllocateOnePage(UserMode)
        │     ↓
        │ 得到真正 Physical Page
        │     ↓
        │ PTE 指向 Physical Page
        │
        └── No
              ↓
         使用已有映射
```

---

# 37. findPTECreate 为什么可能申请两次 4KB

第一次：

```c
AllocateOnePage(UserMode)
```

可能用于：

```text
创建新的二级 Page Table
```

第二次：

```c
AllocateOnePage(UserMode)
```

用于：

```text
创建真正给虚拟地址使用的 Physical Page
```

两者虽然都申请 4KB，但用途不同：

```text
第一次
→ 存页表

第二次
→ 存用户数据
```

---

# 38. Page Fault 完整处理流程

当前 GOS：

```text
             程序访问 Virtual Address
                       │
                       ▼
                CPU 查询页表
                       │
                       ▼
               PDE / PTE 有效？
                       │
                      NO
                       │
                       ▼
              Page Fault #0x0E
                       │
           ┌───────────┴───────────┐
           │                       │
           ▼                       ▼
    CR2 = Fault Address      Error Code
           │                 = Fault Reason
           │                       │
           └───────────┬───────────┘
                       │
                       ▼
            InterruptHandler_0x0e
                       │
                       ▼
                保存寄存器现场
                       │
                       ▼
        pageFaultHandler(vector,errorCode)
                       │
                       ▼
                 检查 P bit
                  /         \
               P = 1       P = 0
                │             │
                ▼             ▼
        Protection Fault   Not Present
                │             │
              Panic           ▼
                           MapPage
                              │
                              ▼
                       findPTECreate
                        /          \
                 PDE 不存在      PTE 不存在
                     │              │
                     ▼              ▼
              创建 Page Table   分配 Physical Page
                     \              /
                      └──────┬──────┘
                             │
                             ▼
                        映射建立完成
                             │
                             ▼
                         enablePaging
                             │
                             ▼
                          FlushTLB
                             │
                             ▼
                        返回汇编入口
                             │
                             ▼
                         恢复寄存器
                             │
                             ▼
                            iret
                             │
                             ▼
                     重新执行原指令
                             │
                             ▼
                           成功
```

---

# 39. 为什么 iret 后原来的指令能成功

第一次：

```text
CPU 访问 0x00800000
↓
没有映射
↓
Page Fault
```

Handler：

```text
MapPage(0x00800000)
↓
建立 PDE / PTE / Physical Page
```

然后：

```text
iret
```

CPU 恢复到出错指令的位置。

再次执行：

```text
访问 0x00800000
```

这一次：

```text
PDE 已存在
PTE 已存在
Physical Page 已存在
```

所以访问成功。

因此：

> Page Fault 不是简单地“跳过错误指令”，而是在修复问题以后重新执行。

---

# 40. 实际 Page Fault 测试

测试代码：

```c
volatile u32* test = (u32*)0x00800000;
*test = 1234;
```

当前内核初始映射主要只有低 4MB。

而：

```text
0x00800000
=
8MB
```

因此第一次访问必然没有映射。

实际输出：

```text
Before page fault test
Page Fault: addr=0x800000 error=0x2
After page fault test: 1234
```

说明：

```text
发生 Page Fault
↓
Handler 正确得到地址
↓
建立映射
↓
返回
↓
写入成功
↓
读取仍然得到 1234
```

---

# 41. 为什么 Error Code 是 0x2

实际：

```text
error = 0x2
```

二进制：

```text
010
```

所以：

```text
P   = 0
W/R = 1
U/S = 0
```

解释：

```text
P = 0
→ 页面不存在

W/R = 1
→ 写操作触发

U/S = 0
→ 内核态触发
```

与测试：

```c
*test = 1234;
```

完全吻合。

---

# 42. GDB 实际验证

使用 GDB 设置：

```gdb
break pageFaultHandler
break MapPage
continue
```

在 Page Fault Handler 中观察：

```gdb
p/x $cr2
p/x $cr3
```

实际得到：

```text
CR2 = 0x00800000
CR3 = 0x00112000
```

其中：

```text
CR2
→ Fault Virtual Address

CR3
→ 当前 Page Directory 的物理地址
```

然后继续到：

```text
MapPage
```

执行：

```gdb
p/x addr
```

得到：

```text
addr = 0x00800000
```

因此验证：

```text
CPU CR2
0x00800000
      │
      ▼
pageFaultHandler
      │
      ▼
MapPage(addr)
0x00800000
```

整个地址传递完全一致。

---

# 43. 本次调试遇到的问题

在给 Page Fault Handler 增加 `errorCode` 参数以后，第一次运行出现：

```text
Exception{13}: General Protection
```

原因不是 Page Fault 本身。

而是修改：

```text
interrupt_handler.asm
```

时漏掉了：

```asm
popad
pop gs
pop fs
pop es
pop ds

add esp, 4
```

导致：

```text
保存的寄存器仍然在栈上
↓
直接执行 iret
↓
CPU 把错误的数据当成返回现场
↓
General Protection
```

修复完整栈恢复流程以后：

```text
Page Fault: addr=0x800000 error=0x2
After page fault test: 1234
```

恢复正常。

这个问题说明：

> 中断 / 异常入口中的栈布局必须严格保持一致，任何多 push 或少 pop 都可能破坏 iret。

---

# 44. Paging 总体逻辑图

```text
                         GOS Paging
                             │
            ┌────────────────┴────────────────┐
            │                                 │
            ▼                                 ▼
      Physical Memory                   Virtual Memory
            │                                 │
            ▼                                 ▼
      Frame Allocator                  Page Directory
            │                                 │
            ▼                                 ▼
     AllocateOnePage                         PDE
            │                                 │
            ▼                                 ▼
      Physical Page                      Page Table
                                              │
                                              ▼
                                             PTE
                                              │
                                              ▼
                                       Physical Page
```

地址转换：

```text
Virtual Address
      │
      ├───────────────┐
      │               │
      ▼               │
 PDE Index            │
      │               │
      ▼               │
Page Directory        │
      │               │
      ▼               │
     PDE              │
      │               │
      ▼               │
 Page Table           │
      │               │
      ▼               │
     PTE              │
      │               │
      ▼               │
Physical Page         │
      │               │
      └───────┐       │
              ▼       ▼
          Page Base + Offset
                  │
                  ▼
           Physical Address
```

---

# 45. 三个控制寄存器的关系

```text
CR0
→ Paging 是否开启

CR2
→ Page Fault 时哪个 Virtual Address 出错

CR3
→ 当前 Page Directory 在哪里
```

可以记成：

```text
CR0：开不开分页
CR2：哪里出错
CR3：页目录在哪
```

---

# 46. 当前 GOS Demand Paging 的能力

当前已经实现：

```text
访问未映射 Virtual Address
        ↓
Page Fault
        ↓
自动创建缺失 Page Table
        ↓
自动分配 Physical Page
        ↓
建立 PTE
        ↓
恢复执行
```

所以已经具备：

> 简单的 Anonymous Demand Paging（匿名按需分页）能力。

---

# 47. 当前还没有实现的高级虚拟内存机制

当前没有实现：

```text
Swap（交换空间）

Page Replacement（页面置换）

Copy-On-Write，COW（写时复制）

File Mapping（文件映射）

Memory-Mapped File（内存映射文件）

完整 Virtual Memory Area 管理

Lazy Executable Loading（可执行文件懒加载）
```

这些不是当前 GOS 第一阶段必须完成的内容。

---

# 48. 当前值得以后重新审视的问题

## 48.1 修改页表时关闭 Paging

当前：

```text
disablePaging
↓
修改页表
↓
enablePaging
```

这是教学实现上的简化。

以后可以考虑：

```text
Paging 一直开启
↓
让内核拥有可以访问页表的 Virtual Address
↓
直接修改
↓
Flush TLB
```

---

## 48.2 Protection Fault 当前直接 Panic

当前：

```text
Error Code.P = 1
↓
Panic
```

以后如果是用户进程：

```text
User Process
发生非法访问
↓
应该更合理地终止当前进程
```

而不是直接让整个 Kernel Panic。

---

## 48.3 Page Fault 不知道虚拟地址是否合法

当前：

```text
addr >= 4MB
且 P = 0
↓
MapPage
```

意味着只要地址满足简单条件，就可能给它分配 Physical Page。

以后应该区分：

```text
合法 Heap
合法 Stack
合法程序区域
非法 Virtual Address
```

---

## 48.4 缺少 UnmapPage

当前已经有：

```text
MapPage
```

但完整地址空间管理以后还需要：

```text
UnmapPage
```

用于：

```text
释放进程地址空间
回收页面
exit
exec
fork 失败回滚
```

等场景。

---

## 48.5 Page Table 使用 UserMode 物理页

当前 `findPTECreate()` 创建新的二级 Page Table 时使用：

```c
AllocateOnePage(UserMode);
```

但是：

```text
Page Table 本身
```

属于 Kernel 管理的数据结构。

而最终 PTE 指向的：

```text
User Physical Page
```

才是真正的用户数据页。

因此以后需要重新审视：

```text
Page Table Physical Page
是否应该从 KernelMode 页框池分配
```

当前实现能够工作，但设计语义需要后续统一。

---

# 49. Paging 第一阶段最终总结

现在我已经能够解释：

1. Frame Allocator 和 Paging 的区别。
2. Virtual Address 和 Physical Address 的区别。
3. 为什么 32 位虚拟地址拆成 10 + 10 + 12。
4. PDE / PTE 分别是什么。
5. 为什么 PDE / PTE 是 4 Byte。
6. 为什么 Page Directory / Page Table 是 4KB。
7. 为什么一个 PDE 可以管理 4MB。
8. PPN 和 Physical Address 如何转换。
9. Identity Mapping 是什么。
10. CR0、CR2、CR3 分别负责什么。
11. TLB 为什么存在。
12. 为什么修改映射后要 FlushTLB。
13. 用户进程为什么可以拥有独立地址空间。
14. 用户栈为什么映射在 PDE[63] / PTE[1023]。
15. Page Fault 为什么是 0x0E。
16. CR2 和 Page Fault Error Code 的区别。
17. Page Fault 汇编入口为什么需要保存寄存器。
18. Error Code 为什么位于 `[esp + 48]`。
19. 为什么 C Handler 参数要从右向左压栈。
20. 为什么 Handler 返回后必须正确恢复整个栈。
21. Demand Paging 为什么叫“按需分页”。
22. `findPTECreate()` 为什么可能申请两次 4KB。
23. `iret` 后为什么原来的指令可以重新成功执行。
24. 如何使用 QEMU 验证 Page Fault。
25. 如何使用 GDB 观察 CR2、CR3 和函数参数。
26. 当前 GOS Paging 实现有哪些简化和限制。

---

# 50. 一句话总结

GOS 当前 Paging 的核心可以总结成：

```text
Frame Allocator 提供 Physical Page
        ↓
Page Directory / Page Table 建立映射
        ↓
CR3 决定当前地址空间
        ↓
CPU 使用 Virtual Address 访问内存
        ↓
如果页面不存在
        ↓
Page Fault
        ↓
CR2 告诉哪里错了
Error Code 告诉为什么错了
        ↓
MapPage 按需创建映射
        ↓
FlushTLB
        ↓
iret
        ↓
重新执行原来的指令
        ↓
访问成功
```

开机以后，操作系统怎么知道有多少内存？

E820 的结果怎么变成可分配的物理页？

AllocateOnePage() 到底在分配什么？

Buddy 和 frame allocator 为什么要同时存在？

开启分页的时候页目录和页表是怎么建立的？

一个虚拟地址 CPU 怎么拆成 PDE/PTE/offset？

CR3 保存什么？

TLB 为什么存在？

访问一个没有映射的地址以后发生什么？

Page Fault 为什么能通过 CR2 找到出错地址？

内核最终怎么补上这个映射？