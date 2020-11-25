#include "updater_service.h"
#include <Shlobj.h>

CString GetFormattedTime() {
    SYSTEMTIME sysTime = { 0 };
    ::GetLocalTime(&sysTime);

    CString message;
    message.Format(_T("%04d-%2d-%2d %2d:%2d:%2d "),
        sysTime.wYear, sysTime.wMonth, sysTime.wDay,
        sysTime.wHour, sysTime.wMinute, sysTime.wSecond);

    return message;
}
void UpdaterService::OnStart(DWORD /*argc*/, TCHAR** /*argv[]*/) {
    m_logFile.close();
    TCHAR szPath[MAX_PATH] = { 0 };
    SHGetFolderPath(NULL, 
                    CSIDL_COMMON_APPDATA | CSIDL_FLAG_CREATE,
                    NULL,
                    0,
                    szPath);
    PathAppend(szPath, _T("Audit\\logs"));
    SHCreateDirectoryEx(NULL, szPath, NULL);
    CString logPath;
    logPath.Format(_T("%s\\xadl_service.log"), szPath);
    m_logFile.open(logPath, std::ios_base::app);

    if (!m_logFile.is_open()) {
        CString msg = _T("Can't open log file ");
        msg += logPath;
        WriteToEventLog(msg, EVENTLOG_ERROR_TYPE);
    }
    else {
        m_logFile << GetFormattedTime().GetString() << "started" << std::endl;
    }
}

void UpdaterService::OnStop() {
    // Doesn't matter if it's open.
    if (m_logFile.is_open()) {
        m_logFile << GetFormattedTime().GetString() << "stopped" << std::endl;
    }
    m_logFile.close();
}