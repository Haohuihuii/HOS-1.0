process.md

GOS 进程管理

当前内容只覆盖 Fork 之前的基础进程管理。

主要涉及：

src/kernel/process/type.h

src/kernel/process/process.c

src/kernel/process/switch.asm

src/kernel/process/create.c

Fork / Parent / Child / ParentID / Process Tree 将在后续继续补充。

1. 进程模块整体结构

当前基础进程模块主要分成四部分：

type.h
→ 定义 PCB、ProcessState、ProcessType、ProcessManager、SwitchContext、InterruptContext

process.c
→ PID 分配、Runnable Queue、Schedule()

switch.asm
→ 保存 old context、切换 Kernel Stack、恢复 next context

create.c
→ 创建 Kernel Process / User Process，并构造第一次运行所需的 Context

整体流程：

进程创建
→ PCB 初始化
→ 加入 Runnable Queue
→ Schedule() 选择 next
→ 切换 TSS / CR3
→ SwitchProcess()
→ 恢复 next 执行现场

2. PCB

当前基础版 PCB：

typedef struct PCB {
    PhysicalAddress* KernelStackPointer;
    PID ID;
    ProcessState Status;
    ProcessType Type;
    u32 RootPPN;
} PCB;

字段作用：

KernelStackPointer
→ 保存当前进程被切走时的 Kernel Stack Pointer

ID
→ PID

Status
→ 当前进程状态

Type
→ User Process / Kernel Process

RootPPN
→ 当前进程 Root Page Table 的 Physical Page Number

PCB 本身不是整个进程，只是 Kernel 管理进程的核心元数据。

3. PIDAllocator

GOS 使用 Bitmap 管理 PID。

#define MAX_PROCESS_COUNT 1024

typedef struct PIDAllocator {
    u32 Bitmap[MAX_PROCESS_COUNT / 32];
} PIDAllocator;

一个 u32 管理 32 个 PID。

规则：

bit = 0
→ PID 空闲

bit = 1
→ PID 已分配

PID 计算：

PID = i * 32 + j;

其中：

i
→ Bitmap 第几个 u32

j
→ 当前 u32 中第几个 bit

4. AllocatePID() 与 FreePID()

AllocatePID()：

遍历 Bitmap

找到第一个为 0 的 bit

将该 bit 设置为 1

返回对应 PID

核心：

if ((pidAllocator.Bitmap[i] & (1 << j)) == 0) {
    pidAllocator.Bitmap[i] |= (1 << j);
    return i * 32 + j;
}

FreePID()：

pidAllocator.Bitmap[pid / 32] &= ~(1 << (pid % 32));

作用：

将对应 bit 清零，使 PID 重新可用。

5. ProcessState

当前定义：

typedef enum ProcessState {
    PROCESS_STATE_RUNNABLE,
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_BLOCKED
} ProcessState;

含义：

RUNNING
→ 当前正在 CPU 上运行

RUNNABLE
→ 已具备运行条件，正在等待 CPU

BLOCKED
→ 当前暂时不能继续执行

当前基础源码定义了 BLOCKED，但没有完整展示 wait / wakeup 机制。

6. ProcessType

当前：

typedef enum ProcessType {
    PROCESS_TYPE_USER,
    PROCESS_TYPE_KERNEL
} ProcessType;

Kernel Process：

运行在 Ring0

不需要 User Stack

第一次启动只需 SwitchContext

User Process：

目标运行在 Ring3

需要 User Stack

需要 Kernel Stack

调度时需要更新 TSS.ESP0

第一次进入 Ring3 需要 InterruptContext + RestoreContext

7. ProcessManager

结构：

typedef struct ProcessManager {
    PCB* Current;
    PCB* RunnableProcesses[MAX_PROCESS_COUNT];
    u32 Front;
    u32 Rear;
} ProcessManager;

其中：

Current
→ 当前运行进程

RunnableProcesses[]
→ Runnable PCB 指针组成的循环队列

Front
→ 下一个取出位置

Rear
→ 下一个插入位置

8. Runnable Queue

队列为空：

Front == Rear

队列满：

(Rear + 1) % MAX_PROCESS_COUNT == Front

当前实现故意空出一个槽位，用来区分：

Empty

和

Full

因此数组长度虽然是 MAX_PROCESS_COUNT，循环队列本身最多容纳 MAX_PROCESS_COUNT - 1 个 PCB 指针。

9. AddProcess()

核心：

RunnableProcesses[Rear] = process;
Rear = (Rear + 1) % MAX_PROCESS_COUNT;

功能：

把一个 PCB 指针插入 Runnable Queue 尾部。

注意：

AddProcess()

不会自动：

process->Status = PROCESS_STATE_RUNNABLE;

状态修改和入队是两个独立动作。

10. fetchProcess()

核心流程：

判断 Queue 是否为空

取出 RunnableProcesses[Front]

原位置设为 NULL

Front 前移

返回 PCB*

因此：

AddProcess()
→ Rear 入队

fetchProcess()
→ Front 出队

整体是 FIFO。

11. InitializeProcessManager()

初始化主要完成：

processManager.Current = NULL;

清空：

RunnableProcesses[]

设置：

Front = Rear = 0;

然后：

CreateKernelProcess(idle);
idleProcess = fetchProcess();

当前基础版中，idle 被创建后单独保存，但后续没有看到完整 fallback 使用逻辑。

12. Schedule() 总体流程

一次普通调度可以概括为：

current = Current
    ↓
如果 current 不是 PID 0 且未 BLOCKED
    ↓
current: RUNNING → RUNNABLE
    ↓
AddProcess(current)
    ↓
next = fetchProcess()
    ↓
如果 next 是 USER
更新 TSS.ESP0
    ↓
如果 RootPPN 不同
切换 Root Page Table
    ↓
next: RUNNABLE → RUNNING
    ↓
Current = next
    ↓
SwitchProcess(current, next)

13. current 的特殊判断

源码：

if (current->ID && current->Status != PROCESS_STATE_BLOCKED) {
    current->Status = PROCESS_STATE_RUNNABLE;
    AddProcess(current);
}

current->ID

表示 PID 数值。

C 中：

0 → false

非 0 → true

所以这里等价于：

current->ID != 0

结合初始化顺序，可以看出 PID 0 的 idle 进程被当作特殊进程处理。

同时：

current->Status != PROCESS_STATE_BLOCKED

保证 BLOCKED Process 不会重新进入 Runnable Queue。

14. Schedule() 的中间状态

分析 Schedule() 时必须区分执行时间点。

例如调度开始：

Current → P1(RUNNING)

Queue：

P2 → P3

执行：

P1.Status = RUNNABLE;
AddProcess(P1);

后：

Current 仍然暂时指向 P1

但：

P1.Status = RUNNABLE

Queue：

P2 → P3 → P1

再执行：

next = fetchProcess();

得到：

next = P2

Queue：

P3 → P1

最终更新：

Current → P2(RUNNING)

所以中间状态不能当成最终稳定状态理解。

15. TSS.ESP0

当 next 是 User Process：

if (next->Type == PROCESS_TYPE_USER) {
    SetTSSEsp0(...);
}

作用：

提前告诉 CPU，这个 User Process 以后发生 Ring3 → Ring0 时，应该从哪个 Kernel Stack Top 开始使用内核栈。

它不会立即执行：

CPU.ESP = TSS.ESP0

只是更新 TSS 中的 ESP0。

16. KernelStackPointer 与 Kernel Stack Top

KernelStackPointer

不是固定的 Kernel Stack Top。

它表示：

进程被切走时保存下来的当前 Kernel ESP。

而 TSS.ESP0 需要的是：

该 Kernel Stack 页面的高地址边界。

所以使用：

((u32)next->KernelStackPointer + PageSize - 1)
    / PageSize * PageSize

将当前 KernelStackPointer 向上按 4KB 对齐。

17. RootPPN 与 CR3

RootPPN

表示当前进程 Root Page Table 的物理页号。

切换进程时：

if (next->RootPPN != processManager.Current->RootPPN) {
    SetRootPageTableAddr(GetAddressFromPPN(next->RootPPN));
}

如果 RootPPN 不同：

需要让 CPU 使用 next 的 Root Page Table。

本质上对应：

CR3
→ next Root Page Table Physical Address

这样 next 才使用自己的虚拟地址空间映射。

18. runFirstProcess()

第一次运行进程时：

Current == NULL
Runnable Queue != Empty

没有真正的 old process。

但 SwitchProcess() 接口仍要求：

SwitchProcess(current, next)

因此代码构造：

PCB unused;
PCB* unusedPtr = &unused;

让 SwitchProcess 可以把旧 ESP 写到一个临时位置。

这个 unused PCB 不会作为真正进程恢复。

19. SwitchContext

结构：

typedef struct SwitchContext {
    u32 EDI;
    u32 ESI;
    u32 EBX;
    u32 EBP;
    u32 EIP;
} SwitchContext;

它负责：

进程调度时最基本的 Kernel Context 保存与恢复。

真正的数据主要保存在：

Kernel Stack

PCB 只通过：

KernelStackPointer

记录这份 Context 在哪里。

20. SwitchProcess()

核心代码：

push ebp
push ebx
push esi
push edi

mov eax, [esp + 4*5]
mov [eax], esp

mov eax, [esp + 4*6]
mov esp, [eax]

pop edi
pop esi
pop ebx
pop ebp

ret

执行逻辑：

保存 current 的寄存器

保存 current ESP

读取 next PCB

切换 ESP 到 next Kernel Stack

恢复 next 寄存器

ret 恢复 next 的执行位置

21. mov [eax], esp 与 mov esp, [eax]

这是 SwitchProcess 最核心的两句。

保存 current

mov [eax], esp

此时：

EAX
→ current PCB

因为：

KernelStackPointer

是 PCB 第一个字段，

所以：

[eax]
=
current->KernelStackPointer

最终效果：

current->KernelStackPointer = ESP

恢复 next

mov esp, [eax]

此时：

EAX
→ next PCB

所以：

ESP = next->KernelStackPointer

执行后 CPU 已经使用 next 的 Kernel Stack。

22. call / ret 与 EIP

C 代码：

SwitchProcess(current, next);

源码中没有显式写 call，但编译器生成函数调用时会产生对应调用机制。

Return Address 会进入调用栈。

SwitchProcess 保存寄存器后，Kernel Stack 布局与 SwitchContext 对应。

因此：

SwitchContext.EIP

实际对应的是：

以后这个进程恢复后继续执行的位置。

ret：

从当前 ESP 指向的位置取得 Return Address，

并让 CPU 从该地址继续执行。

23. CreateKernelProcess()

主要过程：

Malloc PCB
→ AllocatePID
→ RUNNABLE
→ Allocate Kernel Stack
→ Type = KERNEL
→ Allocate Root Page Table
→ Copy Kernel Mapping
→ Construct SwitchContext
→ KernelStackPointer = Context
→ AddProcess

Kernel Stack 初始化：

u32 stack = AllocateOnePage(KernelMode) + PageSize;

因为 x86 栈向低地址增长，所以初始 stack 使用页面高地址边界。

24. Kernel Process 第一次启动

新进程从未真正运行过，没有历史 Context。

因此 CreateKernelProcess() 人工构造：

EDI = 0
ESI = 0
EBX = 0
EBP = 0
EIP = entry

并：

process->KernelStackPointer = stack;

第一次被调度时：

SwitchProcess
→ ESP = KernelStackPointer
→ pop
→ ret
→ entry

因此 Kernel Process 可以直接开始执行。

25. CreateUserProcess()

User Process 除了 PCB、PID、Kernel Stack、Root Page Table 外，还需要：

User Stack Mapping

InterruptContext

SwitchContext

restore()

原因是：

User Process 最终目标运行在 Ring3。

所以除了“切到这个进程”，还必须完成：

Ring0
→ Ring3

26. User Stack Mapping

当前：

#define UserStackTop 0x10000000

由于 x86 栈向低地址增长：

ESP = 0x10000000
push 4 Bytes
→ ESP = 0x0FFFFFFC

因此真正需要映射的是：

0x0FFFF000 ~ 0x0FFFFFFF

对应：

PDE[63]
→ PTE[1023]
→ User Stack Physical Page

新 Page Table 通过 MemoryFree() 清零。

需要注意：

MemoryFree()

实际功能是 Zero Fill，不是真正释放内存。

27. InterruptContext 与 ESP3 / SS3

User Process 创建时会构造：

InterruptContext

其中关键字段：

EIP
→ user entry

CS
→ User Code Segment Selector

PSW
→ EFLAGS

ESP3
→ UserStackTop

SS3
→ User Data Segment Selector

ESP3

不是独立 CPU 寄存器。

它只是 InterruptContext 中表示：

返回 Ring3 后 ESP 应恢复成什么值

的字段。

SS3

表示：

返回 Ring3 后使用的 User Stack Segment Selector。

28. User Process 的两层 Context

User Process 第一次启动需要两层。

SwitchContext

SwitchContext.EIP = restore;

作用：

先让 Scheduler 能把 CPU 切到这个进程。

InterruptContext

InterruptContext.EIP = entry;

作用：

再让这个进程从 Ring0 恢复到 Ring3。

所以：

SwitchContext
→ Process Switch

InterruptContext
→ User Mode Restore

29. restore()

文件：

src/kernel/process/create.c

核心过程：

GetCurrentProcess()
→ 找到当前 PCB

KernelStackPointer
→ 向上按页对齐
→ 找到 Kernel Stack Top

Kernel Stack Top
- sizeof(InterruptContext)
→ 找到 InterruptContext

ESP = InterruptContext

jmp RestoreContext

它的作用是：

把 SwitchContext 使用的进程切换现场，转换成 RestoreContext 所需要的 InterruptContext 栈布局。

30. User Process 第一次运行完整路径

完整主线：

CreateUserProcess(entry)
    ↓
构造 User Stack Mapping
    ↓
构造 InterruptContext
EIP = entry
ESP3 = UserStackTop
    ↓
构造 SwitchContext
EIP = restore
    ↓
AddProcess
    ↓
Schedule
    ↓
SetTSSEsp0
    ↓
必要时切 CR3
    ↓
SwitchProcess
    ↓
恢复 SwitchContext
    ↓
ret → restore
    ↓
ESP 指向 InterruptContext
    ↓
jmp RestoreContext
    ↓
Ring3
    ↓
EIP = entry
ESP = UserStackTop
    ↓
User Process 正式执行