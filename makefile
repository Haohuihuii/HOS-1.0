AsmCompile=nasm
CCompile=gcc
IMG=img/gos.img

# ⚠️ 导师特供：帮你把 -w (关闭警告) 换回了 -Wall (开启警告)！永远不要对 Bug 闭上眼睛！
GccFlags=-m32 -fno-builtin -fno-stack-protector -march=pentium
GccFlags+=-Wall -nostdinc -nostdlib -fno-pic -fno-pie -g
# ⚠️ 增加了 -e _start 明确入口点
LdFlags=-m elf_i386 -static -e _start

TARGET=target
BootLoader=src/bootloader
KernelPath=src/kernel

ELFKernel=$(TARGET)/kernel/os.elf
NakedKernel=$(TARGET)/kernel/os.bin

# 自动搜寻所有的 .c 和 .asm 文件
KernelSourceFile=$(wildcard $(KernelPath)/*.c) $(wildcard $(KernelPath)/*.asm)
KernelSourceFile+=$(wildcard $(KernelPath)/*/*.c) $(wildcard $(KernelPath)/*/*.asm)

# 🚨 核心修复：在操作系统链接时，entry.o 必须是第一个文件，否则机器码顺序错乱会直接死机！
ENTRY_OBJ=$(TARGET)/kernel/entry.o
OTHER_ASM_SRCS=$(filter-out $(KernelPath)/entry.asm, $(filter %.asm, $(KernelSourceFile)))
C_SRCS=$(filter %.c, $(KernelSourceFile))

# 重新组装对象文件列表，让 ENTRY_OBJ 打头阵
KernelOBJ=$(ENTRY_OBJ)
KernelOBJ+=$(patsubst $(KernelPath)/%.asm, $(TARGET)/kernel/%.o, $(OTHER_ASM_SRCS))
KernelOBJ+=$(patsubst $(KernelPath)/%.c, $(TARGET)/kernel/%.o, $(C_SRCS))

# 🌟 强硬编码 User 文件，彻底杜绝找不到文件的玄学 Bug
USER_OBJS = $(TARGET)/user/user_process.o

ENTRYPOINT=0x7e00

.PHONY: all run build debug clean

all: build

run: build
	qemu-system-i386 -m 32M \
		-drive file=$(IMG),if=ide,index=0,media=disk,format=raw

build: $(TARGET) $(IMG)

# 🌟 动态生成顶级目录即可，子目录靠编译规则自动生成
$(TARGET):
	@mkdir -p $(TARGET)

# ----- .c, .asm ---> .o -----
# 🌟 @mkdir -p $(dir $@) 会在编译每个文件前自动建好对应的目标文件夹！
$(TARGET)/kernel/%.o: $(KernelPath)/%.c
	@mkdir -p $(dir $@)
	$(CCompile) $(GccFlags) -c -o $@ $<

$(TARGET)/kernel/%.o: $(KernelPath)/%.asm
	@mkdir -p $(dir $@)
	$(AsmCompile) -f elf32 -g $< -o $@ 

# 🌟 新增 user 目录的编译规则
$(TARGET)/user/%.o: src/user/%.c
	@mkdir -p $(dir $@)
	$(CCompile) $(GccFlags) -c -o $@ $<

$(TARGET)/user/%.o: src/user/%.asm
	@mkdir -p $(dir $@)
	$(AsmCompile) -f elf32 -g $< -o $@ 
# ----------------------------

# ----- bootloader -----------
$(TARGET)/bootloader/%.bin: $(BootLoader)/%.asm
	@mkdir -p $(dir $@)
	$(AsmCompile) -o $@ $<
# ----------------------------

# ----- kernel made ---------- 
# 🌟 把 USER_OBJS 塞进链接名单里
$(ELFKernel): $(KernelOBJ) $(USER_OBJS)
	ld $(LdFlags) -Ttext $(ENTRYPOINT) $^ -o $@

$(NakedKernel): $(ELFKernel)
	objcopy -O binary $< $@
# ----------------------------

# ----- img made -------------
$(IMG): $(TARGET)/bootloader/boot.bin $(TARGET)/bootloader/loader.bin $(NakedKernel)
	@mkdir -p img
	@if [ ! -f $(IMG) ]; then \
		bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $(IMG); \
	fi
	dd if=$(word 1, $^) of=$@ bs=512 count=1 conv=notrunc status=none
	dd if=$(word 2, $^) of=$@ bs=512 count=3 seek=1 conv=notrunc status=none
	dd if=$(word 3, $^) of=$@ bs=512 count=250 seek=4 conv=notrunc status=none
# ----------------------------

debug: build 
	qemu-system-i386 -m 32M \
		-drive file=$(IMG),if=ide,index=0,media=disk,format=raw \
		-s -S \
		-display none
clean:
	rm -rf $(TARGET) img
	rm -rf *.bin *.o *.lock *.ini *.s *.asm