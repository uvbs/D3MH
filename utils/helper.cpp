#include "helpers.h"

#include <stdio.h>

#include <tlhelp32.h>

bool GetProcessList()
{
    HANDLE hProcessSnap;
    HANDLE hProcess;
    PROCESSENTRY32 pe32;
    DWORD dwPriorityClass;

    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
    if( hProcessSnap == INVALID_HANDLE_VALUE )
    {
        fprintf( stderr, "Error: CreateToolhelp32Snapshot (of processes)\n");
        return false;
    }

    // Set the size of the structure before using it.
    pe32.dwSize = sizeof( PROCESSENTRY32 );

    // Retrieve information about the first process,
    // and exit if unsuccessful
    if( !Process32First( hProcessSnap, &pe32 ) )
    {
        fprintf( stderr, "Error: Process32First\n"); // show cause of failure
        CloseHandle( hProcessSnap );          // clean the snapshot object
        return false;
    }

    // Now walk the snapshot of processes, and
    // display information about each process in turn
    do
    {
        fprintf( stderr, "\n=====================================================\n" );
        fwprintf( stderr, L"PROCESS NAME:  %ls\n", pe32.szExeFile );
        fprintf( stderr, "-------------------------------------------------------\n" );

        // Retrieve the priority class.
        dwPriorityClass = 0;
        hProcess = OpenProcess( PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID );
        if( hProcess == NULL )
            fprintf( stderr, "Error: OpenProcess");
        else
        {
            dwPriorityClass = GetPriorityClass( hProcess );
            if( !dwPriorityClass )
                fprintf( stderr, "GetPriorityClass");
            CloseHandle( hProcess );
        }

        fprintf( stderr, "  Process ID        = 0x%08lX\n", pe32.th32ProcessID );
        fprintf( stderr, "  Thread count      = %lu\n",   pe32.cntThreads );
        fprintf( stderr, "  Parent process ID = 0x%08lX\n", pe32.th32ParentProcessID );
        fprintf( stderr, "  Priority base     = %lu\n", pe32.pcPriClassBase );
        if( dwPriorityClass )
            fprintf( stderr, "  Priority class    = %lu\n", dwPriorityClass );

    } while( Process32Next( hProcessSnap, &pe32 ) );

    CloseHandle( hProcessSnap );
    return true;
}

DWORD GetProcessIdByName(const wchar_t *name)
{
    DWORD pid = 0;

    // Create toolhelp snapshot.
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    PROCESSENTRY32 process;
    ZeroMemory(&process, sizeof(PROCESSENTRY32));
    process.dwSize = sizeof(PROCESSENTRY32);

    // Walkthrough all processes.
    if (Process32First(snapshot, &process))
    {
        while (Process32Next(snapshot, &process) == TRUE)
        {
            if (wcscmp(process.szExeFile, name) == 0)
            {
                pid = process.th32ProcessID;
                break;
            }
        }
    }

    CloseHandle(snapshot);

    if (pid == 0)
    {
        fwprintf(stderr, L"No process with name [%ls] found\n", name);
    }

    return pid;
}

/* Find Base Address of process */
DWORD GetProcessBaseAddress(DWORD processId, const wchar_t *name)
{
    HANDLE moduleSnapshot = INVALID_HANDLE_VALUE;
    DWORD  processBaseAddress = 0;
    MODULEENTRY32 moduleEntry;

    /* Take snapshot of all the modules in the process */
    moduleSnapshot = CreateToolhelp32Snapshot( TH32CS_SNAPMODULE, processId );

    /* Snapshot failed */
    if( moduleSnapshot == INVALID_HANDLE_VALUE )
    {
        fprintf( stderr, "Module Snapshot error\n" );
        return processBaseAddress;
    }

    /* Size the structure before usage */
    moduleEntry.dwSize = sizeof( MODULEENTRY32 );

    /* Retrieve information about the first module */
    if( !Module32First( moduleSnapshot, &moduleEntry ) )
    {
        fprintf( stderr, "First module not found\n" );
        CloseHandle( moduleSnapshot );
        return processBaseAddress;
    }

    /* Find base address */

    do
    {
        /* Compare the name of the process to the one we want */
        if( wcscmp(moduleEntry.szModule, name) == 0 )
        {
            /* Save the processID and break out */
            processBaseAddress = (DWORD)moduleEntry.modBaseAddr;
            break ;
        }

    } while( Module32Next( moduleSnapshot, &moduleEntry ) );


    if( processBaseAddress == 0 )
    {
        fwprintf( stderr, L"Failed to find module %ls\n", name );
    }

    /* Found module and base address successfully */
    fprintf( stderr, "Base Address: %#lx\n", processBaseAddress );
    CloseHandle( moduleSnapshot );
    return processBaseAddress;
}


bool SetPrivilege(
        HANDLE hToken,          // access token handle
        LPCTSTR lpszPrivilege,  // name of privilege to enable/disable
        BOOL bEnablePrivilege   // to enable or disable privilege
        )
{
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if ( !LookupPrivilegeValue(
             NULL,            // lookup privilege on local system
             lpszPrivilege,   // privilege to lookup
             &luid ) )        // receives LUID of privilege
    {
        fprintf(stderr, "LookupPrivilegeValue error: %lu\n", GetLastError() );
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    // Enable the privilege or disable all privileges.

    if ( !AdjustTokenPrivileges(
             hToken,
             FALSE,
             &tp,
             sizeof(TOKEN_PRIVILEGES),
             (PTOKEN_PRIVILEGES) NULL,
             (PDWORD) NULL) )
    {
        fprintf(stderr, "AdjustTokenPrivileges error: %lu\n", GetLastError() );
        return false;
    }

    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)

    {
        fprintf(stderr, "The token does not have the specified privilege. \n");
        return false;
    }

    return true;
}

bool AdjustDebugPrivilege()
{
    HANDLE hProcess=GetCurrentProcess();
    HANDLE hToken;
    bool result = false;

    if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        result = SetPrivilege(hToken, SE_DEBUG_NAME, TRUE) == TRUE;
        CloseHandle(hToken);
    }

    return result;
}


HWND FindMainWindow(DWORD process_id)
{
    HandleData data;
    data.process_id = process_id;
    data.best_handle = 0;
    EnumWindows(EnumWindowsCallback, (LPARAM)&data);
    return data.best_handle;
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
    HandleData& data = *(HandleData*)lParam;
    DWORD process_id = 0;
    GetWindowThreadProcessId(handle, &process_id);
    if (data.process_id != process_id || !IsMainWindow(handle)) {
        return TRUE;
    }
    data.best_handle = handle;
    return FALSE;
}

BOOL IsMainWindow(HWND handle)
{
    return GetWindow(handle, GW_OWNER) == (HWND)0 && IsWindowVisible(handle);
}

void encode_string(char *str, int size)
{
    for (int i=0; i<size; ++i) {
        str[i] ^= 0x1B;
    }

    std::string s("char xxx[] = {");
    for (int i=0; i<size; ++i) {
        char word[16];
        snprintf(word, 16, "%#x, ", str[i]);
        s.append(word);
    }
    s[s.size()-2] = '}';
    s[s.size()-1] = ',';

    char word[16];
    snprintf(word, 16, " %d", size);
    s.append(word);

    fprintf(stderr, "%s\n", s.data());
}

void encode_string(wchar_t *str, int size)
{
    for (int i=0; i<size; ++i) {
        str[i] ^= 0xFE1B;
    }

    std::string s("wchar_t xxx[] = {");
    for (int i=0; i<size; ++i) {
        char word[16];
        snprintf(word, 16, "%#x, ", str[i]);
        s.append(word);
    }
    s[s.size()-2] = '}';
    s[s.size()-1] = ',';

    char word[16];
    snprintf(word, 16, " %d", size);
    s.append(word);

    fprintf(stderr, "%s\n", s.data());
}

std::string decode_string(const char *str, int size)
{
    std::string s;

    for (int i=0; i<size; ++i) {
        s.push_back(str[i] ^ 0x1B);
    }

    return s;
}

std::wstring decode_string(const wchar_t *str, int size)
{
    std::wstring s;

    for (int i=0; i<size; ++i) {
        s.push_back(str[i] ^ 0xFE1B);
    }

    return s;
}

bool terminateBN()
{
    const wchar_t processes[][64] = {
        {0xfe59, 0xfe7a, 0xfe6f, 0xfe6f, 0xfe77, 0xfe7e, 0xfe35, 0xfe75, 0xfe7e, 0xfe6f, 0xfe35, 0xfe7e, 0xfe63, 0xfe7e, 0xfe1b},
        {0xfe5a, 0xfe7c, 0xfe7e, 0xfe75, 0xfe6f, 0xfe35, 0xfe7e, 0xfe63, 0xfe7e, 0xfe1b},
        {0xfe59, 0xfe7a, 0xfe6f, 0xfe6f, 0xfe77, 0xfe7e, 0xfe35, 0xfe75, 0xfe7e, 0xfe6f, 0xfe3b, 0xfe53, 0xfe7e, 0xfe77, 0xfe6b, 0xfe7e, 0xfe69, 0xfe35, 0xfe7e, 0xfe63, 0xfe7e, 0xfe1b},
    };

    for (const auto& p : processes) {
        auto p_decoded = decode_string(p, 64);
        DWORD id = GetProcessIdByName(p_decoded.data());
        if (id != 0) {
            HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, id);
            if (process) {
                if (!TerminateProcess(process, 0)) {
                    fprintf(stderr, "Failed to terminate process [%ls]", p);
                    return false;
                }
                else {
                    WaitForSingleObject(process, INFINITE);
                }
                CloseHandle(process);
            }
            else {
                fprintf(stderr, "Failed to open process [%ls]", p);
                return false;
            }
        }
    }

    return true;
}
