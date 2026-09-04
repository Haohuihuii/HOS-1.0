# GOS Architecture

## 1. Project Overview

GOS 是一个面向 Intel 80386 兼容 32 位 x86 环境的教学操作系统。

当前系统采用单体内核结构。

当前已确认的启动主链：

Firmware
→ Boot Sector
→ Loader
→ Protected Mode
→ Kernel Entry
→ C Kernel

---

## 2. Boot Architecture

启动阶段分为三层。

### Stage 1: Boot Sector

源码：

src/bootloader/boot.asm

职责：

- BIOS 启动入口
- 从磁盘加载 Loader
- 将控制权转移到 Loader

运行地址：

0x7C00

---

### Stage 2: Loader

源码：

src/bootloader/loader.asm

职责：

- 获取物理内存地图
- 开启 A20
- 建立临时 GDT
- 从实模式进入 32 位保护模式
- 建立临时栈
- 从磁盘加载 Kernel
- 将 E820 内存信息传递给 Kernel

运行地址：

0x500

---

### Stage 3: Kernel Entry

源码：

src/kernel/entry.asm

职责：

- 接收 Loader 提供的启动参数
- 初始化早期物理内存管理
- 调用 KernelMain
- 从汇编启动环境进入主要 C 内核

Kernel 加载地址：

0x7E00

---

## 3. Current Initialization Skeleton

当前已知的高层启动关系：

Boot
→ Loader
→ Kernel Entry
→ Early Memory Initialization
→ KernelMain
→ Kernel Subsystems

具体 KernelMain 子系统初始化顺序将在逐模块复核源码后补充。

---

## Memory Architecture

当前 GOS 内存管理分为两层：

```text
BIOS E820
    ↓
MemoryCheckout()
    ↓
FrameAllocator
    ↓
4KB Physical Pages
    ↓
BuddyAllocator
    ↓
Kernel Malloc / Free


## Paging Architecture

GOS 内存与分页模块关系：

```text
Frame Allocator
      ↓
提供 4KB Physical Page（物理页）
      ↓
Paging
      ├── Page Directory（页目录）
      ├── Page Table（二级页表）
      └── Virtual → Physical Mapping
                  ↓
             Page Fault
                  ↓
        Demand Paging（按需分页）
```

发生缺页时：

```text
Page Fault
→ CR2 获取出错虚拟地址
→ Error Code 判断异常原因
→ MapPage()
→ 建立缺失映射
→ Flush TLB
→ iret 返回继续执行
```