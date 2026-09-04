# GOS Memory Management

本文描述当前 GOS 已确认的物理内存管理与 Kernel Heap（内核堆）实现。

当前覆盖：

- BIOS E820 内存信息接收
- Physical Frame Allocator（物理页框分配器）
- Buddy Allocator（伙伴分配器）
- Kernel `Malloc` / `Free`

Paging（分页）与 Virtual Memory（虚拟内存）将在 `docs/paging.md` 中单独记录。

---

## 1. Architecture

当前 GOS 内存管理前半部分分为两层：

```text
BIOS E820
    ↓
ARDS
    ↓
MemoryCheckout()
    ↓
FrameAllocator
    │
    ├─ AllocateOnePage()
    ├─ AllocatePagesContinuously()
    └─ FreeOnePage()
    ↓
BuddyAllocator
    │
    ├─ Malloc()
    └─ Free()
    ↓
Kernel dynamic allocations
```

其中：

- `FrameAllocator` 管理固定大小的 4KB 物理页。
- `BuddyAllocator` 建立在 `FrameAllocator` 之上，负责 Kernel 的较小粒度动态内存分配。

---

## 2. Basic Constants

当前定义：

```c
#define MAX_PAGE_COUNT 8192
#define PageSize 4096
#define PageSizeBits 12
```

因此：

- Page Size：4KB
- 最大物理页数：8192
- `FrameAllocator` 最多可以描述 32MB 物理内存

计算：

```text
8192 × 4096 Byte
= 33554432 Byte
= 32MB
```

因为：

```text
4096 = 2^12
```

所以物理页号和物理地址之间可以通过移位转换。

---

## 3. Physical Page Number

PPN（Physical Page Number，物理页号）表示一个物理页的编号。

当前关系：

```text
PPN = PhysicalAddress >> 12

PhysicalAddress = PPN << 12
```

例如：

```text
PPN 256
= 256 × 4096
= 0x00100000
= 1MB
```

当前相关接口：

```c
PhysicalPageNumber GetPPNFromAddressFloor(PhysicalAddress address);
PhysicalPageNumber GetPPNFromAddressCeil(PhysicalAddress address);
PhysicalAddress GetAddressFromPPN(PhysicalPageNumber ppn);
```

其中：

- `GetPPNFromAddressFloor()`：向下取所在物理页号
- `GetPPNFromAddressCeil()`：向上取完整物理页边界
- `GetAddressFromPPN()`：将物理页号转换为物理地址

---

## 4. FrameAllocator

当前数据结构：

```c
typedef struct FrameAllocator {
    u8 Pages[MAX_PAGE_COUNT];
    u32 TotalFreePageCount;
    u32 KernelFreePageCount;
} FrameAllocator;
```

`Pages[i]` 对应 PPN 为 `i` 的物理页。

每一个物理页使用 1 Byte 保存状态。

根据当前真实代码行为：

| Bit | Value | Meaning |
| --- | --- | --- |
| bit0 | 0 | Free |
| bit0 | 1 | Used |
| bit1 | 1 | Kernel |
| bit1 | 0 | User |
| bit2 | 1 | Dirty |

> 注意：当前 `type.h` 中部分注释与实际代码行为相反，因此本文以真实函数执行逻辑为准。

---

## 5. Early Physical Memory Initialization

物理页管理在 `KernelMain()` 之前初始化。

启动阶段已经完成：

```text
Loader
    ↓
BIOS E820
    ↓
ards_cnt + ards[]
    ↓
push ards_cnt
    ↓
entry.asm
    ↓
MemoryCheckout()
```

`MemoryCheckout()` 首先调用：

```c
globalFrameAllocatorInit();
```

初始化策略非常保守：

> 所有物理页一开始都视为 Used。

之后再根据 BIOS E820 返回的 ARDS，
只将 BIOS 明确声明为可用的物理区域改为 Free。

这种方式可以避免 Kernel 错误分配 BIOS 保留区、设备映射区或其他不可使用的物理内存。

---

## 6. Address Range Descriptor

ARDS（Address Range Descriptor Structure，地址范围描述结构）定义为：

```c
typedef struct AddressRangeDescriptor {
    u64 BaseAddress;
    u64 Length;
    u32 Type;
} AddressRangeDescriptor;
```

Loader 使用 BIOS E820 获取一系列 ARDS，并将它们保存为：

```text
ards_cnt
ards[0]
ards[1]
ards[2]
...
```

其中：

- `ards_cnt`：ARDS 数量
- `ards[]`：实际内存区域描述数组

Loader 将 `ards_cnt` 的地址压栈并传递给 Kernel。

Kernel 可以通过该地址先取得数量，再访问紧随其后的 ARDS 数组。

---

## 7. MemoryCheckout

当前 `MemoryCheckout()` 只处理满足：

```text
Type == 1
BaseAddress >= 0x100000
```

的内存区域。

其中：

- `Type == 1`：BIOS 声明该区域可用
- `BaseAddress >= 1MB`：跳过低端 1MB 地址空间

处理流程：

```text
ARDS
 ↓
检查 Type == 1
 ↓
检查 BaseAddress >= 1MB
 ↓
BaseAddress 向上对齐到 4KB
 ↓
每隔 4KB 调用 FreeOnePage()
 ↓
加入 FrameAllocator 空闲页池
```

起始地址必须向上对齐，是因为 `FrameAllocator` 只能管理完整的 4KB 页。

例如：

```text
0x00100234
```

不能直接作为完整物理页使用。

需要向上对齐为：

```text
0x00101000
```

---

## 8. Physical Page Classes

`globalFrameAllocatorInit()` 当前将物理页划分成两类。

### 8.1 Kernel Pages

前 1024 页：

```text
0 ~ 4MB
```

被标记为 Kernel 类型。

因为：

```text
1024 × 4KB = 4MB
```

---

### 8.2 User Pages

4MB 之后的物理页被标记为 User 类型。

因此当前模型大致为：

```text
0MB                    4MB                    32MB
│                       │                       │
├──── Kernel Pages ─────┼───── User Pages ─────┤
```

这里的 Kernel/User 表示当前物理页分配器中的页池分类，
而不是某个具体进程已经拥有该页。

---

## 9. AllocateOnePage

接口：

```c
PhysicalAddress AllocateOnePage(MachineMode mode);
```

函数从：

```text
PPN 256
```

开始扫描。

PPN 256 对应：

```text
0x00100000
= 1MB
```

因此低端 1MB 不参与普通物理页分配。

一个候选页必须同时满足：

```text
Free
+
MachineMode 匹配
```

找到以后：

```text
找到空闲页
    ↓
setUsed()
    ↓
更新空闲页计数
    ↓
清空 4KB 页内容
    ↓
PPN → PhysicalAddress
    ↓
返回页起始物理地址
```

在返回前必须先执行 `setUsed()`，
否则下一次分配可能再次找到同一个物理页。

---

## 10. FreeOnePage

接口：

```c
void FreeOnePage(PhysicalAddress address);
```

释放地址必须满足：

```text
address % PageSize == 0
```

也就是必须正好处于 4KB 页面边界。

例如：

```text
0x00100000  OK
0x00101000  OK

0x00100123  Invalid
```

处理流程：

```text
PhysicalAddress
    ↓
转换为 PPN
    ↓
setFree()
    ↓
增加空闲页计数
```

`FreeOnePage()` 才是真正将物理页归还给 `FrameAllocator` 的函数。

---

## 11. AllocatePagesContinuously

接口：

```c
PhysicalAddress AllocatePagesContinuously(
    MachineMode mode,
    u32 pageCount
);
```

该函数寻找：

> `pageCount` 个地址连续，并且全部为空闲、MachineMode 一致的物理页。

例如需要连续 3 页：

```text
Free  Free  Free
```

可以分配。

而：

```text
Free  Free  Used  Free
```

即使总共有 3 个 Free 页，也不能作为连续 3 页返回。

成功找到后：

```text
连续 N 个空闲页
    ↓
全部 setUsed()
    ↓
更新空闲页计数
    ↓
清空整个连续区域
    ↓
返回第一页面的物理地址
```

`BuddyAllocator` 就通过这个函数申请自己的 Kernel Heap。

---

## 12. Relationship Between FrameAllocator and BuddyAllocator

两者关系：

```text
Physical Memory
    ↓
FrameAllocator
    │
    │  AllocatePagesContinuously(KernelMode, 16)
    ↓
16 continuous pages
    ↓
64KB
    ↓
BuddyAllocator
    ↓
Malloc / Free
    ↓
Kernel small allocations
```

可以简单理解为：

- `FrameAllocator` 管“整页”
- `BuddyAllocator` 管“小块”

如果 Kernel 申请：

```c
Malloc(100);
```

直接占用一个 4KB 页会产生较大浪费。

因此 Buddy 先从 `FrameAllocator` 获得一块较大的连续内存，
再在内部进行更细粒度的拆分。

---

## 13. Buddy Allocator Configuration

当前定义：

```c
#define BUDDY_BLOCKS 1024
#define BUDDY_PAGES 16
```

Buddy 初始化时调用：

```c
AllocatePagesContinuously(KernelMode, BUDDY_PAGES);
```

因此得到：

```text
16 × 4KB
= 64KB
```

连续 Kernel 物理内存。

当前这 64KB 构成固定大小的 Kernel Buddy Heap。

---

## 14. BuddyBlock

Buddy 使用以下元数据描述当前 Heap 中的内存块：

```c
typedef struct BuddyBlock {
    PhysicalAddress BaseAddress;
    Size BlockSize;
    Boolean IsUsed;
    i32 Next;
} BuddyBlock;
```

字段含义：

- `BaseAddress`：该内存块起始地址
- `BlockSize`：该块大小
- `IsUsed`：当前是否已经被分配
- `Next`：链表中下一个 `BuddyBlock` 的数组下标

需要注意：

> `BuddyBlock` 只是元数据，不是真实的内存块。

例如：

```text
BlockSize = 64KB
```

只表示该描述符描述了一块 64KB 内存，
并不意味着这个 `BuddyBlock` 自己占用了额外 64KB。

---

## 15. BuddyBlock Linked List

当前 `Next` 的特殊值：

```text
Next == -1
→ 当前节点是链表最后一个节点

Next == 0
→ 当前 BuddyBlock 元数据槽未使用

Next > 0
→ 下一个有效 BuddyBlock 的数组下标
```

因此 `BuddyBlock Blocks[]` 实际上被组织成一个基于数组下标的链表。

---

## 16. Buddy Initialization

初始化后主要结构：

```text
Blocks[0]
    │
    ▼
Blocks[1]
    │
    ▼
   -1
```

### Blocks[0]

主要作用：

- 链表哨兵节点
- 标记为 Used
- 保存 Buddy Heap 的基准地址和大小信息
- `Next = 1`

### Blocks[1]

描述真正可供分配的 64KB 内存：

```text
BaseAddress = Buddy Heap Base
BlockSize   = 64KB
IsUsed      = FALSE
Next        = -1
```

所以初始化完成后，整个 Buddy Heap 可以理解为：

```text
┌──────────────────────────────┐
│           64KB Free          │
└──────────────────────────────┘
```

---

## 17. Malloc

接口：

```c
PhysicalAddress Malloc(Size size);
```

当前查找策略为：

**First Fit（首次适配）**

即从链表开始寻找第一个满足：

```text
IsUsed == FALSE
BlockSize >= size
```

的内存块。

---

## 18. Buddy Split

如果找到的块满足：

```text
BlockSize >= 2 × size
```

则继续进行二分。

一次 split：

```text
2N
│
├─ N
└─ N
```

例如：

```text
64KB
↓
32KB + 32KB
```

split 不会移动或复制真实内存。

它只是将：

```text
一个 BuddyBlock 描述 64KB
```

修改为：

```text
一个 BuddyBlock 描述前 32KB
+
一个 BuddyBlock 描述后 32KB
```

真实的底层 64KB 物理内存完全没有变化。

---

## 19. Example: Malloc(1000)

假设当前 Buddy Heap 完全空闲：

```text
64KB Free
```

调用：

```c
Malloc(1000);
```

可能经历：

```text
64KB
↓
32KB
↓
16KB
↓
8KB
↓
4KB
↓
2KB
↓
1KB
```

准确来说最终块大小为：

```text
1024 Byte
```

因为：

```text
1024 >= 1000
```

但是：

```text
1024 < 2 × 1000
```

因此不再继续拆分。

最终：

```text
BlockSize = 1024
IsUsed = TRUE
```

并返回该块的 `BaseAddress`。

此时 Buddy Heap 大致为：

```text
┌──────┬──────┬──────┬──────┬──────┬───────┬───────┐
│ 1KB  │ 1KB  │ 2KB  │ 4KB  │ 8KB  │ 16KB  │ 32KB  │
│ Used │ Free │ Free │ Free │ Free │ Free   │ Free   │
└──────┴──────┴──────┴──────┴──────┴───────┴───────┘
```

总大小仍然是 64KB。

---

## 20. Free

接口：

```c
void Free(PhysicalAddress address);
```

处理流程：

```text
address
    ↓
遍历 BuddyBlock 链表
    ↓
查找 BaseAddress == address
    ↓
IsUsed = FALSE
    ↓
mergeBuddy()
```

这里的 `Free()` 并不会调用：

```c
FreeOnePage()
```

因此：

> 被释放的小块只是重新归 Buddy Heap 所有。

Buddy 初始化时获得的 64KB 仍然属于 Buddy，
不会重新归还给 `FrameAllocator`。

---

## 21. Buddy Relationship

两个块即使满足：

- 地址相邻
- 大小相同
- 都是 Free

仍然不一定能合并。

例如：

```text
        4KB
       /   \
     2KB   2KB
    /  \   /  \
   A    B C    D
```

其中：

```text
A = 1KB
B = 1KB
C = 1KB
D = 1KB
```

真正的伙伴关系是：

```text
A ↔ B
C ↔ D
```

虽然：

```text
B ↔ C
```

在地址上也是相邻的，
但它们不是从同一个父块拆出来的，因此不能合并。

Buddy 算法的核心约束可以概括为：

> 怎么 split 出来的，就必须按照相反的关系 merge 回去。

---

## 22. Buddy Merge

当两个真正的伙伴同时处于 Free 状态时，可以合并：

```text
1KB + 1KB
↓
2KB

2KB + 2KB
↓
4KB

4KB + 4KB
↓
8KB
```

一直可以恢复到：

```text
32KB + 32KB
↓
64KB
```

merge 同样不会搬移真实内存。

主要修改的是 Buddy 元数据：

- 扩大左侧块的 `BlockSize`
- 修改 `Next`
- 释放右侧 `BuddyBlock` 元数据槽

---

## 23. Current Limitations

### 23.1 FrameAllocator Capacity

当前：

```text
MAX_PAGE_COUNT = 8192
PageSize       = 4KB
```

因此最多描述：

```text
8192 × 4KB = 32MB
```

物理内存。

该容量属于当前实现限制。

---

### 23.2 Fixed Buddy Heap

Buddy 启动时固定获得：

```text
16 × 4KB = 64KB
```

当前 `Malloc()` 没有在 64KB 不够时再次向 `FrameAllocator` 请求更多物理页的机制。

因此当前 Kernel Buddy Heap 是固定大小的 64KB 内存池。

---

### 23.3 Physical Page Classification

当前实现简单地按照地址范围划分：

```text
0 ~ 4MB
→ Kernel Pages

4MB ~ 32MB
→ User Pages
```

这是一种当前 GOS 使用的简化物理页分类策略。

未来如果内存管理设计发生变化，需要同步更新本文档。

---

## 24. Known Issues

当前源码学习阶段已经发现若干问题：

- `type.h` 中部分 FrameAllocator bit 注释与实际代码行为相反。
- `AllocatePagesContinuously()` 缺少 `i + j` 的数组边界保护。
- `MemoryCheckout()` 对超过 32MB 的 E820 内存范围缺少明显边界限制。
- `isDirty()` 与 `setDirty()` 的当前语义存在冲突。
- Buddy `Free()` 找不到地址时可能访问 `Blocks[-1]`。
- `mergeBuddy()` 当前伙伴判断条件过宽。
- Buddy 元数据初始化循环使用 `BUDDY_PAGES`，而不是 `BUDDY_BLOCKS`。

具体调查记录见：

```text
notes/questions.md
```

这些问题在经过运行时测试、QEMU 或 GDB 验证后，
再决定是否建立独立的 debugging 文档。

---

## 25. Memory Management Flow

当前已经确认的完整前半段内存管理链路：

```text
BIOS E820
    ↓
ARDS
    ↓
Loader
    ↓
entry.asm
    ↓
MemoryCheckout()
    ↓
FrameAllocator
    ↓
4KB Physical Frames
    ↓
AllocatePagesContinuously(KernelMode, 16)
    ↓
64KB Buddy Heap
    ↓
Malloc()
    ↓
First Fit
    ↓
Split
    ↓
Kernel allocation
    ↓
Free()
    ↓
Merge
```

---

## 26. Paging

本文当前不覆盖 Paging（分页）与 Virtual Memory（虚拟内存）。

后续学习 `memory_mapping.c` 后，将在：

```text
docs/paging.md
```

中记录：

- Linear Address（线性地址）
- Physical Address（物理地址）
- Page Directory（页目录）
- Page Table（页表）
- PDE（Page Directory Entry，页目录项）
- PTE（Page Table Entry，页表项）
- CR3（Control Register 3，3号控制寄存器）
- CR0.PG（Paging，分页使能位）
- TLB（Translation Lookaside Buffer，地址转换后备缓冲器）
- Kernel Virtual Address Mapping
- User Virtual Address Mapping