PPN physical page number
物理地址 = PPN*4096 -> PPN = address >> 12
当前这个物理页框分配器只能记录 32MB 范围内的物理页状态 8192*4kB

默认所有物理页不可用
        ↓
读取 BIOS E820
        ↓
只把确定可用的页标成 Free

*(Size*)ardCountAddress  (size*):把后面的地址类型当成size*

BIOS E820
    ↓
Loader
    ↓
ards_cnt + ards[]
    ↓
push ards_cnt
    ↓
entry.asm
    ↓
MemoryCheckout()
    ↓
globalFrameAllocatorInit()
    │
    └─ 默认所有页 Used
    ↓
遍历 ARDS
    ↓
找到 Type == 1 的可用内存
    ↓
4KB 对齐
    ↓
FreeOnePage()
    ↓
真正建立物理页框池

for (Size i = 256; i < MAX_PAGE_COUNT; i++) 少个边界检查？

Block[0] = 链表哨兵 + 整个 Buddy 区域的一些全局描述信息。
Block[1] = 当前真正参与分配的第一块 64KB 空闲块。
现在Buddy算法的malloc策略：first fit

BIOS E820
   ↓
ARDS 内存地图
   ↓
MemoryCheckout()
   ↓
┌────────────────────────────┐
│ frame.c                    │
│ FrameAllocator             │
│ 粒度：4KB                  │
│                            │
│ AllocateOnePage()          │
│ AllocatePagesContinuously()│
│ FreeOnePage()              │
└─────────────┬──────────────┘
              │
              │ 给 Buddy 连续 16 页
              │ = 64KB
              ▼
┌────────────────────────────┐
│ buddy.c                    │
│ BuddyAllocator             │
│                            │
│ Malloc(size)               │
│ Free(address)              │
└─────────────┬──────────────┘
              │
              ▼
     Kernel 各模块的小块内存


Buddy 不是 FrameAllocator 的替代品，而是建立在 FrameAllocator 上的一层。
Malloc() 并不是每次都拆分。只有当前找到的块明显比需求大时才拆。
整个 Kernel 动态堆目前就是固定 64KB。

# Memory Learning Notes

> 当前阶段只记录：
>
> - E820 / ARDS 到物理页框管理
> - FrameAllocator
> - BuddyAllocator
>
> Paging（分页）与 Virtual Memory（虚拟内存）之后单独学习。

---

## 1. 当前理解的内存管理主线

目前我理解的 GOS 内存管理前半部分是：

BIOS E820
→ Loader 获取 ARDS
→ entry.asm 将 ards_cnt 地址传给 Kernel
→ MemoryCheckout()
→ FrameAllocator
→ 4KB 物理页分配
→ BuddyAllocator
→ Kernel Malloc / Free

也就是：

物理内存
→ 按 4KB 划分成物理页
→ FrameAllocator 管理整页
→ Buddy 从 FrameAllocator 一次拿 64KB
→ 再将 64KB 拆成小块供 Kernel 动态分配

---

## 2. Frame 和 Buddy 的区别

### FrameAllocator

FrameAllocator 负责管理整页物理内存。

当前：

- PageSize = 4096 Byte = 4KB
- MAX_PAGE_COUNT = 8192
- 最多描述 8192 × 4KB = 32MB 物理内存

主要接口：

- AllocateOnePage()
- AllocatePagesContinuously()
- FreeOnePage()

它解决的问题是：

> “给我一页物理内存。”
>
> “给我连续 N 页物理内存。”

---

### BuddyAllocator

BuddyAllocator（伙伴分配器）建立在 FrameAllocator 之上。

初始化时：

AllocatePagesContinuously(KernelMode, 16)

因此一次获得：

16 × 4KB = 64KB

之后 Kernel 调用：

Malloc(size)

时，一般不会再向 FrameAllocator 申请物理页，而是在这 64KB 内部进行拆分。

它解决的问题是：

> “Kernel 只想申请几十、几百、几千字节，没必要浪费一整个 4KB 页。”

---

## 3. PPN 与物理地址

PPN（Physical Page Number，物理页号）表示第几个物理页。

因为：

PageSize = 4096 = 2^12

所以：

PPN = PhysicalAddress >> 12

PhysicalAddress = PPN << 12

例如：

PPN 256
→ 256 × 4096
→ 0x00100000
→ 1MB

---

## 4. FrameAllocator.Pages[]

每一个物理页使用 1 Byte 保存状态。

虽然 type.h 中的部分注释和实际代码行为相反，但根据真实函数逻辑，目前实际含义是：

bit0：

- 0 = Free
- 1 = Used

bit1：

- 1 = Kernel
- 0 = User

bit2：

- Dirty 状态位

因此学习代码时应以实际执行逻辑为准，而不能只相信旧注释。

---

## 5. FrameAllocator 初始化思想

globalFrameAllocatorInit() 的核心思想是：

> 默认所有物理页都不能使用，然后只释放 BIOS 明确告诉我们的可用页。

初始化后：

0 ~ 4MB：
- Used
- Kernel 类型

4MB 以后：
- Used
- User 类型

然后 MemoryCheckout() 解析 Loader 传来的 ARDS。

只处理：

- Type == 1
- BaseAddress >= 1MB

的区域。

之后：

1. 将 BaseAddress 向上对齐到 4KB。
2. 每隔 4KB 调用一次 FreeOnePage()。
3. 将真正可用的物理页从 Used 改成 Free。

这种方式比“默认所有内存都可用”安全，因为 Kernel 不会误用 BIOS 或其他保留区域。

---

## 6. AllocateOnePage()

AllocateOnePage(mode) 从 PPN 256 开始扫描。

PPN 256 对应：

0x00100000 = 1MB

因此低 1MB 不参与普通物理页分配。

找到一个满足：

- Free
- MachineMode 匹配

的页以后：

1. setUsed()
2. 更新空闲页计数
3. 清空这一页内容
4. 将 PPN 转换为物理地址
5. 返回物理地址

必须在返回之前 setUsed()，否则下一次分配可能再次找到同一个物理页。

---

## 7. FreeOnePage()

FreeOnePage(address) 是 AllocateOnePage() 的反过程。

首先要求：

address % 4096 == 0

因为 FrameAllocator 管理的是完整 4KB 页，不能从页面中间释放。

之后：

PhysicalAddress
→ PPN
→ setFree()
→ 更新空闲页数量

注意：

FreeOnePage()
是真正将物理页归还 FrameAllocator。

它和 MemoryFree() 不是一回事。

---

## 8. AllocatePagesContinuously()

这个函数寻找连续的 N 个物理页。

不能只判断“总共有 N 个 Free 页”，因为调用者需要的是地址连续的区域。

例如：

Free Free Used Free

虽然存在 3 个 Free 页，但不能作为连续 3 页返回。

找到连续区域后：

1. 将所有页标记为 Used。
2. 更新计数。
3. 清空整个连续区域。
4. 返回第一页面的物理地址。

BuddyAllocator 就通过该函数获得连续 16 页。

---

## 9. BuddyBlock

BuddyBlock 不是实际分配出去的内存。

它只是描述真实内存块的元数据：

- BaseAddress
- BlockSize
- IsUsed
- Next

因此：

Blocks[1024]

表示最多存在 1024 个描述符槽，
不是拥有 1024 块真实内存。

---

## 10. Buddy 初始化

Buddy 初始化时：

FrameAllocator
→ 连续申请 16 页
→ 64KB

Blocks[0] 主要作为链表哨兵节点，同时保存 Buddy 区域的基准信息。

Blocks[1] 才真正描述最初的：

64KB Free

初始链表：

Blocks[0]
→ Blocks[1] (64KB Free)
→ -1

Next 的含义：

- -1：链表最后一个节点
- 0：该 BuddyBlock 元数据槽未使用
- 其他值：下一个 BuddyBlock 的数组下标

---

## 11. Malloc()

Malloc(size) 首先使用 First Fit（首次适配）：

从链表头向后找第一个：

- IsUsed == FALSE
- BlockSize >= size

的块。

如果该块明显过大：

BlockSize >= 2 × size

则继续将它平均分成两个伙伴。

例如：

Malloc(1000)

可能经历：

64KB
→ 32KB
→ 16KB
→ 8KB
→ 4KB
→ 2KB
→ 1KB

最终得到：

1024 Byte

因为：

1024 >= 1000

但：

1024 < 2 × 1000

所以停止继续拆分。

最后将该块：

IsUsed = TRUE

并返回 BaseAddress。

---

## 12. Buddy 的 split

一次 split 不会移动或复制真实内存。

例如：

64KB

只是从：

一个 BuddyBlock 描述 64KB

变成：

一个 BuddyBlock 描述前 32KB
+
另一个 BuddyBlock 描述后 32KB

真实的 64KB 物理内存始终没有变化。

---

## 13. Free()

Free(address)：

1. 遍历 BuddyBlock 链表。
2. 找 BaseAddress == address 的块。
3. 将 IsUsed 设置为 FALSE。
4. 调用 mergeBuddy()。

注意：

Buddy Free()
并不会调用 FreeOnePage()。

因此内存只是重新回到 Buddy 的 64KB 内存池，
并没有归还 FrameAllocator。

---

## 14. 什么是真正的 Buddy

两个块满足：

- 相邻
- 大小相同
- 都是 Free

仍然不一定可以合并。

例如：

4KB
├─ 2KB
│  ├─ A 1KB
│  └─ B 1KB
└─ 2KB
   ├─ C 1KB
   └─ D 1KB

真正的伙伴是：

A ↔ B
C ↔ D

虽然 B 和 C：

- 相邻
- 大小相同

但它们不是同一个父块拆出来的，所以不能合并。

我目前对 Buddy 算法最重要的理解是：

> 怎么切出来，就必须怎么合并回去。

---

## 15. merge

理论上如果两个真正的伙伴都 Free：

1KB + 1KB
→ 2KB

2KB + 2KB
→ 4KB

...

32KB + 32KB
→ 64KB

整个过程中仍然不会复制真实内存。

主要修改的是：

- BlockSize
- Next
- BuddyBlock 元数据槽状态
