#pragma once
#include <ntdef.h>
#include <ntifs.h>
#include <intrin.h>
#include <stdint.h>

typedef __int64(__fastcall* t_FreePool)(__int64, __int64, __int64);
typedef (__fastcall* AllocatePagesFunc_t)(IN SIZE_T Size, IN ULONG Node);

extern NTSTATUS AdjustRegisters();

// NTSTATUS NTAPI CopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress, PEPROCESS TargetProcess, PVOID TargetAddress,
//     SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, PSIZE_T ReturnSize);

NTKERNELAPI PPEB NTAPI GetProcessPeb(IN PEPROCESS Process);
NTSYSAPI PVOID RtlGetFileHeader(PVOID PcValue, PVOID* BaseOfImage);

NTSTATUS QuerySystemInfo(
    _In_ ULONG SystemInfoClass,
    _Inout_ PVOID SystemInfo,
    _In_ ULONG SystemInfoLength,
    _Out_opt_ PULONG ReturnLength
);

typedef struct
{
    uintptr_t selfReference;
    uintptr_t targetPid;     // PID cílového procesu
    uintptr_t destinationAddr;
    int commandId;          // akce k provedení
    char operationType;     // 0 = čtení, 1 = zápis
    unsigned char* sourceAddr;
    SIZE_T size;
} Command;

#define CMD_READWRITE     0xB16B00B5
#define CMD_GETPROCPID    0xBADA55
#define CMD_ISLOADED      0x69420

typedef struct _PROCESS_MODULE_INFO
{
    HANDLE SectionHandle;
    PVOID MappedBaseAddress;
    PVOID ImageBaseAddress;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
} PROCESS_MODULE_INFO, * PPROCESS_MODULE_INFO;

typedef struct _PROCESS_MODULES
{
    ULONG ModuleCount;
    PROCESS_MODULE_INFO Modules[1];
} PROCESS_MODULES, * PPROCESS_MODULES;
