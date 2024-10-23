#pragma once
#include <ntdef.h>
#include <ntifs.h>
#include <intrin.h>
#include <stdint.h>

// Definice typu funkcí
typedef __int64(__fastcall* t_CustomFreePool)(__int64, __int64, __int64);
typedef (__fastcall* MemoryAllocatePages_t)(IN SIZE_T NumberOfBytes, IN ULONG Node);

// Definice struktury Command
typedef struct {
    uintptr_t selfref;
    uintptr_t pid;            // Cílový proces
    uintptr_t destination;
    int cmdId;                // Akce k provedení
    char rw;                  // 0 = čtení, 1 = zápis
    unsigned char* pSource;
    SIZE_T size;
} Command;

// Konstanty pro různé příkazy
#define COMMAND_READWRITE  0xB16B00B5
#define COMMAND_GETPROCPID 0xBADA55
#define COMMAND_ISLOADED   0x69420

// Deklarace externích funkcí
extern NTSTATUS AdjustRegister();
extern t_CustomFreePool CustomFreePool;
extern MemoryAllocatePages_t MemoryAllocatePages;

// Prototypy funkcí
NTSTATUS InitializeDriver(DWORD32 pid);
uintptr_t FindPIDByName(char* processName);
