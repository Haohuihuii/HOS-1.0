#pragma once

#include "../common/type.h"
#include "../console/type.h"
#include "../int/type.h"

#define stack_size(t) (sizeof(t) <= sizeof(char *) ? sizeof(char *) : sizeof(t))
#define va_start(ap, v) (ap = (va_list)&v + sizeof(char *))
#define va_arg(ap, t) (*(t *)((ap += stack_size(t)) - stack_size(t)))
#define va_end(ap) (ap = (va_list)0)

#define ZEROPAD 0x01
#define SIGN 0x02
#define PLUS 0x04
#define SPACE 0x08
#define LEFT 0x10
#define SPECIAL 0x20
#define SMALL 0x40
#define DOUBLE 0x80