#include "xadl_utils.h"
#include "utils.h"
#include "updater_service.h"
#include <Shlwapi.h>
#include <Shlobj.h>
#include <iostream>
#include <Windows.h>
#include <WtsApi32.h>

using namespace std;

wstring GetAppInstallPath() {
    static TCHAR szInstallPath[MAX_PATH] = { 0 };

    int i = _tcsnlen(szInstallPath, MAX_PATH);
    if (i == 0) {
        ReadFromRegistry(_T("SOFTWARE\\XADL\\Audit"), _T("InstallPath"), szInstallPath, MAX_PATH);
    }
    return szInstallPath;
}


wstring GetServiceExeDir() {
    static TCHAR szPath[MAX_PATH] = { 0 };

    int i = _tcsnlen(szPath, MAX_PATH);
    if (i == 0) {
        SHGetFolderPath(NULL,
            CSIDL_COMMON_APPDATA | CSIDL_FLAG_CREATE,
            NULL,
            0,
            szPath);
        PathAppend(szPath, _T(PRODUCT_NAME));
    }
    return szPath;
}

void UninstallClient() {
    wstring installedPath = GetAppInstallPath();
    wstring cmd = installedPath;
    cmd += _T("\\uninst.exe");

    KillUpdater();
    if (!PathFileExists(cmd.c_str())) {
        _tprintf(_T("%s not exists"), cmd.c_str());
        return;
    }
    else {
        cmd += _T(" /S");
        _tprintf(_T("run %s"), cmd.c_str());

        LPTSTR szCmd = _tcsdup(cmd.c_str());
        RunNewProcess(szCmd);
        free(szCmd);
    }
}

void KillUpdater() {
    DWORD pid = 0;
    if (FindProcessPid(_T("AuditUpdate.exe"), pid)) {
        HANDLE handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (NULL != handle)
        {
            TerminateProcess(handle, 0);
            CloseHandle(handle);
        }
    }
}
