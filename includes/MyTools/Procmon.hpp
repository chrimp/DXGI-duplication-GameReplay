#ifndef PROCMON_HPP
#define PROCMON_HPP

#include <windows.h>
#include <TlHelp32.h>

#include "../includes/procmon/sdk/procmonsdk/sdk.hpp"
#include "../includes/MyTools/ThreadManager.hpp"
#include "../includes/MyTools/LogMessage.hpp"

#pragma comment(lib, "procmonsdk.lib")

class ProcmonEvent: public IEventCallback {
    public:
    ProcmonEvent();
    ~ProcmonEvent() {}
    virtual BOOL DoEvent(const CRefPtr<CEventView> pEventView);

    private:
    LONG m_PID = -1;
    bool m_isPlaying = false;
    std::chrono::system_clock::time_point m_lastPlayTime;

    DWORD getPID();
};

bool StartProcmon();
void StopProcmon();

#endif // PROCMON_HPP