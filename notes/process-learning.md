- 

- ```
  ## 1. 当前理解的基础进程管理主线
  
  目前我理解的 GOS 基础进程管理主线是：
  
  PCB / PID
  → 创建进程
  → 将进程设置为 RUNNABLE
  → AddProcess() 放入 Runnable Queue
  → Schedule() 选择 next
  → 必要时更新 TSS.ESP0
  → 必要时切换 CR3
  → SwitchProcess()
  → 切换 Kernel Stack
  → 恢复 next 的执行现场
  → CPU 开始执行 next
  
  如果是 Kernel Process：
  
  SwitchProcess()
  → ret
  → entry
  
  如果是 User Process：
  
  SwitchProcess()
  → ret
  → restore()
  → InterruptContext
  → RestoreContext
  → Ring3
  → entry
  
  所以目前最重要的理解是：
  
  > PCB 只是 Kernel 管理进程的“档案”，真正让进程重新运行起来的关键，是它自己的页表、Kernel Stack，以及保存在栈上的 Context。
  
  ---
  
  ## 2. PID 与 PCB 的区别
  
  PID（Process Identifier，进程标识符）只是一个编号。
  
  它解决的是：
  
  > “这个进程是谁？”
  
  例如：
  
  PID = 3
  
  只能说明这是编号为 3 的进程。
  
  但是 Kernel 还需要知道：
  
  - 这个进程现在是什么状态
  - 它是 User Process 还是 Kernel Process
  - 它的 Kernel Stack 在哪里
  - 它使用哪张 Root Page Table
  - 被切换回来时从哪里恢复执行
  
  因此还需要 PCB。
  
  当前基础版 PCB：
  
  ```c
  typedef struct PCB {
      PhysicalAddress* KernelStackPointer;
      PID ID;
      ProcessState Status;
      ProcessType Type;
      u32 RootPPN;
  } PCB;
  ```

  当前理解：

  KernelStackPointer
   → 保存这个进程当前的 Kernel Stack Pointer

  ID
   → 进程 PID

  Status
   → RUNNING / RUNNABLE / BLOCKED

  Type
   → USER / KERNEL

  RootPPN
   → 当前进程 Root Page Table 所在物理页号

  所以：

  PID
   → “身份证号”

  PCB
   → “Kernel 管理这个进程需要的档案”

  ------

  ## 3. PIDAllocator

  GOS 使用 Bitmap 管理 PID。

  ```
  #define MAX_PROCESS_COUNT 1024
  
  typedef struct PIDAllocator {
      u32 Bitmap[MAX_PROCESS_COUNT / 32];
  } PIDAllocator;
  ```

  一个 u32 有 32 bit。

  因此：

  Bitmap[0]
   → PID 0 ~ 31

  Bitmap[1]
   → PID 32 ~ 63

  ...

  一共：

  1024 / 32 = 32 个 u32

  当前 Bitmap 的含义：

  bit = 0
   → 这个 PID 没有被占用

  bit = 1
   → 这个 PID 已经分配给某个进程

  AllocatePID() 会遍历：

  i
   → 第几个 u32

  j
   → 当前 u32 中第几个 bit

  如果：

  ```
  (pidAllocator.Bitmap[i] & (1 << j)) == 0
  ```

  说明该 bit 还没有被使用。

  然后：

  ```
  pidAllocator.Bitmap[i] |= (1 << j);
  ```

  把这个 bit 设置成 1。

  最后：

  ```
  return i * 32 + j;
  ```

  得到真正 PID。

  例如：

  i = 2
   j = 5

  PID
   = 2 * 32 + 5
   = 69

  需要继续注意：

  > `current->ID` 本身就是 PID 数值，不是 Bitmap 中的某个 bit。

  ------

  ## 4. FreePID()

  FreePID()：

  ```
  pidAllocator.Bitmap[pid / 32] &= ~(1 << (pid % 32));
  ```

  例如：

  PID = 35

  则：

  35 / 32 = 1

  35 % 32 = 3

  所以 PID 35 对应：

  Bitmap[1] 的 bit3

  通过：

  ```
  ~(1 << 3)
  ```

  构造一个 bit3 为 0 的掩码。

  再：

  ```
  &=
  ```

  把该 bit 清零。

  最终：

  bit 1
   → bit 0

  说明该 PID 再次变为空闲。

  ------

  ## 5. ProcessState

  当前基础版只有三种状态：

  ```
  PROCESS_STATE_RUNNABLE
  PROCESS_STATE_RUNNING
  PROCESS_STATE_BLOCKED
  ```

  ### RUNNING

  表示：

  > 当前正在 CPU 上运行。

  在当前单核模型中，正常情况下只有当前 `ProcessManager.Current` 对应的进程处于 RUNNING。

  ### RUNNABLE

  表示：

  > 已经具备运行条件，但是当前 CPU 正在运行其他进程。

  所以：

  RUNNABLE
   ≠ 正在运行

  而是：

  RUNNABLE
   = 可以运行、正在等 CPU

  ### BLOCKED

  表示：

  > 当前暂时不具备继续执行的条件。

  例如从通用 OS 概念看，进程可能在等待：

  - IO
  - 键盘输入
  - 某个事件
  - 子进程
  - 某个资源

  当前这批基础源码只定义了 BLOCKED，并没有完整展示 wait / wakeup 机制。

  因此当前只确认它的状态意义。

  ------

  ## 6. ProcessType

  当前有：

  ```
  PROCESS_TYPE_USER
  PROCESS_TYPE_KERNEL
  ```

  它不只是一个普通标签。

  因为 User Process 和 Kernel Process 在运行环境上差别很大。

  Kernel Process：

  - 本身就在 Ring0
  - 不需要 Ring3 → Ring0 的跨特权级切换
  - 第一次启动不需要构造用户态返回现场
  - 只需要 SwitchContext 即可开始运行

  User Process：

  - 正常运行在 Ring3
  - 系统调用 / 中断时需要进入 Ring0
  - 每个 User Process 需要自己的 Kernel Stack
  - 调度到 User Process 时需要更新 TSS.ESP0
  - 第一次进入 Ring3 需要 InterruptContext + RestoreContext

  ------

  ## 7. ProcessManager

  当前：

  ```
  typedef struct ProcessManager {
      PCB* Current;
      PCB* RunnableProcesses[MAX_PROCESS_COUNT];
      u32 Front;
      u32 Rear;
  } ProcessManager;
  ```

  其中：

  ### Current

  ```
  PCB* Current;
  ```

  表示：

  > 当前被进程管理器认为正在运行的进程 PCB。

  `GetCurrentProcess()`：

  ```
  PCB* GetCurrentProcess() {
      return processManager.Current;
  }
  ```

  所以以后 Fork 时：

  ```
  PCB* parent = GetCurrentProcess();
  ```

  本质就是：

  > 当前是谁正在执行 Fork，谁就是 Parent。

  ### RunnableProcesses[]

  它保存的是：

  PCB*

  不是把 PCB 本身再复制一份。

  里面放的是：

  > 可以运行，但是当前还没有获得 CPU 的进程。

  ------

  ## 8. Runnable Queue

  GOS 当前用一个循环队列管理 RUNNABLE Process。

  Front：

  > 下一个要从哪里取出进程。

  Rear：

  > 下一个新进程要插入哪里。

  需要特别记：

  > Rear 指向的是“下一个插入位置”，不是“最后一个有效元素”。

  空：

  ```
  Front == Rear
  ```

  满：

  ```
  (Rear + 1) % MAX_PROCESS_COUNT == Front
  ```

  之所以故意空一个位置，是因为如果所有槽位都用满：

  Front == Rear

  既可能表示：

  - Empty
  - Full

  无法区分。

  因此当前循环队列最多实际容纳：

  MAX_PROCESS_COUNT - 1

  个 PCB 指针。

  ------

  ## 9. AddProcess()

  核心：

  ```
  RunnableProcesses[Rear] = process;
  Rear = (Rear + 1) % MAX_PROCESS_COUNT;
  ```

  它只完成：

  > 把 `process` 对应的 PCB 指针插到队尾。

  它不会做：

  ```
  process->Status = PROCESS_STATE_RUNNABLE;
  ```

  因此：

  状态变化

  和

  进入 Runnable Queue

  是两个不同动作。

  例如 Schedule() 中：

  ```
  current->Status = PROCESS_STATE_RUNNABLE;
  AddProcess(current);
  ```

  先：

  RUNNING
   → RUNNABLE

  然后：

  PCB
   → Runnable Queue

  ------

  ## 10. fetchProcess()

  fetchProcess()：

  1. 判断 Queue 是否为空
  2. 从 Front 取出 PCB*
  3. 将原位置写为 NULL
  4. Front 前移
  5. 返回 PCB*

  例如：

  Runnable Queue：

  P1 → P2 → P3

  Front 指向 P1。

  执行：

  fetchProcess()

  结果：

  result = P1

  Queue：

  P2 → P3

  所以：

  AddProcess()
   → Rear 插入

  fetchProcess()
   → Front 取出

  构成 FIFO：

  First In First Out
   先进先出

  ------

  ## 11. InitializeProcessManager()

  初始化时：

  ```
  processManager.Current = NULL;
  ```

  说明：

  > 一开始还没有由 ProcessManager 管理的当前运行进程。

  然后清空：

  ```
  RunnableProcesses[]
  ```

  再：

  ```
  Front = Rear = 0;
  ```

  表示队列为空。

  之后：

  ```
  CreateKernelProcess(idle);
  idleProcess = fetchProcess();
  ```

  先创建 idle Kernel Process，再马上从 Runnable Queue 取出，单独保存到 `idleProcess`。

  当前基础版 `process.c` 中后续没有看到完整的 `idleProcess` fallback 使用逻辑。

  这个问题需要后面继续关注。

  ------

  ## 12. Schedule() 的第一种情况

  ```
  if (processManager.Current == NULL && isEmpty()) {
      return;
  }
  ```

  说明：

  Current == NULL

  并且：

  Runnable Queue == Empty

  也就是说：

  - 没有当前进程
  - 也没有待运行进程

  此时 Scheduler 没有任何事情可做。

  所以直接：

  return

  ------

  ## 13. Schedule() 的第二种情况

  ```
  if (processManager.Current == NULL) {
      runFirstProcess();
      return;
  }
  ```

  程序能够走到这里，说明前一个判断没有成立。

  现在又满足：

  Current == NULL

  因此可以推出：

  Runnable Queue != Empty

  也就是：

  > 还没有 Current Process，但已经有一个或多个 Runnable Process。

  这是第一次正式启动某个进程的特殊情况。

  因为普通 SwitchProcess() 的逻辑需要：

  current
   +
   next

  但是第一次没有真正的 current。

  所以需要：

  runFirstProcess()

  单独处理。

  ------

  ## 14. Schedule() 中 current 的处理

  普通调度时：

  ```
  PCB* current = processManager.Current;
  ```

  此时：

  current
   → 调度开始前正在运行的旧进程

  然后：

  ```
  if (current->ID && current->Status != PROCESS_STATE_BLOCKED) {
      current->Status = PROCESS_STATE_RUNNABLE;
      AddProcess(current);
  }
  current->ID
  ```

  不是在检查 Bitmap bit。

  C 中：

  0 → false

  非 0 → true

  所以：

  ```
  if (current->ID)
  ```

  等价于：

  ```
  if (current->ID != 0)
  ```

  由于初始化时第一个 CreateKernelProcess(idle) 会拿到 PID 0，因此这里明显是在把 PID 0 的特殊进程排除在普通重新入队逻辑之外。

  同时：

  ```
  current->Status != PROCESS_STATE_BLOCKED
  ```

  保证 BLOCKED Process 不会重新加入 Runnable Queue。

  如果 current 是正常运行进程：

  RUNNING
   → RUNNABLE
   → AddProcess(current)
   → 放回队尾

  ------

  ## 15. 分析 Schedule() 必须区分时间点

  这是学习过程中容易混淆的地方。

  例如开始：

  Current → P1(RUNNING)

  Runnable Queue：

  P2 → P3

  执行：

  ```
  P1.Status = RUNNABLE;
  AddProcess(P1);
  ```

  此时临时变成：

  Current 仍然指向 P1

  但是：

  P1.Status = RUNNABLE

  Queue：

  P2 → P3 → P1

  这只是 `Schedule()` 内部执行到一半的中间状态。

  不能把它理解成系统稳定状态。

  接着：

  ```
  PCB* next = fetchProcess();
  ```

  得到：

  next = P2

  Queue：

  P3 → P1

  再：

  ```
  next->Status = RUNNING;
  processManager.Current = next;
  ```

  最终稳定状态：

  Current → P2(RUNNING)

  Queue：

  P3 → P1(RUNNABLE)

  以后分析调度代码必须明确：

  > “执行到哪一行之后？”

  不能把不同时间点的状态放在一起比较。

  ------

  ## 16. Schedule() 中 next

  ```
  PCB* next = fetchProcess();
  ```

  表示：

  > 从 Runnable Queue 队头选出下一个要运行的进程。

  例如：

  Queue：

  P2 → P3 → P1

  则：

  next = P2

  然后 Queue 变成：

  P3 → P1

  需要记：

  > `fetchProcess()` 之后，next 已经不再位于 Runnable Queue 中。

  ------

  ## 17. TSS.ESP0

  如果 next 是 User Process：

  ```
  if (next->Type == PROCESS_TYPE_USER) {
      SetTSSEsp0(...);
  }
  ```

  TSS：

  Task State Segment

  当前 GOS 主要利用 TSS 中：

  SS0
   ESP0

  解决：

  > User Process 从 Ring3 进入 Ring0 时，CPU 应该切到哪个 Kernel Stack。

  TSS.ESP0 不表示：

  > 现在立即修改 CPU 当前 ESP。

  而是提前告诉 CPU：

  > “如果这个 User Process 之后发生 Ring3 → Ring0，请使用这个 Kernel Stack Top。”

  ------

  ## 18. 为什么 TSS.ESP0 要随 User Process 调度更新

  假设：

  P1 Kernel Stack Top
   = A

  P2 Kernel Stack Top
   = B

  当前 P1 在 Ring3。

  此时：

  TSS.ESP0 = A

  当 Scheduler 切换到 P2 后，如果没有修改：

  TSS.ESP0

  它仍然等于：

  A

  那么 P2 以后：

  Ring3
   → int 0x80
   → Ring0

  CPU 可能进入 P1 的 Kernel Stack。

  这就会造成不同进程的内核现场混在一起。

  所以调度到每个 User Process 前，都需要：

  # TSS.ESP0

  这个 next User Process 的 Kernel Stack Top

  ------

  ## 19. SetTSSEsp0() 中的向上对齐

  源码：

  ```
  ((u32)next->KernelStackPointer + PageSize - 1)
      / PageSize * PageSize
  ```

  本质：

  ALIGN_UP(KernelStackPointer, PageSize)

  假设：

  Kernel Stack 页面：

  0x00123000
   ~
   0x00123FFF

  高地址边界：

  0x00124000

  而当前：

  # KernelStackPointer

  0x00123F80

  那么向上按 4096 对齐：

  0x00123F80
   → 0x00124000

  得到真正 Kernel Stack Top。

  为什么必须向上？

  因为 x86 栈向低地址增长。

  如果 ESP0 设置为：

  0x00124000

  第一次 CPU 压 4 Bytes：

  ESP
   → 0x00123FFC

  正好进入这一页。

  如果向下对齐到：

  0x00123000

  那得到的是页面底部，不适合作为栈顶。

  ## 20. RootPPN 与 CR3

  PCB：

  ```
  u32 RootPPN;
  ```

  表示：

  > Root Page Table 所在的 Physical Page Number。

  PPN
   → GetAddressFromPPN()
   → Root Page Table Physical Address

  CPU 的 CR3：

  > 保存当前使用的 Root Page Table Physical Address。

  因此 Schedule()：

  ```
  if (next->RootPPN != processManager.Current->RootPPN) {
      SetRootPageTableAddr(GetAddressFromPPN(next->RootPPN));
  }
  ```

  表示：

  如果 current 与 next 使用不同 Root Page Table：

  就需要切换 CR3。

  这样：

  同一个 Virtual Address

  在 next 运行时

  才能按照 next 自己的 Page Table 做地址翻译。

  ------

  ## 21. runFirstProcess()

  第一次启动进程时：

  Current == NULL

  不存在真正 current PCB。

  但：

  ```
  SwitchProcess(current, next)
  ```

  接口要求 current 和 next 两个参数。

  因此：

  ```
  PCB unused;
  PCB* unusedPtr = &unused;
  ```

  构造一个临时 PCB。

  它不是要被真正调度运行的进程。

  它只是：

  > 给 SwitchProcess 一个可以存放“旧 ESP”的地址。

  然后：

  ```
  SwitchProcess(unusedPtr, next);
  ```

  真正目标仍然是：

  next

  这个 unused 以后不需要再恢复。

  ------

  ## 22. SwitchContext

  结构：

  ```
  typedef struct SwitchContext {
      u32 EDI;
      u32 ESI;
      u32 EBX;
      u32 EBP;
      u32 EIP;
  } SwitchContext;
  ```

  它用于进程上下文切换。

  核心思想：

  > 真正的执行现场保存在每个进程自己的 Kernel Stack 上。

  PCB 不直接保存：

  EDI
   ESI
   EBX
   EBP
   EIP

  PCB 只保存：

  KernelStackPointer

  通过：

  KernelStackPointer
   → 找到 Kernel Stack
   → 找到 SwitchContext

  ------

  ## 23. SwitchProcess() 刚进入时的栈

  C 代码：

  ```
  SwitchProcess(current, next);
  ```

  源码中没有显式写：

  ```
  call SwitchProcess
  ```

  这是 C 编译器生成调用代码时产生的行为。

  在当前 32 位栈传参模型下，可以理解为：

  参数先进入调用栈，

  然后 call 自动压入 Return Address。

  刚进入 SwitchProcess 时：

  [ESP]
   → Return Address

  [ESP + 4]
   → current

  [ESP + 8]
   → next

  需要继续注意：

  > 这里的 `call` 是编译后的调用机制，不是 `process.c` 源文件里直接写出来的一行汇编。

  ------

  ## 24. SwitchProcess() 为什么 current 在 [esp + 4*5]

  进入 SwitchProcess 后：

  ```
  push ebp
  push ebx
  push esi
  push edi
  ```

  又压入 4 个 32 位寄存器。

  一共：

  4 * 4 = 16 Bytes

  所以新的 ESP 比刚进入函数时低 16 Bytes。

  此时：

  ESP + 0
   → EDI

  ESP + 4
   → ESI

  ESP + 8
   → EBX

  ESP + 12
   → EBP

  ESP + 16
   → Return Address

  ESP + 20
   → current

  ESP + 24
   → next

  因此：

  ```
  [esp + 4*5]
  ```

  就是：

  current

  而：

  ```
  [esp + 4*6]
  ```

  就是：

  next

  ------

  ## 25. mov [eax], esp

  源码：

  ```
  mov eax, [esp + 4*5]
  mov [eax], esp
  ```

  第一句：

  # EAX

  current PCB 地址

  当前 PCB：

  ```
  typedef struct PCB {
      PhysicalAddress* KernelStackPointer;
      ...
  } PCB;
  ```

  KernelStackPointer 是 PCB 第一个字段。

  因此：

  [eax]

  直接对应：

  current->KernelStackPointer

  所以：

  ```
  mov [eax], esp
  ```

  等价于：

  ```
  current->KernelStackPointer = ESP;
  ```

  作用：

  > 把 current 当前保存好寄存器后的 Kernel Stack Pointer 记录进 PCB。

  ------

  ## 26. 为什么 PCB 第一个字段很重要

  如果 PCB 是：

  ```
  typedef struct PCB {
      PID ID;
      PhysicalAddress* KernelStackPointer;
  } PCB;
  ```

  那么：

  PCB + 0
   → ID

  PCB + 4
   → KernelStackPointer

  这时：

  ```
  mov [eax], esp
  ```

  会错误覆盖 ID。

  必须改成类似：

  ```
  mov [eax + 4], esp
  ```

  当前代码能直接：

  ```
  mov [eax], esp
  ```

  就是因为 KernelStackPointer 在 offset 0。

  这说明：

  > `switch.asm` 与 PCB 的字段布局存在直接耦合。

  以后修改 PCB 字段顺序时需要非常小心。

  ------

  ## 27. mov esp, [eax]

  接下来：

  ```
  mov eax, [esp + 4*6]
  ```

  此时：

  # EAX

  next PCB 地址

  然后：

  ```
  mov esp, [eax]
  ```

  由于：

  # [eax]

  next->KernelStackPointer

  所以：

  # ESP

  next->KernelStackPointer

  执行前：

  ESP
   → current 的 Kernel Stack

  执行后：

  ESP
   → next 的 Kernel Stack

  这是真正完成：

  > 从 old Kernel Stack 切到 new Kernel Stack

  的关键一步。

  ------

  ## 28. 为什么切 ESP 就能切 Context

  因为寄存器现场已经放在 Kernel Stack 上。

  current Kernel Stack：

  EDI
   ESI
   EBX
   EBP
   Return EIP

  next Kernel Stack：

  EDI
   ESI
   EBX
   EBP
   Return EIP

  只要：

  ESP

  从 current 的栈换成 next 的栈，

  之后的：

  ```
  pop edi
  pop esi
  pop ebx
  pop ebp
  ret
  ```

  就全部从 next 的 Kernel Stack 取数据。

  所以：

  > SwitchProcess 并不是复制一整块 Stack，而只是保存旧 ESP，再换成新的 ESP。

  ------

  ## 29. call / ret 与 EIP

  SwitchContext 中有：

  ```
  u32 EIP;
  ```

  但 switch.asm 没有：

  ```
  push eip
  ```

  原因是：

  > C 调用 `SwitchProcess()` 时，call 已经把 Return Address 保存到 Kernel Stack。

  这个 Return Address 表示：

  > 当前进程以后恢复时应该从哪里继续执行。

  因此它本质上承担了保存 EIP 的作用。

  以后 next 被恢复时：

  ```
  ret
  ```

  本质可以理解为：

  EIP = [ESP]

  ESP += 4

  于是 CPU 从 next 自己保存的 Return Address 继续执行。

  ------

  ## 30. CreateKernelProcess()

  Kernel Process 创建主线：

  Malloc PCB
   → AllocatePID()
   → Status = RUNNABLE
   → Allocate Kernel Stack
   → Type = KERNEL
   → Allocate Root Page Table
   → Copy Kernel Mapping
   → Construct SwitchContext
   → KernelStackPointer = SwitchContext
   → AddProcess()

  刚创建的进程：

  Status = RUNNABLE

  因为：

  > 已经准备运行，但还没有真正获得 CPU。

  ------

  ## 31. Kernel Stack 为什么 + PageSize

  代码：

  ```
  u32 stack = AllocateOnePage(KernelMode) + PageSize;
  ```

  假设：

  # AllocateOnePage()

  0x00123000

  这一页：

  0x00123000
   ~
   0x00123FFF

  但是栈向低地址增长。

  所以初始 stack 设为：

  0x00124000

  后面：

  stack -= sizeof(...)

  或者：

  push

  才会进入：

  0x00123000 ~ 0x00123FFF

  这一页。

  ------

  ## 32. Kernel Process 为什么要构造 Root Page Table

  Kernel Process 也有：

  ```
  RootPPN
  ```

  创建时：

  1. AllocateOnePage(KernelMode)
  2. 得到 Root Page Table Page
  3. 保存 RootPPN
  4. MemoryCopy 当前 Root Page Table 一整页

  当前源码注释说明：

  > 复制 Kernel 前 4MB 的等值映射。

  因此新 Kernel Process 在自己的 Root Page Table 中仍然拥有当前基础 Kernel Mapping。

  ------

  ## 33. Kernel Process 为什么要伪造 SwitchContext

  新进程以前从来没有运行过。

  因此它没有真正的：

  “上一次被切走时保存的上下文”。

  但是 SwitchProcess() 恢复 next 时固定执行：

  pop edi
   pop esi
   pop ebx
   pop ebp
   ret

  所以 CreateKernelProcess() 提前人为构造：

  EDI = 0
   ESI = 0
   EBX = 0
   EBP = 0
   EIP = entry

  并让：

  KernelStackPointer
   → 这个 SwitchContext

  以后第一次：

  SwitchProcess
   → ESP = KernelStackPointer
   → pop
   → ret
   → entry

  于是新 Kernel Process 就像“以前被切走过，现在恢复回来”一样启动。

  ------

  ## 34. CreateUserProcess() 与 Kernel Process 的区别

  User Process 前半部分和 Kernel Process 类似：

  - PCB
  - PID
  - RUNNABLE
  - Kernel Stack
  - Root Page Table

  但是还需要：

  - User Stack
  - User Page Mapping
  - InterruptContext
  - SwitchContext
  - restore()

  因为 User Process 最终要运行在：

  Ring3

  而 Kernel Process 一直在：

  Ring0

  ------

  ## 35. User Stack

  当前：

  ```
  #define UserStackTop 0x10000000
  ```

  这个值是：

  > User Stack 的高地址边界。

  x86 栈向低地址增长。

  所以：

  ESP = 0x10000000

  第一次：

  push 4 Bytes

  变成：

  ESP = 0x0FFFFFFC

  因此真正需要映射的是：

  0x0FFFF000
   ~
   0x0FFFFFFF

  而不是：

  0x10000000
   ~
   0x10000FFF

  ------

  ## 36. User Stack 的 PDE / PTE

  用户栈页起始地址：

  ```
  stackPageBase = UserStackTop - PageSize;
  ```

  得到：

  0x0FFFF000

  然后：

  ```
  pdeIndex = stackPageBase >> 22;
  ```

  得到：

  63

  再：

  ```
  pteIndex = (stackPageBase >> 12) & 0x3FF;
  ```

  得到：

  1023

  所以：

  Virtual Address 0x0FFFF000

  → PDE[63]

  → Page Table 63

  → PTE[1023]

  → User Stack Physical Page

  ------

  ## 37. 新二级页表为什么要 MemoryFree()

  源码：

  ```
  u32 secondPT = AllocateOnePage(KernelMode);
  MemoryFree((void*)secondPT, PageSize);
  ```

  当前项目中的：

  ```
  void MemoryFree(void* ptr, Size size)
  ```

  实际实现是：

  逐 Byte 写 0。

  所以它并不是真正 Free Memory。

  而是：

  > 把这张新 Page Table 整页清零。

  这意味着：

  所有 PTE
   → Present = 0

  然后再只设置：

  PTE[1023]

  建立 User Stack Mapping。

  因此当前最需要记：

  > `MemoryFree()` 这个名字有误导性，它本质类似 `memset(ptr, 0, size)`。

  ------

  ## 38. User Process 为什么同时需要 User Stack 和 Kernel Stack

  User Stack：

  > 用户程序在 Ring3 运行时使用。

  例如：

  - 用户函数调用
  - 用户局部变量
  - 用户 push / pop

  Kernel Stack：

  > 这个 User Process 进入 Ring0 后使用。

  例如：

  - int 0x80 System Call
  - Interrupt
  - 保存 Kernel Context
  - 执行 Kernel Handler

  所以：

  User Process
   并不是只有一个 Stack。

  而是：

  Ring3
   → User Stack

  Ring0
   → Kernel Stack

  ------

  ## 39. InterruptContext

  当前 InterruptContext 包含：

  - General Registers
  - Segment Registers
  - ErrCode
  - EIP
  - CS
  - PSW / EFLAGS
  - ESP3
  - SS3

  它的作用是：

  > 构造或保存完整的中断返回现场。

  对于新 User Process：

  它没有真的从 Ring3 被中断过。

  所以 CreateUserProcess() 人为伪造一个 InterruptContext，

  让 RestoreContext 以后可以像：

  > “这个进程以前本来就在 Ring3，只是现在从中断返回”

  一样把它送入 User Mode。

  ## 40. ESP3 与 SS3

  ### ESP3

  `ESP3` 不是另一个 CPU 寄存器。

  真实 CPU 只有：

  ESP

  ```
  InterruptContext.ESP3
  ```

  只是作者起的字段名。

  表示：

  > 返回 Ring3 后 CPU 的 ESP 应该恢复成什么值。

  当前：

  ```
  ESP3 = UserStackTop
  ```

  也就是：

  0x10000000

  ### SS3

  SS：

  Stack Segment

  ```
  SS3
  ```

  表示：

  > 返回 Ring3 后使用的 User Stack Segment Selector。

  当前：

  ```
  SS3 = UserDataSegmentSelector
  ```

  所以进入 User Mode 后，可以理解为：

  # SS

  UserDataSegmentSelector

  # ESP

  UserStackTop

  ------

  ## 41. ESP / ESP0 / ESP3 的区别

  这是目前最容易混的三个概念。

  ### ESP

  CPU 当前真实的 Stack Pointer Register。

  CPU 此刻在哪个栈上，

  ESP 就指向哪个位置。

  ### TSS.ESP0

  用于：

  Ring3
   → Ring0

  告诉 CPU：

  > 进入 Kernel Mode 时从哪个 Kernel Stack Top 开始。

  ### InterruptContext.ESP3

  用于：

  Ring0
   → Ring3

  告诉恢复流程：

  > 回到 User Mode 时 User Stack Pointer 应该恢复成什么值。

  所以方向是：

  ESP3
   → 返回用户态

  ESP0
   → 进入内核态

  ------

  ## 42. User Process 的两层 Context

  User Process 创建时有两层：

  ### SwitchContext

  用于：

  > Scheduler 把 CPU 切到这个进程。

  设置：

  ```
  SwitchContext.EIP = restore;
  ```

  ### InterruptContext

  用于：

  > 把这个已经获得 CPU 的进程从 Ring0 送到 Ring3。

  设置：

  ```
  InterruptContext.EIP = entry;
  ```

  因此：

  SwitchContext.EIP
   ≠ InterruptContext.EIP

  前者：

  restore

  后者：

  user entry

  ------

  ## 43. 为什么不能 SwitchContext.EIP = entry

  如果 User Process 直接：

  ```
  SwitchContext.EIP = entry;
  ```

  那么 SwitchProcess 最后的：

  ret

  只会：

  EIP = entry

  但是这并没有自动完成：

  - CS → User Code Segment
  - SS → User Data Segment
  - ESP → User Stack
  - Ring0 → Ring3

  所以 CPU 仍然处于当前 Kernel Context。

  因此必须先：

  ret → restore()

  再利用 InterruptContext + RestoreContext

  完成真正的用户态恢复。

  ------

  ## 44. restore()

  文件：

  ```
  src/kernel/process/create.c
  ```

  核心：

  ```
  static void restore() {
      PCB* current = GetCurrentProcess();
  
      u32 stack =
          ((u32)current->KernelStackPointer + PageSize - 1)
          / PageSize * PageSize;
  
      stack -= sizeof(InterruptContext);
  
      asm volatile ("movl %0, %%esp" : : "m"(stack));
      asm volatile ("jmp RestoreContext");
  }
  ```

  第一步：

  ```
  PCB* current = GetCurrentProcess();
  ```

  拿到当前已经被 Schedule() 设置为 Current 的 User Process PCB。

  第二步：

  ```
  ALIGN_UP(current->KernelStackPointer, PageSize)
  ```

  重新找到 Kernel Stack Top。

  第三步：

  ```
  stack -= sizeof(InterruptContext);
  ```

  重新找到之前构造好的 InterruptContext。

  第四步：

  ```
  ESP = stack
  ```

  让 CPU 当前 Kernel ESP 精确指向 InterruptContext。

  第五步：

  ```
  jmp RestoreContext
  ```

  不使用 call，

  避免额外压入 Return Address 干扰当前已经安排好的栈布局。

  所以：

  restore()

  本质是：

  > 从 SwitchContext 使用的进程切换现场，过渡到 InterruptContext 使用的中断返回现场。

  ------

  ## 45. User Process 第一次启动完整过程

  CreateUserProcess(entry)

  → Allocate PCB

  → Allocate PID

  → Status = RUNNABLE

  → Allocate Kernel Stack

  → Allocate Root Page Table

  → Build User Stack Mapping

  → Construct InterruptContext

  其中：

  EIP = entry

  CS = UserCodeSegmentSelector

  ESP3 = UserStackTop

  SS3 = UserDataSegmentSelector

  → Construct SwitchContext

  其中：

  EIP = restore

  → KernelStackPointer 指向 SwitchContext

  → AddProcess()

  → Schedule()

  → fetchProcess() 选中这个 User Process

  → SetTSSEsp0()

  → SetRootPageTableAddr()

  → next = RUNNING

  → Current = next

  → SwitchProcess()

  → ESP = next->KernelStackPointer

  → pop

  → ret

  → restore()

  → 找到 InterruptContext

  → ESP = InterruptContext

  → jmp RestoreContext

  → 恢复 User Context

  → Ring3

  → EIP = entry

  → ESP = UserStackTop

  → User Process 正式开始运行

  ------

  ## 46. Kernel Process 与 User Process 的第一次启动对比

  ### Kernel Process

  创建：

  SwitchContext.EIP = entry

  运行：

  Schedule
   → SwitchProcess
   → pop
   → ret
   → entry

  一直处于：

  Ring0

  ### User Process

  创建：

  SwitchContext.EIP = restore

  InterruptContext.EIP = entry

  运行：

  Schedule
   → SwitchProcess
   → pop
   → ret
   → restore
   → RestoreContext
   → Ring3
   → entry

  所以：

  Kernel Process
   只需要解决“怎么开始执行”

  User Process
   还需要额外解决“怎么进入 Ring3”

  ------

  ## 47. 当前已经发现的源码问题 / 疑点

  ### MemoryFree() 命名误导

  名字：

  MemoryFree

  实际：

  Memory Clear / Zero Fill

  更像：

  memset(ptr, 0, size)

  后续整理代码质量问题时值得记录。

  ### idleProcess

  InitializeProcessManager() 中：

  ```
  CreateKernelProcess(idle);
  idleProcess = fetchProcess();
  ```

  但是当前基础版 `process.c` 后续没有看到完整使用 `idleProcess` 的逻辑。

  而普通 Schedule()：

  ```
  PCB* next = fetchProcess();
  ```

  如果 Queue 为空：

  fetchProcess()
   → Panic("Queue is empty")

  因此 idle fallback 设计当前看起来并没有完全体现出来。

  ### AddProcess() 不检查 Status

  AddProcess() 不会确认：

  process->Status == RUNNABLE

  所以 Runnable Queue 的正确性依赖调用者。

  ### fetchProcess() 不检查 Status

  fetchProcess() 也不会确认：

  取出的 PCB 是否真的 RUNNABLE。

  ### PCB 布局与汇编强耦合

  SwitchProcess() 直接：

  ```
  mov [eax], esp
  ```

  依赖：

  KernelStackPointer

  必须处于 PCB offset 0。

  以后修改 PCB 结构字段顺序时可能破坏汇编。

  ### Schedule() 的中间状态

  Schedule() 内部会出现：

  Current 仍指 old process

  但：

  old.Status 已经变成 RUNNABLE

  的短暂中间状态。

  后续需要继续确认调度是否依赖不可重入 / 关中断等条件。

  ------

  ## 48. Fork 前最需要继续巩固的知识

  ### PCB

  必须能说清：

  KernelStackPointer
   → Kernel Context 的入口

  RootPPN
   → Address Space 的入口

  ### Schedule()

  必须能说清：

  current 怎么重新入队

  next 怎么从队头取出

  什么时候修改 TSS.ESP0

  什么时候修改 CR3

  最后怎么 SwitchProcess

  ### SwitchProcess()

  必须能说清：

  push 保存什么

  current 参数为什么在 `[esp + 4*5]`

  next 参数为什么在 `[esp + 4*6]`

  ```
  mov [eax], esp
  ```

  和：

  ```
  mov esp, [eax]
  ```

  分别是谁写给谁

  ### Context

  必须能区分：

  SwitchContext
   → Process Switch

  InterruptContext
   → Interrupt / User Mode Restore

  ### User Process 第一次启动

  必须能独立说出：

  SwitchProcess
   → restore
   → InterruptContext
   → RestoreContext
   → Ring3
   → entry

  ------

  ## 49. 当前阶段最重要的总体理解

  目前我对 GOS Process 模块最重要的理解是：

  > 进程切换并不是“复制一个进程”或者“直接把 CPU 指向另一个函数”，而是 Kernel 通过 PCB 找到每个进程自己的 Kernel Stack 和 Root Page Table，再通过切换 CR3 与 ESP，把 CPU 所使用的地址空间和执行现场切换成另一个进程的环境。

  对于新建进程：

  > 因为它以前从未运行过，所以没有真实保存的旧 Context。Kernel 就人为伪造一个 Context，让 `SwitchProcess()` 把它当成“以前被切走、现在恢复”的进程。

  对于 User Process：

  > 只完成 Process Switch 还不够，还要从 Ring0 进入 Ring3，因此需要第二层 InterruptContext，并通过 `restore()` 把 SwitchContext 和 InterruptContext 两套栈布局连接起来。