# HOS — GOS Learning & Development Workspace

## Project Background

本项目基于**获得授权的 GOS 教学操作系统源码**进行学习、整理与后续开发。

- 当前仓库的初始提交（`chore: import current GOS study baseline`）是本人开始独立维护时的**代码快照**。
- 初始快照包含：原 GOS 基础源码、本人此前学习阶段加入的部分源码注释、以及本人撰写的学习文档（learning-log、memory、interrupt、paging 等）。
- 初始快照**不代表**其中全部源码均为本人原创，原 GOS 实现来源于授权使用的教学项目。
- 原项目剩余模块（Process、Syscall、Disk IO、Filesystem、Shell 等）将按照本人学习进度**逐模块导入**，并遵循 `AGENTS.md` 中的 Git 提交规范以区分代码来源。

## Development Plan

- **v1.0 以前**：完整理解、注释整理并验证原 GOS 系统；逐模块导入尚未学习的上游模块。
- **v1.0 以后**：开始本人的独立功能扩展、Bug 修复与架构改进。
- 未来的 `v1.0-study-baseline` 里程碑仅表示“完整 GOS 学习与复现基线”，不表示本人从零原创整个操作系统。

---

## 学习顺序
学原理
→ 看当前实现
→ 设计
→ 实现
→ 测试
→ Debug
→ 更新 docs
→ 更新 learning-log
→ commit
////

0. 整体架构
      ↓
1. Boot / Protected Mode
      ↓
2. Console + 基础运行环境
      ↓
3. Physical Memory + Frame Allocator + Buddy
      ↓
4. GDT / IDT / PIC / Interrupt
      ↓
5. Paging                
      ↓
6. Process / Scheduler
      ↓
7. TSS / Ring3 / Context Switch
      ↓
8. Syscall
      ↓
==============================
==============================
      ↓
9. 开始继续开发新功能

////
- [Architecture](docs/architecture.md)
- [Boot](docs/boot.md)
- [Memory Management](docs/memory.md)
- [Paging](docs/paging.md)
- [Interrupts](docs/interrupt.md)
- [Processes](docs/process.md)
- [System Calls](docs/syscall.md)
- [Disk](docs/disk.md)
- [Filesystem](docs/filesystem.md)
