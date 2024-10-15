#ifndef PROCMON_HPP
#define PROCMON_HPP

#include <windows.h>
#include <TlHelp32.h>

#include "../includes/procmon/sdk/procmonsdk/sdk.hpp"
#include "../includes/MyTools/ThreadManager.hpp"

class ProcmonEvent: public IEventCallback {
    public:
    virtual BOOL DoEvent(const CRefPtr<CEventView> pEventView);
    bool StartMonitor();
    void StopMonitor();

    private:
    LONG m_PID = -1;
    bool m_isPlaying = false;
    std::chrono::system_clock::time_point m_lastPlayTime;

    DWORD getPID();
};

#endif // PROCMON_HPP