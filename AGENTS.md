## Git Commit & Project Provenance Rules

本项目基于获得授权的 GOS 教学操作系统源码进行学习、整理与后续开发。

仓库中的 Git 历史必须清楚区分：

```
Upstream / Baseline Work
→ 原 GOS 项目已有源码或后续逐模块导入的原始实现

Personal Work
→ 本人编写的学习文档
→ 本人增加的源码注释
→ 本人完成的调试验证
→ 本人确认并修改的 Bug
→ 本人新增的功能
→ 本人进行的架构优化
```

### 1. 禁止混淆代码来源

任何 Agent 不得：

- 将原 GOS 已有实现描述为本人从零原创。
- 修改 README、CHANGELOG 或 Commit Message，使原始代码看起来像本人独立实现。
- 因为代码现在位于本人仓库，就自动将其归类为 Personal Work。
- 删除项目来源说明来制造错误的原创印象。

------

### 2. Upstream 模块必须独立提交

后续从原 GOS 项目导入尚未学习的模块时，应尽量单独提交。

推荐格式：

```
chore: import upstream process module

chore: import upstream syscall module

chore: import upstream disk io module

chore: import upstream filesystem module

chore: import upstream shell module
```

这类 commit 应尽量只包含原项目代码导入，不同时混入大量个人修改。

------

### 3. 学习内容独立提交

本人在学习过程中增加的内容，应与 Upstream Import 分开。

例如：

```
docs: annotate process scheduler

docs: add process learning notes

docs: document fork and process tree

docs: complete filesystem notes
```

源码注释属于学习工作，不应伪装成原始功能开发。

------

### 4. Bug Fix 必须有依据

只有在已经确认问题后，才使用：

```
fix:
```

推荐格式：

```
fix: correct process scheduler state transition

fix: prevent page allocator out-of-bounds access
```

如果只是怀疑存在问题，但尚未通过源码分析、测试、QEMU 或 GDB 确认，则优先记录到：

```
*-question.md
TODO.md
Issue
```

不要提前提交为 Bug Fix。

------

### 5. 新功能使用 `feat`

只有本人真正新增的功能才使用：

```
feat:
```

例如：

```
feat: implement copy-on-write fork

feat: add process wait syscall

feat: add elf user program loader
```

不得把原项目已有功能重新导入后写成：

```
feat: implement ...
```

除非该功能确实由本人重新设计和实现。

------

### 6. Commit 应保持单一职责

禁止将以下内容混在一个大型 commit：

```
导入 upstream 源码
+
增加学习注释
+
修改 Bug
+
新增功能
+
重构
+
更新文档
```

应尽可能拆分为：

```
chore
↓
docs
↓
fix
↓
feat
```

让 Git History 可以直接反映实际工作来源。

------

### 7. Commit Message Convention

优先使用：

```
chore: 项目维护、baseline、upstream 导入

docs: 文档、学习笔记、源码学习注释

fix: 已确认的问题修复

feat: 本人新增功能

refactor: 不改变外部行为的代码重构

test: 测试、QEMU/GDB 验证代码

build: Makefile、构建脚本、工具链相关
```

Commit Message 应描述实际变化，不夸大工作范围。

------

### 8. v1.0 前后的阶段定义

```
Before v1.0
→ 原 GOS 模块逐步导入
→ 系统性学习
→ 源码注释
→ 文档整理
→ 调试验证
→ 问题分析
v1.0-study-baseline
→ 完整 GOS 学习与复现基线
After v1.0
→ 独立功能开发
→ Bug 修复
→ 架构改进
→ 性能优化
→ 新机制实现
```

Agent 不得自行创建 `v1.0` Tag，必须等待用户明确确认。

------

### 9. Sensitive Information

任何情况下不得提交：

```
API Key
Access Token
Password
Private Key
.env
Credential
Secret
```

如果发现疑似敏感信息：

```
立即停止相关 commit / push
↓
只报告文件路径
↓
不要输出 Secret 原文
↓
等待用户处理
```

在没有用户明确要求前，不得：

```
修改 GitHub Repository Visibility
创建 Public Repository
Force Push
删除旧仓库
重写旧仓库 Git History
```

------

### 10. Agent 修改前的原则

在进行较大的 Git 操作、模块导入或代码重构之前，应先确认：

```
这个变化属于 Upstream Import？
Personal Documentation？
Bug Fix？
New Feature？
Refactor？
```

如果来源或类别无法判断，应先询问用户，不得自行归类。

The goal of this repository is to build a truthful and traceable record of my OS learning and development work. Git history must preserve the distinction between imported upstream code and work completed by me.