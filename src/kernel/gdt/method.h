#pragma once

#include "../common/type.h"

void InitializeGDT();
void SetTSSEsp0(u32 esp0);