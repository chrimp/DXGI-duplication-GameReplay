#include "Procmon.hpp"

ProcmonEvent::ProcmonEvent() {
    m_PID = getPID();
}

DWORD ProcmonEvent::getPID() {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);

    DWORD pid = -1;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"WJMAX.exe") == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32Next(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

BOOL ProcmonEvent::DoEvent(const CRefPtr<CEventView> pEventView) {
    ULONGLONG time = pEventView->GetStartTime().QuadPart;
    DWORD pid = pEventView->GetProcessId();

    if (pid == m_PID) {
        switch(pEventView->GetEventClass()) {
            case MONITOR_TYPE_FILE:
                PLOG_ENTRY plog = pEventView->GetPreEventEntry();
                std::wstring process(pEventView->GetProcessName().GetBuffer());
                std::wstring operation(StrMapOperation(plog));
                std::wstring path(pEventView->GetPath().GetBuffer());

                if (path.find(L".ogg") != std::wstring::npos) {
                    m_lastPlayTime = std::chrono::system_clock::now();
                    CaptureThreadManager::GetInstance().UpdateGameState(1);
                } else if (path.find(L"review.mp4") != std::wstring::npos) {
                    std::chrono::seconds gap{ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - m_lastPlayTime) };
                    if (m_isPlaying && gap.count() > 1) {
                        m_isPlaying = false;
                        CaptureThreadManager::GetInstance().UpdateGameState(0);
                    }
                }
                break;
        }
    }

    return TRUE;
}

bool StartProcmon() {
    CEventMgr& OptMgr = Singleton<CEventMgr>::getInstance();
    CMonitorController& MonitorMgr = Singleton<CMonitorController>::getInstance();
    CDrvLoader& DrvLoader = Singleton<CDrvLoader>::getInstance();

    if (!DrvLoader.Init(TEXT("CROCMON24"), TEXT("CROCMON24.sys"))) {
		LogMessage(3, "Failed to load procmon driver");
        LogMessage(3, "Is .sys file in the same directory as the executable?");
		return false;
    }
    OptMgr.RegisterCallback(new ProcmonEvent);

    if (!MonitorMgr.Connect()) {
		LogMessage(3, "Failed to connect to procmon driver");
        LogMessage(3, "Have you launched the program as administrator?");
		return false;
    }

    MonitorMgr.SetMonitor(TRUE, TRUE, FALSE);
    if (!MonitorMgr.Start()) {
		LogMessage(3, "Failed to start procmon monitor");
		return false;
    }

    return true;
}

void StopProcmon() {
    CMonitorController& MonitorMgr = Singleton<CMonitorController>::getInstance();
    MonitorMgr.Stop();
    MonitorMgr.Destory();
}