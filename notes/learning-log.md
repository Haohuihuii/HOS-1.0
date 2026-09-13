## 2026-08-30

### 今日学习内容

完成了 GOS 启动阶段的第一轮复习：

- 逐行复习 `boot.asm`
- 学习 `loader.asm`
- 理解 BIOS 到 Loader 再到 Kernel 的完整控制流
- 理解 ATA PIO 读取磁盘的基本流程
- 理解 BIOS E820 内存探测与 ARDS 数据结构
- 理解 A20 的作用
- 理解 GDT、段描述符和段选择子的关系
- 理解段选择子中的 Index、TI、RPL
- 理解 `lgdt`、`CR0.PE` 和 far jump 在进入保护模式时的作用
- 明确 `[bits 32]` 只是 NASM 汇编指令，不会直接改变 CPU 模式
- 理解 Loader 如何建立 32 位栈并将 Kernel 加载到 `0x7E00`
- 理解 `push ards_cnt` 如何把 E820 内存信息传递给 Kernel
- 理解 `entry.asm` 如何利用栈调用约定调用 `MemoryCheckout`
- 理解 `call KernelMain` 标志着系统正式进入 C 内核

### 当前能够独立解释的启动链

BIOS
→ `boot.asm` @ `0x7C00`
→ ATA PIO 加载 Loader 到 `0x500`
→ Loader 使用 E820 获取物理内存地图
→ 开启 A20
→ 加载临时 GDT
→ 设置 `CR0.PE = 1`
→ far jump 进入 32 位保护模式代码
→ 设置数据段和 `ESP = 0x7000`
→ ATA PIO 加载 Kernel 到 `0x7E00`
→ `push ards_cnt`
→ 跳转到 Kernel
→ `entry.asm`
→ `MemoryCheckout`
→ `KernelMain`

### 今天纠正的几个理解

- `0x7C00` 是传统 PC BIOS 启动约定，不是 x86 CPU 硬件规定。
- `0x08` 是段选择子，不是 GDT 地址。
- 段选择子的低 3 位由 `TI + RPL` 构成，`Index = selector >> 3`。
- `CR0.PE = 1` 开启保护模式机制，far jump 用于重新加载 `CS` 并开始使用新的代码段描述符。
- `[bits 32]` 是给 NASM 看的，不是给 CPU 切换模式的指令。
- `push ards_cnt` 压入的是地址，`push [ards_cnt]` 才是压入 ARDS 数量。
- LBA 从 0 开始编号，因此 Loader 使用 `ECX = 4` 表示从 `LBA 4` 开始读取 Kernel。

### 当前发现但尚未确认的问题

- `boot.asm` / Loader 实模式阶段在使用 `call`、`push`、`ret` 前没有明显初始化 `SS:SP`，可能依赖 BIOS 遗留状态。
- Loader 使用 `DS:SI` 访问字符串，但没有明显显式初始化 `DS = 0`，可能同样依赖启动环境。
- Loader 固定读取 200 个 Kernel 扇区，Kernel 后续增长到约 100KB 以上时需要调整加载协议。

这些问题目前记录为待验证项，不作为已确认运行时 Bug。

### 文档进展

今天开始整理：

- `docs/boot.md`
- `docs/architecture.md`
- `docs/memory-layout.md`

其中 `boot.md` 记录完整启动流程；`architecture.md` 和 `memory-layout.md` 当前只记录已经确认的部分，后续随模块学习继续扩展。

### 下一步

下一阶段开始进入真实 Kernel 源码：

`entry.asm`
→ `kernel_main.c`
→ `memory/frame.c`
→ `memory/buddy.c`

重点重新理解：

- 物理页框管理
- Buddy 伙伴分配器
- Kernel 初始化依赖关系


## 2026-08-31

### 今日完成

- 完成 Physical Memory（物理内存）与 Buddy Allocator（伙伴分配器）的文档收尾。
- 开始学习 Paging（分页）与 Virtual Memory（虚拟内存）。
- 理解 32 位地址的 `10 + 10 + 12` 两级分页结构。
- 理解 PDE（Page Directory Entry，页目录项）负责寻找二级页表，PTE（Page Table Entry，页表项）负责寻找最终物理页。
- 学习 GOS `InitializeMemoryMapping()`：
  - 创建页目录和二级页表。
  - 建立 0～4MB 基础等值映射。
  - 使用 CR3（Control Register 3，3号控制寄存器）指定当前页目录。
  - 使用 CR0.PG（Paging，分页使能位）开启分页。
  - 理解 TLB（Translation Lookaside Buffer，地址转换后备缓冲器）及刷新原因。
- 开始学习用户进程分页：
  - 每个用户进程拥有自己的页目录。
  - 复制 Kernel 页目录不会复制二级页表。
  - 理解用户栈 `0x0FFFF000` 位于 `PDE[63] / PTE[1023]`。
  - 开始分析用户栈虚拟地址到物理页的真实映射。

### 当前理解

分页的核心是：

`虚拟地址 → 页目录 → 二级页表 → 物理页 + 页内偏移`

物理内存分配和分页职责不同：

`FrameAllocator` 决定“哪个物理页可以使用”，分页决定“某个虚拟地址映射到哪个物理页”。

### 下一步

继续完成用户栈映射：

`PDE[63] → 二级页表 → PTE[1023] → 用户栈物理页`

之后学习进程切换时如何通过 `process->RootPPN` 和 `CR3` 切换不同的虚拟地址空间，并完成 Paging 模块第一轮学习。

## 2026-09-01

完成 GOS Paging / Page Fault 第一阶段。

- 理解 PDE / PTE、CR0 / CR2 / CR3、TLB 和用户地址空间。
- 补全 `MapPage()`、`findPTE()`、`findPTECreate()` 和 Page Fault Handler。
- 增加 Page Fault Error Code 判断，区分缺页与 Protection Fault。
- 使用 QEMU 验证 `0x00800000` 按需分配成功。
- 使用 GDB 验证 `CR2 = 0x00800000`、`CR3` 和 `MapPage(addr)`。
- 完成 `notes/paging-learning.md`、`docs/paging.md` 和 `notes/questions/paging.md`。

# 2026-09-02 Learning Log

## 今日学习
复习了第四部分「中断与特权级」的大部分核心内容：

- GDT、段选择子，以及 CPL / DPL / RPL 的区别
- IDT、中断门、Vector 与中断入口的对应关系
- `InterruptHandlerEntryTable` 与 `InterruptHandlerList` 两层中断分发
- 中断发生后 CPU 自动压栈、汇编保存现场、ErrorCode 统一处理
- 结合 Page Fault 串通：
  `访存异常 → #PF(0x0E) → IDT → 汇编入口 → C Handler → MapPage`
- TSS 的 `SS0 / ESP0`，以及用户态进入内核态时的栈切换
- `InterruptContext` 与真实栈布局
- `AllTrapsEntry / RestoreContext / iret`
- 理解了系统调用返回值通过修改栈中保存的 EAX，在 `popad` 后返回用户态

## 仍需注意
- 页向上对齐得到的是内核栈页的高地址边界（栈顶），不是页起始地址
- `IDT.DPL` 主要限制软件 `int n` 主动调用中断门
- `TSS.ESP0` 必须随当前用户进程更新

# 2026-09-03 Learning Log

## 今日学习

完成了第四部分「中断与特权级」的剩余内容，并做了整体串联：

- 学习 8259A PIC 的主从结构与 IRQ 映射
- 理解 `IRQ0~15 → Vector 0x20~0x2F`
- 理解 `SetInterrupt()` 如何解除 PIC 中断屏蔽
- 区分 PIC Mask 与 CPU `EFLAGS.IF`
- 学习 `sti / cli` 对可屏蔽硬件中断的控制
- 理解 EOI，以及从 PIC 中断为什么需要同时通知主片和从片
- 串通三种中断来源：
  - CPU Exception
  - Hardware IRQ
  - Software Interrupt `int 0x80`
- 完整复盘：
   `GDT → TSS → IDT → 汇编入口 → C Handler → 恢复现场 → iret`
- 进一步明确 CPU 自动保存现场与 GOS 汇编主动保存现场的区别
- 完成第四部分答辩式综合复习
- 整理完成 `interrupt.md`
- 整理完成 `interrupt-learning.md`

## 仍需注意

- `InterruptHandlerEntryTable` 只是汇编入口地址表，真正保存和恢复现场的是对应的汇编入口代码
- `ds/es/fs/gs` 和 `pushad` 是 GOS 汇编主动保存，不是 CPU 自动保存
- `IRQ` 与 `Vector` 是两个不同概念
- `SetInterrupt()` 和 `sti` 分别控制 PIC 层与 CPU 层的中断开关

# 2026-09-06 Learning Log

## 今日学习

完成了第六部分「基础进程管理」的核心内容，并把进程创建、调度与上下文切换串联起来：

- 复习进程、PID 与 PCB 的基本概念
- 理解 `PIDAllocator` 使用 Bitmap 分配和释放 PID
- 理解三种进程状态：
  - `RUNNING`
  - `RUNNABLE`
  - `BLOCKED`
- 学习 `ProcessManager` 与循环就绪队列
- 理解 `Front / Rear / AddProcess() / fetchProcess()` 的作用
- 学习 `Schedule()` 的基本调度流程
- 复习 `TSS.ESP0`，理解其用于用户进程 `Ring3 → Ring0` 时切换到对应内核栈
- 学习 `SwitchContext` 与 `SwitchProcess()`
- 理解：
   `保存 current 的 ESP → 切换 next 的 ESP → pop 恢复寄存器 → ret 恢复 EIP`
- 学习 `CreateKernelProcess()`，理解新内核进程如何通过伪造 `SwitchContext` 第一次启动
- 学习 `CreateUserProcess()`，理解用户进程同时需要 Kernel Stack 与 User Stack
- 理解用户进程的两层上下文：
  - `SwitchContext`：负责进程调度与切换
  - `InterruptContext`：负责从 Ring0 恢复到 Ring3
- 详细学习 `restore()`，理解：
   `SwitchContext → restore() → InterruptContext → RestoreContext → Ring3 entry`
- 进一步区分 `TSS.ESP0` 与 `InterruptContext.ESP3 / SS3`

## 仍需注意
- `current->ID` 是 PID 数值，不是 PID Bitmap 中的某一个 bit
- `Current` 与 `RUNNABLE` 要结合 `Schedule()` 执行到哪一行来判断，不能混淆中间状态与调度完成后的稳定状态
- `KernelStackPointer` 保存的是进程被切换时的内核栈指针，不等于固定的内核栈顶
- `mov [eax], esp` 是保存旧进程 ESP，`mov esp, [eax]` 是切换到新进程内核栈
- Kernel Process 第一次启动伪造的是 `SwitchContext`，不是 `InterruptContext`
- User Process 的 `SwitchContext.EIP = restore`，而 `InterruptContext.EIP = entry`
- `ESP3` 不是独立的 CPU 寄存器，而是 `InterruptContext` 中保存的 Ring3 用户栈指针
- `MemoryFree()` 实际功能是将一段内存清零，并不是真正释放内存，函数命名容易产生误解


# 2026-09-12 Learning Log
- 完成 Fork / Parent / Child / Process Tree 主体流程学习，理解父进程返回 Child PID、子进程返回 `0` 的实现。
- 理解 `exit / zombie / wait` 的基本关系：
   `exit → ZOMBIE → parent wait → 最终回收`。
- 明确 Zombie 不应再次进入 Runnable Queue，因此 Scheduler 应只把原本 `RUNNING` 的进程重新设为 `RUNNABLE`。
- 设计 PCB 增加 `ExitCode`，用于保存子进程退出状态。