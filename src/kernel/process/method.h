#pragma once

void InitializeProcessManager();
PID AllocatePID();
void FreePID(PID pid);
PCB* GetCurrentProcess();
void AddProcess(PCB* process);
void Schedule();

void CreateKernelProcess(void* entry);
void CreateUserProcess(void* entry);