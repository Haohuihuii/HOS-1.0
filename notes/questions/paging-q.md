# Paging Questions / Known Issues

> 本文件只记录 GOS Paging 当前存在的问题、设计限制和后续需要重新审视的地方。
>
> 已经确认并解决的问题不在这里展开记录。

---

# PAGING-001：MapPage 修改页表时关闭 Paging

当前：

```text
MapPage
 ↓
disablePaging
 ↓
findPTECreate
 ↓
enablePaging
 ↓
FlushTLB
```

当前实现关闭 Paging 后直接使用 Physical Address 访问 Page Table。

需要明确：

> x86 并没有要求修改 Page Table 时必须关闭 Paging。

真实操作系统通常会：

```text
Paging 保持开启
       ↓
Page Table 本身映射进 Kernel Virtual Address Space
       ↓
Kernel 修改 Page Table
       ↓
Flush TLB
```

当前方案属于教学实现上的简化。

以后可以研究：

```text
Recursive Page Mapping（递归页表映射）
```

或者其他 Kernel Page Table Mapping 方案。

---

# PAGING-002：Page Table Physical Page 使用 UserMode 分配

当前 `findPTECreate()` 中：

```c
PhysicalAddress secondPageTable =
    AllocateOnePage(UserMode);
```

也就是说：

```text
保存 Page Table 本身的 Physical Page
```

从 UserMode 页框池获取。

但：

```text
Page Table
```

本身属于 Kernel 管理的数据结构。

而最终：

```text
PTE → Physical Page
```

指向的页面才真正属于用户数据。

因此以后需要重新审视：

```text
Page Table Physical Page
是否应该 AllocateOnePage(KernelMode)
```

当前实现能够运行，但 KernelMode / UserMode 的设计语义需要统一。

---

# PAGING-003：Protection Fault 当前直接 Panic

当前：

```text
Page Fault
 ↓
Error Code.P == 1
 ↓
Panic
```

能够避免无限 Page Fault 循环，但处理方式仍然简单。

未来应该区分：

```text
Kernel Protection Fault
→ Kernel Panic
```

与：

```text
User Protection Fault
→ Terminate Current Process
```

即：

> 一个用户程序非法访问内存，不应该直接让整个操作系统停止。

这个问题应在进程退出机制完成后继续处理。

---

# PAGING-004：Demand Paging 不判断 Virtual Address 是否合法

当前逻辑接近：

```text
addr >= 4MB
+
Page Not Present
 ↓
MapPage(addr)
```

这意味着一个尚未映射的高地址可能直接得到 Physical Page。

但真正的 Virtual Memory 应该知道：

```text
这个地址属于什么区域？
```

例如：

```text
Program Code
Program Data
Heap
Stack
Shared Memory
File Mapping
Illegal Address
```

当前 GOS 尚没有完整的：

```text
VMA（Virtual Memory Area，虚拟内存区域）
```

描述机制。

因此目前 Demand Paging 只能视为：

```text
简单 Anonymous Demand Paging
```

以后需要限制哪些 Virtual Address 可以自动分配。

---

# PAGING-005：缺少完整 UnmapPage

当前已经实现：

```text
MapPage
```

但是还没有对应完整的：

```text
UnmapPage
```

以后在以下场景中需要：

```text
Process Exit
fork 失败回滚
exec
释放 User Stack
释放 Heap
销毁 User Address Space
```

一个完整的 UnmapPage 至少需要考虑：

```text
找到 PDE / PTE
 ↓
清除 PTE
 ↓
Flush TLB
 ↓
FreeOnePage
 ↓
判断 Page Table 是否已经为空
 ↓
必要时释放 Page Table
```

---

# PAGING-006：缺少完整 Address Space Destroy

当前用户进程拥有独立 Page Directory。

但是以后进程退出时必须正确释放：

```text
用户 Physical Page
Page Table
Page Directory
```

同时不能错误释放：

```text
Kernel Shared Page Table
```

因此进程退出阶段需要明确：

```text
哪些 PDE 是 Kernel Shared
哪些 PDE 是 Process Private
```

---

# PAGING-007：Kernel Mapping 和 User Mapping 的权限需要继续检查

当前初始化 Page Table 时部分条目使用：

```text
User = TRUE
```

未来需要重新审查：

```text
Kernel Code
Kernel Data
Kernel Stack
Page Table
User Code
User Data
User Stack
```

各自应该具有怎样的：

```text
Present
Write
User
```

权限。

特别是：

> Kernel 页面是否应该允许 User Mode 直接访问。

这是后续权限隔离的重要问题。

---

# PAGING-008：用户栈目前依赖固定地址

当前用户栈顶部固定：

```text
0x10000000
```

当前只映射固定的一页：

```text
0x0FFFF000
~
0x0FFFFFFF
```

以后需要考虑：

```text
多页 User Stack
Stack Growth
Stack Overflow
Guard Page
```

例如可以利用 Page Fault：

```text
访问合法 Stack 下方页面
 ↓
自动扩展 Stack
```

但是必须限制最大栈大小。

---

# PAGING-009：Page Fault Error Code 目前只使用 P 位

目前已经读取 Page Fault Error Code。

当前主要判断：

```text
bit 0：P
```

但以后还需要利用：

```text
W/R
→ Read / Write

U/S
→ User / Supervisor
```

这样才能输出和处理更准确的异常信息。

例如：

```text
User Write Protection Fault
Kernel Read Protection Fault
User Not-Present Page
```

应该采取不同策略。

---

# PAGING-010：当前 Demand Paging 没有 Copy-On-Write

当前以后实现 `fork()` 时，可以先采用：

```text
复制父进程 Physical Page
→ 子进程拥有独立 Physical Page
```

第一版不需要立即实现：

```text
COW（Copy-On-Write，写时复制）
```

未来可以优化为：

```text
Parent / Child
       ↓
共享只读 Physical Page
       ↓
发生 Write Page Fault
       ↓
复制 Physical Page
       ↓
分别建立映射
```

COW 可以显著减少 `fork()` 时的页面复制。

---

# PAGING-011：当前没有 Swap 和 Page Replacement

当前所有有效页面都必须真实存在于 Physical Memory。

尚未实现：

```text
Swap（交换空间）
Page Replacement（页面置换）
```

因此当前系统不存在：

```text
Physical Memory 不够
 ↓
选择页面换出磁盘
 ↓
之后 Page Fault 再换入
```

这不是当前 GOS 第一版本的必要目标。

---

# PAGING-012：当前没有 File-backed Paging

当前 Page Fault 创建页面时主要：

```text
AllocateOnePage
```

也就是创建新的匿名页。

未来实现文件系统和程序加载后，可以研究：

```text
Executable File
      ↓
Page Fault
      ↓
读取对应 File Block
      ↓
加载到 Physical Page
      ↓
建立 Mapping
```

即：

```text
Lazy Loading
File-backed Page
```

目前暂不实现。

---

# PAGING-013：Page Fault Handler 命名与接口风格

目前新增：

```text
pageFaultHandler
InitializePageFaultHandler
```

后续整理 Interrupt 模块时，可以统一：

```text
Exception Handler 命名
Interrupt Handler 命名
初始化函数命名
```

保持整个 Kernel 风格一致。

优先级较低。

---

# PAGING-014：需要建立 Paging 自动化测试

目前验证方式主要是：

```text
QEMU
+
手工测试
+
GDB
```

后续可以增加专门测试：

```text
Test 1：
访问未映射地址
→ 自动建立映射

Test 2：
连续访问同一个地址
→ 不重复 Page Fault

Test 3：
写入数据后重新读取
→ 数据一致

Test 4：
Null Pointer
→ 正确异常

Test 5：
Protection Fault
→ 正确拒绝

Test 6：
两个 Process 使用相同 Virtual Address
→ 对应不同 Physical Page
```

便于后续修改 `fork / exit / exec` 时进行回归测试。

---

# 当前优先级

建议后续处理顺序：

```text
Process / Child Process
        ↓
fork / exit / wait
        ↓
需要释放地址空间时
        ↓
补 UnmapPage / Address Space Destroy
        ↓
重新审视 UserMode / KernelMode Page Table 分配
        ↓
重新审视 User / Write 权限
```

以下功能暂时不作为 GOS 第一版必要目标：

```text
Copy-On-Write
Swap
Page Replacement
File-backed Paging
完整 VMA
```