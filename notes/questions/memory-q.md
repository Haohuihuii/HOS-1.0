# Physical Memory

## MEM-001 AllocatePagesContinuously boundary check

**Status:** Confirmed code issue / Runtime trigger unverified

当前内部检查：

```c
isFree(i + j);
isSpecifiedMode(i + j, mode);
```

但外层只保证：

```text
i < MAX_PAGE_COUNT
```

没有保证：

```text
i + pageCount <= MAX_PAGE_COUNT
```

因此如果候选起点靠近 `Pages[]` 尾部，`i + j` 可能超过数组范围。

### 建议后续测试

- 人为申请接近 `MAX_PAGE_COUNT` 边界的连续页。
- 增加断言或边界检查，保护 `pageCount` 和 `i + j`。

---

## MEM-002 E820 result may exceed FrameAllocator capacity

**Status:** Suspected integration issue / Runtime unverified

当前：

```text
MAX_PAGE_COUNT = 8192
PageSize = 4KB
```

因此 `Pages[]` 最多描述：

```text
8192 × 4KB = 32MB
```

但 `MemoryCheckout()` 会遍历 BIOS E820 返回的可用区域，目前没有看到对：

```text
PPN < MAX_PAGE_COUNT
```

的边界限制。

如果虚拟机配置的物理内存超过 32MB，需要确认是否可能访问：

```text
Pages[MAX_PAGE_COUNT ...]
```

### 待验证

- 当前 QEMU 实际配置的物理内存大小。
- 当前 BIOS E820 返回的最大可用内存范围。
- `MemoryCheckout()` 实际访问到的最大 PPN。

---

## MEM-003 FrameAllocator bit comments conflict with implementation

**Status:** Confirmed documentation issue

`type.h` 注释描述：

```text
bit0: 1 free / 0 used
bit1: 0 kernel / 1 user
```

但实际代码行为为：

```text
bit0: 0 free / 1 used
bit1: 1 kernel / 0 user
```

当前实现逻辑内部基本一致，主要问题是旧注释会误导源码阅读。

### 后续建议

- 修正 `type.h` 中对应注释。
- 保证位定义只有一个权威来源。
- 技术文档与源码注释保持一致。

---

## MEM-004 isDirty semantics

**Status:** Confirmed code inconsistency / Currently unused

当前：

```c
setDirty();
```

会将 `bit2` 置为 `1`。

但：

```c
isDirty();
```

却在 `bit2 == 0` 时返回 `TRUE`。

因此两者语义相反。

目前 Dirty 相关函数没有在已经学习的 `frame.c` 主路径中实际使用。

### 后续建议

进入 Paging（分页）模块后重新检查：

- Dirty 位在当前 GOS 中是否真的需要。
- `isDirty()` 的预期语义。
- 是否应该修改为 `bit2 == 1` 时返回 `TRUE`。

---

# Buddy Allocator

## BUDDY-001 Free invalid address

**Status:** Confirmed code issue / Runtime trigger unverified

`Free(address)` 遍历链表寻找：

```text
BaseAddress == address
```

如果没有找到：

```text
p == -1
```

随后代码仍然执行：

```c
getBuddyBlock(p)->IsUsed = FALSE;
```

等价于访问：

```text
Blocks[-1]
```

因此存在数组越界访问。

### 后续建议

- 在 `p == -1` 时直接 `Panic` 或 `Assert`。
- 增加非法地址 `Free()` 测试。
- 增加重复释放等异常场景测试。

---

## BUDDY-002 mergeBuddy buddy detection

**Status:** Confirmed algorithm issue / Runtime trigger unverified

当前判断中使用：

```text
offset % BlockSize == 0
```

但这只能证明当前块按照自身大小对齐，不能证明两个相邻块来自同一个父块。

例如：

```text
A B C D
```

四个块均为 1KB 时：

```text
A + B 是伙伴
C + D 是伙伴
B + C 不是伙伴
```

虽然 `B` 和 `C`：

- 地址相邻
- 大小相同
- 都可能处于 Free 状态

但它们并不是由同一个父块拆出来的，因此不能合并。

当前判断可能错误允许：

```text
B + C
```

发生 merge。

### 正确判断应表达原始二分关系

一种思路是要求左块满足：

```text
offset % (2 × BlockSize) == 0
```

另一种 Buddy 算法中的常见方法是：

```text
buddy_offset = offset XOR BlockSize
```

通过异或关系寻找真正的伙伴块。

### 后续建议

- 为 `mergeBuddy()` 编写专门测试。
- 构造 `A Used / B Free / C Free / D Used` 场景。
- 验证当前实现是否会错误合并 `B + C`。
- 最终根据测试结果重新实现伙伴判断。

---

## BUDDY-003 BuddyBlock initialization loop

**Status:** Suspicious implementation

`BuddyAllocator` 中：

```text
BUDDY_BLOCKS = 1024
BUDDY_PAGES  = 16
```

但 `globalBuddyAllocatorInit()` 显式初始化 `BuddyBlock` 时使用：

```c
i < BUDDY_PAGES
```

因此只显式初始化前 16 个 `BuddyBlock` 描述符。

由于：

```c
BuddyAllocator globalBuddyAllocator;
```

是全局对象，其余静态存储区当前通常会被零初始化，因此该问题可能暂时没有造成运行故障。

### 需要确认

- 作者原意是否应为：

```c
i < BUDDY_BLOCKS
```

- 修改后是否影响现有行为。
- 是否存在代码逻辑依赖“未显式初始化但默认全零”的情况。

---

## BUDDY-004 Malloc(0)

**Status:** Edge case / Runtime unverified

`Malloc()` 使用：

```text
while BlockSize >= 2 × size
```

如果：

```text
size == 0
```

则：

```text
2 × size == 0
```

循环条件可能持续成立。

这可能导致：

```text
64KB
→ 32KB
→ 16KB
→ ...
→ 不断 split
```

最终可能：

- 耗尽 `BuddyBlock` 描述符。
- 产生大小异常的内存块。
- 进入未定义或错误状态。

### 后续建议

明确规定：

```c
Malloc(0)
```

的行为。

例如：

- 直接返回 `0`。
- 或通过 `Assert` / `Panic` 拒绝零字节申请。

---

# Design Limitations

以下属于当前设计限制，不作为 Bug。

## Buddy Heap size

当前 Buddy Heap 固定为：

```text
16 pages × 4KB = 64KB
```

启动时由 `FrameAllocator` 一次性提供。

当前没有动态扩容机制。

也就是说：

> Kernel 的 Buddy Heap 当前固定为 64KB。

详细设计见：

```text
docs/memory.md
```

---

## FrameAllocator range

当前 `FrameAllocator` 元数据最多只能描述：

```text
8192 pages × 4KB
= 32MB
```

物理内存。

该容量本身属于当前设计选择。

真正需要调查的问题是：

> 当 BIOS E820 返回超过 32MB 的物理内存时，`MemoryCheckout()` 是否能够安全处理。

该问题记录于：

```text
MEM-002
```