#pragma once

#define EOF -1
#define NULL 0
#define TRUE 1
#define FALSE 0

typedef unsigned char Boolean;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef char i8;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef char* String;
typedef unsigned int Size;

typedef unsigned int PhysicalAddress;
typedef unsigned int VirtualAddress;
typedef unsigned int PhysicalPageNumber;

typedef unsigned int PID;

// 可变参数列表
typedef char* va_list;

#define StructNotAlign __attribute__((packed));

// 机器模式
typedef enum MachineMode {
    KernelMode,
    UserMode
} MachineMode;