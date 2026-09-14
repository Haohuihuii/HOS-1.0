| 命令             | 作用               |  例子     |
| ---------------- | ------------------ | ---------------------- |
| `p expr`         | 打印变量/表达式    | `-exec p process->ID`  |
| `p/x expr`       | 十六进制打印       | `-exec p/x process`    |
| `p *ptr`         | 看指针指向的结构   | `-exec p *process`     |
| `info locals`    | 看当前函数局部变量 | `-exec info locals`    |
| `info args`      | 看当前函数参数     | `-exec info args`      |
| `info registers` | 看 CPU 寄存器      | `-exec info registers` |
| `bt`             | 看调用栈           | `-exec bt`             |
| `frame n`        | 切到第 n 层调用栈  | `-exec frame 1`        |
| `x/... addr`     | 直接查看内存       | `-exec x/16wx $esp`    |
| `list`           | 看当前源码附近     | `-exec list`           |
| `disassemble`    | 看汇编             | `-exec disassemble`    |

寄存器：
-exec info registers
-exec p/x $esp
-exec p/x $ebp

内存：
-exec x/16wx $esp
x      = examine memory
16     = 看 16 个单位
w      = 每个单位 4 bytes，word
x      = 十六进制显示
$esp   = 从 ESP 指向的位置开始

-exec x/10i $eip : 从当前 EIP 开始，看接下来的 10 条机器指令。i = instruction

-exec continue or -exec c

vscode f10 = GDB next/n 

-exec finish 把当前函数执行完，回到调用这个函数的地方。

-exec si == step i
汇编调试： -exec ni


p / p/x
x
info args
info locals
info registers
bt
frame
b
watch
continue
next / step
ni / si


调试 CreateUserProcess() 时发现：
process = 0x100820
process->ID = 1
RunnableProcesses[Rear - 1] = 0x100820
ProcessTable[1] = NULL
CreateUserProcess遗漏了RegisterProcess(process)