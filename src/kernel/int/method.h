#pragma once

#include "../common/type.h"

void InitializeInterrupt();
void InitializePageFaultHandler();
void SetInterruptHandler(u32 vector, void* handler);
void SetInterrupt(u32 vector);
void OuteralInterruptCompleted(u32 vector);
u8 GetInterruptStatus();
void RestoreInterruptStatus(u8 status);