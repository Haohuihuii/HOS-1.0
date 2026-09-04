# GOS Memory Layout

本文记录 GOS 当前已经确认的关键内存地址。

该文档随着内存管理和分页模块学习继续扩充。

---

## 1. Early Boot Physical Memory Layout

```text
Low Address

0x00000000
    |
    | BIOS / Interrupt Vector / low memory structures
    |
0x00000500
+--------------------------------+
| Loader                         |
| loader.asm                     |
+--------------------------------+

        ...

0x00007000
+--------------------------------+
| Loader Stack Top               |
| ESP = 0x7000                   |
| Stack grows downward           |
+--------------------------------+

        ...

0x00007C00
+--------------------------------+
| Boot Sector                    |
| boot.asm                       |
| 512 Byte                       |
+--------------------------------+

0x00007E00
+--------------------------------+
| Kernel                         |
| os.bin                         |
| loaded by Loader               |
+--------------------------------+
        |
        |
        v

0x000B8000
+--------------------------------+
| VGA Text Framebuffer           |
+--------------------------------+


## 2.Disk-to-Memory Mapping

Disk                         Physical Memory

LBA 0
boot.bin
        ------------------>  0x7C00

LBA 1~3
loader.bin
        ------------------>  0x500

LBA 4...
os.bin
        ------------------>  0x7E00


## Physical Memory Classes

当前 FrameAllocator 最多描述 32MB 物理内存。

```text
0x00000000
    │
    │ Low memory / reserved
    │ 不参与普通页分配
    │
0x00100000  1MB
    │
    │ Kernel-class physical pages
    │
0x00400000  4MB
    │
    │ User-class physical pages
    │
0x02000000  32MB
    │
    └─ 当前 FrameAllocator 描述范围结束


## Virtual Memory Layout

当前 Paging 初始化后的主要虚拟地址布局：

```text
0x00000000 - 0x00000FFF
→ Unmapped
→ 用于捕获 Null Pointer（空指针）访问

0x00001000 - 0x003FFFFF
→ Kernel Identity Mapping（内核恒等映射）
→ Virtual Address ≈ Physical Address

0x00400000 以上
→ 不在初始低 4MB 映射中
→ 可通过 Page Fault + Demand Paging 动态建立映射

0x0FFFF000 - 0x0FFFFFFF
→ 当前用户栈页

0x10000000
→ 当前用户栈顶
```

不同用户进程可以拥有独立 Page Directory（页目录），因此相同 Virtual Address 可以映射到不同 Physical Page（物理页）。