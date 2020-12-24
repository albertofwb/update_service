#include "updater_service.h"
#include <Shlobj.h>
#include <Windows.h>
#include <WtsApi32.h>
#include <string>
#include <fstream>
using tofstream = std::wofstream;

// 以当前用户身份启动一个进程
#pragma comment(lib, "Wtsapi32.lib")

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
        0,
        NULL,
        lpWorkDir,
        &si,
        &pi))
    {
        _tprintf(_T("CreateProcess failed %s (%d).\n"), lpCommandLine, GetLastError());
        return;
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (exitCode != NULL) {
        GetExitCodeProcess(pi.hProcess, exitCode);
    }
    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

bool StartInteractiveProcess(LPTSTR lpCommandLine, LPCTSTR lpWorkDir)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.lpDesktop = TEXT("winsta0\\default");  // Use the default desktop for GUIs
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));
    HANDLE token;
    DWORD sessionId = ::WTSGetActiveConsoleSessionId();
    if (0xffffffff == sessionId)  // Noone is logged-in
    {
        return false;
    }
    // This only works if the current user is the system-account (we are probably a Windows-Service)
    HANDLE dummy;
    if (::WTSQueryUserToken(sessionId, &dummy)) {
        if (!::DuplicateTokenEx(dummy, TOKEN_ALL_ACCESS, NULL, SecurityDelegation, TokenPrimary, &token)) {
            ::CloseHandle(dummy);
            return false;
        }
        ::CloseHandle(dummy);
        // Create process for user with desktop
        if (!::CreateProcessAsUser(token, NULL, lpCommandLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, lpWorkDir, &si, &pi)) {  // The "new console" is necessary. Otherwise the process can hang our main process
            ::CloseHandle(token);
            return false;
        }
        ::CloseHandle(token);
    }

    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
    return true;
}

CString GetFormattedTime() {
    SYSTEMTIME sysTime = { 0 };
    ::GetLocalTime(&sysTime);

    static CString message;
    message.Format(_T("%04d-%2d-%2d %2d:%2d:%2d "),
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
    TCHAR szInstallPath[MAX_PATH] = { 0 };

    ReadFromRegistry(_T("SOFTWARE\\XADL\\Audit"), _T("InstallPath"), szInstallPath, MAX_PATH);
    return szInstallPath;
}



void RunUpdater(std::wstring& installPath, tofstream& m_logFile) {
    std::wstring cmd = installPath;
    cmd += _T("\\updater.exe");
    if (!PathFileExists(cmd.c_str())) {
        m_logFile << GetFormattedTime().GetString() << cmd << " not exists" << std::endl;
        return;
    }

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

void RunApp(std::wstring& installPath, tofstream& m_logFile) {
    std::wstring cmd = installPath;
    cmd += _T("\\Audit.exe");
    if (!PathFileExists(cmd.c_str())) {
        m_logFile << GetFormattedTime().GetString() << cmd << " not exists" << std::endl;
        return;
    }
    cmd += _T(" --auto-start");

    LPTSTR szAppCmdline = _tcsdup(cmd.c_str());
    m_logFile << GetFormattedTime().GetString() << "cmdline " << szAppCmdline << std::endl;
    StartInteractiveProcess(szAppCmdline, installPath.c_str());
    free(szAppCmdline);
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
}