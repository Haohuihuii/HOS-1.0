## BOOT-001 Real-mode stack initialization

boot.asm 和 loader.asm 在实模式阶段使用了：

call
ret
push
pop

但当前代码未看到显式初始化 SS:SP。

当前 QEMU 环境可以正常启动，
但代码可能依赖 BIOS 遗留的栈状态。

需要后续通过调试确认启动时 SS 和 SP 的实际值，
并判断是否应该由 Boot Sector 主动初始化。


## BOOT-002 Real-mode DS initialization

Loader 使用：

mov si, loader_message

随后通过：

mov al, [si]

读取字符串。

实模式下实际地址为 DS:SI。

当前代码未看到进入 Loader 后显式初始化 DS = 0，
因此可能依赖 BIOS 或 Boot 阶段遗留的 DS 状态。

当前 QEMU 可以正常运行。

后续使用调试器确认进入 Loader 时 DS 的实际值。

BUDDY-001
Free() 找不到地址时仍访问 Blocks[-1]
→ 确认代码缺陷

BUDDY-002
mergeBuddy() 伙伴判断条件过宽
→ 可能把相邻但并非伙伴的同大小 Free 块错误合并
→ 确认算法缺陷

BUDDY-003
globalBuddyAllocatorInit() 初始化循环使用 BUDDY_PAGES=16，
但元数据数组大小是 BUDDY_BLOCKS=1024
→ 可疑；全局对象零初始化目前可能掩盖问题

BUDDY-004
Buddy Heap 固定只有 64KB
→ 当前设计限制，不是 Bug


