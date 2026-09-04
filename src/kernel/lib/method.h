#pragma once

#include "../common/method.h"
#include "../console/type.h"

Size Printf(const char *fmt, ...);
Size Println(const char *fmt, ...);
Size PrintWithColor(ConsoleColor color, const char *fmt, ...);
void Panic(const char *fmt, ...);

// Assertion
void AssertFailed(char *exp, char *file, char *base, i32 line);
#define Assert(exp) \
    if (!(exp)) AssertFailed(#exp, __FILE__, __BASE_FILE__, __LINE__)

// log
void DebugHandle(const char *file, i32 line, const char *fmt, ...);
void TraceHandle(const char *file, i32 line, const char *fmt, ...);
void InfoHandle(const char *file, i32 line, const char *fmt, ...);
void WarnHandle(const char *file, i32 line, const char *fmt, ...);
void ErrorHandle(const char *file, i32 line, const char *fmt, ...);

#define Debug(fmt, ...) DebugHandle(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Trace(fmt, ...) TraceHandle(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Info(fmt, ...) InfoHandle(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Warn(fmt, ...) WarnHandle(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define Error(fmt, ...) ErrorHandle(__FILE__, __LINE__, fmt, ##__VA_ARGS__)