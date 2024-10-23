#include "driver.h"

// Implementace funkcí
t_CustomFreePool CustomFreePool;
MemoryAllocatePages_t MemoryAllocatePages;

__int64 __fastcall handler(__int64 arg1, __int64 arg2, __int64 arg3) {
    Command* cmd = (Command*)arg1;
    
    if (cmd->cmdId == COMMAND_READWRITE) {
        AdjustRegister();  // Oprava registru, aby se předešlo problémům s barvami systému

#ifdef _DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "R/W Akce: %d | PID: %d | Zdrojová adresa: 0x%X | Cílová adresa: 0x%X | Velikost: 0x%X\r\n",
            cmd->rw, cmd->pid, cmd->pSource, cmd->destination, cmd->size);

        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Návratová adresa: 0x%X\r\n", _ReturnAddress());
#endif

        NTSTATUS status = 0;
        SIZE_T opResult = 0;

        if (cmd->rw == 0) {  // Čtení z paměti
            status = ReadProcessMemory(cmd->pid, cmd->pSource, cmd->destination, cmd->size, &opResult);
        } else if (cmd->rw == 1) {  // Zápis do paměti
            status = WriteProcessMemory(cmd->pid, cmd->destination, cmd->pSource, cmd->size, &opResult);
        }

        Command* ref_cmd = cmd->selfref;
        ref_cmd->pid = status;
        cmd->cmdId = 0;  // Resetování příkazu
    } 
    else if (cmd->cmdId == COMMAND_GETPROCPID) {
        AdjustRegister();  // Oprava registru

        PEPROCESS Process;
        PsLookupProcessByProcessId(cmd->pid, &Process);

        Command* ref_cmd = cmd->selfref;
        ref_cmd->pid = PsGetProcessPeb(Process);  // Získání PEB procesu

#ifdef _DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Adresa PEB procesu: 0x%p\r\n", ref_cmd->pid);
#endif

        cmd->cmdId = 0;  // Resetování příkazu
    }

    return CustomFreePool(arg1, arg2, arg3);
}

uintptr_t FindPIDByName(char* processName) {
    NTSTATUS status;
    PRTL_PROCESS_MODULES modInfo;

    modInfo = ExAllocatePool(PagedPool, 1024 * 1024);  // Alokace pro seznam modulů

    if (!modInfo) {
#ifdef DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Chyba alokace modulů\r\n");
#endif
        return -1;
    }

    if (!NT_SUCCESS(status = ZwQuerySystemInformation(11, modInfo, 1024 * 1024, NULL))) {
#ifdef _DEBUG
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Chyba při dotazu na systémové informace\r\n");
#endif
        ExFreePool(modInfo);
        return -1;
    }

    for (int i = 0; i < modInfo->NumberOfModules; i++) {
        if (!strcmp(modInfo->Modules[i].FullPathName + modInfo->Modules[i].OffsetToFileName, processName)) {
#ifdef _DEBUG
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Nalezen proces: %s\r\n", processName);
#endif
            return modInfo->Modules[i].ImageBase;
        }
    }

    ExFreePool(modInfo);
    return 0;
}

NTSTATUS InitializeDriver(DWORD32 targetPid) {
#ifdef _DEBUG
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, -1, "Inicializace ovladače s PID %d\r\n", targetPid);
#endif

    PEPROCESS targetProcess;
    PsLookupProcessByProcessId(targetPid, &targetProcess);

    KAPC_STATE apcState;
    KeStackAttachProcess(targetProcess, &apcState);

    uintptr_t base_win32k = FindPIDByName("win32kbase.sys");

    uintptr_t free_pool_ptr = base_win32k + 0x2B3C90;  // Offset od win32kbase.sys
    CustomFreePool = *(t_CustomFreePool*)free_pool_ptr;  // Zachycení původní funkce
    *(t_CustomFreePool*)free_pool_ptr = &handler;  // Hookování funkce

    KeUnstackDetachProcess(&apcState);

    return STATUS_SUCCESS;
}
