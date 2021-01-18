#include "utils.h"
#include <stdio.h>

using namespace std;

void RunNewProcess(LPTSTR lpCommandLine, LPCTSTR lpWorkDir) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(NULL,
        lpCommandLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_PROCESS_GROUP,
        NULL,
        lpWorkDir,
        &si,
        &pi))
    {
        _tprintf(_T("CreateProcess failed %s (%d).\n"), lpCommandLine, GetLastError());
        return;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}


BOOL ReadFromRegistry(LPTSTR lpSubKey, LPTSTR pszKeyname, LPTSTR szBuff, DWORD dwSize)
{
    HKEY hKey = NULL;
    LONG lResult = 0;
    BOOL fSuccess = TRUE;
    DWORD dwRegType = REG_SZ;

    // https://stackoverflow.com/questions/252297/why-is-regopenkeyex-returning-error-code-2-on-vista-64bit
    lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
    if (lResult != 0) {
        lResult = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ | KEY_WOW64_32KEY, &hKey);
    }
    fSuccess = (lResult == 0);

    if (fSuccess)
    {
        lResult = RegQueryValueEx(hKey, pszKeyname, NULL, &dwRegType, (LPBYTE)szBuff, &dwSize);
        fSuccess = (lResult == 0);
    }

    if (fSuccess)
    {
        fSuccess = (_tcslen(szBuff) > 0);
    }

    if (hKey != NULL)
    {
        RegCloseKey(hKey);
        hKey = NULL;
    }

    return fSuccess;
}


BOOL FindProcessPid(LPCTSTR ProcessName, DWORD& dwPid)
{
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;

    // Take a snapshot of all processes in the system.
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE)
    {
        return(FALSE);
    }

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32))
    {
        CloseHandle(hProcessSnap);          // clean the snapshot object
        return(FALSE);
    }

    BOOL    bRet = FALSE;
    do
    {
        if (!_tcsicmp(ProcessName, pe32.szExeFile))
        {
            dwPid = pe32.th32ProcessID;
            bRet = TRUE;
            break;
        }

    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return bRet;
}