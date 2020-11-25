#pragma once
#include <fstream>
#include "service_base.h"

class UpdaterService: public ServiceBase
{
public:
    UpdaterService(const UpdaterService& other) = delete;
    UpdaterService& operator=(const UpdaterService& other) = delete;

    UpdaterService(UpdaterService&& other) = delete;
    UpdaterService& operator=(UpdaterService&& other) = delete;

    UpdaterService()
        : ServiceBase(_T("UpdaterService"),
            _T("Audit Client Update Service"),
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            SERVICE_ACCEPT_STOP) {}
private:
    void OnStart(DWORD argc, TCHAR* argv[]) override;
    void OnStop() override;

#ifdef UNICODE
    using tofstream = std::wofstream;
#else
    using tofstream = std::ofstream;
#endif

    tofstream m_logFile;
};

