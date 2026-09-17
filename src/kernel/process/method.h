#pragma once
void InitializeProcessManager();

PID AllocatePID();
void FreePID(PID pid);

PCB* GetCurrentProcess();
PCB* GetProcessByPID(PID pid);

void RegisterProcess(PCB* process);
void AddProcess(PCB* process);

void ExitProcess(i32 exitCode);

void Schedule();

void CreateKernelProcess(void* entry);
void CreateUserProcess(void* entry);