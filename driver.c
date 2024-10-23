#include "driver.h"
#include "physmem.h"

t_FreePool FreePoolFunction;
AllocatePages_t AllocatePagesFunction;

__int64 __fastcall hookFunction(__int64 param1, __int64 param2, __int64 param3)
{
    Command* cmd = (Command*)param1;

    if (cmd->commandId == COMMAND_READWRITE)
    {
        AdjustRegisters(); // Adjust registers for system color settings

#ifdef _DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Operation: %d | PID: %d | Source: 0x%X | Destination: 0x%X | Size: 0x%X\r\n",
            cmd->operation, cmd->targetPid, cmd->sourceAddr, cmd->destination, cmd->size);

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Return address: 0x%X\r\n", _ReturnAddress());
#endif

        NTSTATUS status = 0;
        SIZE_T bytesTransferred;

        if (cmd->operation == 0) // Read operation
        {
            status = ReadProcessMemory(cmd->targetPid, cmd->sourceAddr, cmd->destination, cmd->size, &bytesTransferred);
        }
        else if (cmd->operation == 1) // Write operation
        {
            status = WriteProcessMemory(cmd->targetPid, cmd->destination, cmd->sourceAddr, cmd->size, &bytesTransferred);
        }

        Command* responseCmd = cmd->reference;
        responseCmd->targetPid = status;

        cmd->commandId = 0; // Reset command ID to avoid repeated calls
    }
    else if (cmd->commandId == COMMAND_GETPROCPID)
    {
        AdjustRegisters(); // Adjust registers for system color settings

        PEPROCESS proc;
        PsLookupProcessByProcessId(cmd->targetPid, &proc);

        Command* responseCmd = cmd->reference;
        responseCmd->targetPid = PsGetProcessPeb(proc);
#ifdef _DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "PEB Address: 0x%p\r\n", responseCmd->targetPid);
#endif

        cmd->commandId = 0; // Reset command ID to avoid repeated calls
    }

    return FreePoolFunction(param1, param2, param3);
}

uintptr_t FindProcessID(char* imageName)
{
    NTSTATUS status;
    PRTL_PROCESS_MODULES moduleInfo;

    moduleInfo = ExAllocatePool(PagedPool, 1024 * 1024); // Allocate memory for module list

    if (!moduleInfo)
    {
#ifdef DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Memory allocation failed\r\n");
#endif
        return -1;
    }

    if (!NT_SUCCESS(status = ZwQuerySystemInformation(11, moduleInfo, 1024 * 1024, NULL))) // 11 = SystemModuleInformation
    {
#ifdef _DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Failed to query system information\r\n");
#endif
        ExFreePool(moduleInfo);
        return -1;
    }

    for (int i = 0; i < moduleInfo->NumberOfModules; i++)
    {
        if (!strcmp(moduleInfo->Modules[i].FullPathName + moduleInfo->Modules[i].OffsetToFileName, imageName))
        {
#ifdef _DEBUG
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Found %s\r\n", imageName);
#endif
            return moduleInfo->Modules[i].ImageBase;
        }
    }

    ExFreePool(moduleInfo);
    return 0;
}

NTSTATUS EntryPoint(DWORD32 pid)
{
#ifdef _DEBUG
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Entry point of driver %d\r\n", pid);
#endif

    PEPROCESS process;
    PsLookupProcessByProcessId(pid, &process);

    KAPC_STATE state;
    KeStackAttachProcess(process, &state);

    uintptr_t win32k_base = FindProcessID("win32kbase.sys");
    uintptr_t poolFreeAddr = win32k_base + 0x2B3C90; // Adjust this address based on the target function

    FreePoolFunction = *(t_FreePool*)poolFreeAddr; // Store original FreePool function
    *(t_FreePool*)poolFreeAddr = &hookFunction; // Hook the function

    KeUnstackDetachProcess(&state);
    return STATUS_SUCCESS;
}
