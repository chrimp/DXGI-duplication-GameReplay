#include "Procmon.hpp"

BOOL ProcmonEvent::DoEvent(const CRefPtr<CEventView> pEventView) {
    ULONGLONG time = pEventView->GetStartTime().QuadPart;
    DWORD pid = pEventView->GetProcessId();

    if (pid == m_PID) {
        switch(pEventView->GetEventClass()) {
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

bool ProcmonEvent::StartMonitor() {
    m_PID = getPID();
    if (m_PID == -1) {
        return false;
    }

    CEventMgr& OptMgr = Singleton<CEventMgr>::getInstance();
    CMonitorController& MonitorMgr = Singleton<CMonitorController>::getInstance();
    CDrvLoader& DrvLoader = Singleton<CDrvLoader>::getInstance();

    if (!DrvLoader.Init(TEXT("CROCMON24"), TEXT("CROCMON24.sys"))) return false;
    OptMgr.RegisterCallback(new ProcmonEvent());

    if (!MonitorMgr.Connect()) return false;

    MonitorMgr.SetMonitor(TRUE, TRUE, FALSE);
    if (!MonitorMgr.Start()) return false;

    return true;
}

void ProcmonEvent::StopMonitor() {
    CMonitorController& MonitorMgr = Singleton<CMonitorController>::getInstance();
    MonitorMgr.Stop();
    MonitorMgr.Destory();
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