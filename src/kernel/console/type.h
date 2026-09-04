#pragma once

#include "../common/type.h"

typedef struct Cursor {
    u32 Row, Col; // current Cursor position
} Cursor;

typedef enum ConsoleColor {
    BLACK,
    BLUE,
    GREEN,
    CYAN,
    RED,
    PURPLE,
    YELLOW,
    WHITE,
    GRAY,
    LIGHT_BLUE,
    LIGHT_GREEN,
    LIGHT_CYAN,
    LIGHT_RED,
    LIGHT_PURPLE,
    LIGHT_YELLOW,
    BRIGHT_WHITE
} ConsoleColor;

// CathodeRayTube Monitor (CRT)
#define MonitorAddressRegister 0x3D4
#define MonitorDataRegister 0x3D5

#define ScreenAddressHigh 0xC
#define ScreenAddressLow 0xD
#define CursorAddressHigh 0xE
#define CursorAddressLow 0xF

#define MonitorBaseAddress 0xB8000
#define MonitorSize 0x4000
#define ScreenWidthByte (80 * 2) // 80 chars per line, but 160 bytes per lin
#define ScreenWidth 80
#define ScreenHigh 25
#define ScreenSize (ScreenWidthByte * ScreenHigh)

#define NullChar 0x00
#define BS 0x08 // \b
#define HT 0x09 // \t
#define LF 0x0A // \n
#define CR 0x0D // \r
#define Space 0x20