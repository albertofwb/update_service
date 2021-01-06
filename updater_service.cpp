#include "updater_service.h"
#include <Shlobj.h>
#include <Windows.h>
#include <WtsApi32.h>
#include <string>
#include <fstream>
using tofstream = std::wofstream;


HANDLE gUpdaterProcess = NULL;
const TCHAR* kUpdateName = _T("AuditUpdate.exe");

void RunNewProcess(LPTSTR lpCommandLine, LPCTSTR lpWorkDir, DWORD* exitCode=NULL) {
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

    if (exitCode != NULL) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, exitCode);
    }
    // Close process and thread handles. 
    // CloseHandle(pi.hProcess);
    gUpdaterProcess = pi.hProcess;
    CloseHandle(pi.hThread);
}


CString GetFormattedTime() {
    SYSTEMTIME sysTime = { 0 };
    ::GetLocalTime(&sysTime);

    static CString message;
    message.Format(_T("%04d-%02d-%02d %2d:%2d:%2d "),
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

    return message;
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


std::wstring GetAppInstallPath() {
    static TCHAR szInstallPath[MAX_PATH] = { 0 };

    int i = _tcsnlen(szInstallPath, MAX_PATH);
    if (i == 0) {
        ReadFromRegistry(_T("SOFTWARE\\XADL\\Audit"), _T("InstallPath"), szInstallPath, MAX_PATH);
    }
    return szInstallPath;
}



void RunUpdater(std::wstring& installPath, tofstream& m_logFile) {
    std::wstring cmd = installPath;
    cmd += _T("\\");
    cmd += kUpdateName;
    if (!PathFileExists(cmd.c_str())) {
        m_logFile << GetFormattedTime().GetString() << cmd << " not exists" << std::endl;
        return;
    }

    // check update every 6 hours
    cmd += _T(" --interval_check 6");

    cmd += _T(" --log_path ");
    cmd += installPath;
    cmd += _T("\\update_log.log");

    cmd += _T(" --install_root ");
    cmd += installPath;

    LPTSTR szUpdaterCmdline = _tcsdup(cmd.c_str());
    m_logFile << GetFormattedTime().GetString() << "cmdline " << szUpdaterCmdline << std::endl;

    RunNewProcess(szUpdaterCmdline, installPath.c_str(), NULL);
    free(szUpdaterCmdline);
}


DWORD WINAPI CheckAppUpdateEverySixHours(tofstream& m_logFile) {
    std::wstring installPath = GetAppInstallPath();
    if (!PathFileExists(installPath.c_str())) {
        m_logFile << GetFormattedTime().GetString() << "install path not exists" << std::endl;
        return false;
    }

    RunUpdater(installPath, m_logFile);

    return true;
}

void UpdaterService::OnStart(DWORD /*argc*/, TCHAR** /*argv[]*/) {
    m_logFile.close();
    TCHAR szPath[MAX_PATH] = { 0 };
    SHGetFolderPath(NULL, 
                    CSIDL_COMMON_APPDATA | CSIDL_FLAG_CREATE,
                    NULL,
                    0,
                    szPath);
    PathAppend(szPath, _T(PRODUCT_NAME "\\logs"));
    SHCreateDirectoryEx(NULL, szPath, NULL);
    CString logPath;
    logPath.Format(_T("%s\\update_service.log"), szPath);
    m_logFile.open(logPath, std::ios_base::app);

    if (!m_logFile.is_open()) {
        CString msg = _T("Can't open log file ");
        msg += logPath;
        WriteToEventLog(msg, EVENTLOG_ERROR_TYPE);
    }
    else {
        m_logFile << GetFormattedTime().GetString() << "started" << std::endl;
    }
    CheckAppUpdateEverySixHours(m_logFile);
}

void UpdaterService::OnStop() {
    // Doesn't matter if it's open.
    if (m_logFile.is_open()) {
        m_logFile << GetFormattedTime().GetString() << "stopped" << std::endl;
    }
    m_logFile.close();
    if (gUpdaterProcess != NULL) {
        TerminateProcess(gUpdaterProcess, 0);
        m_logFile << GetFormattedTime().GetString() << " terminate " << std::endl;
    }
}