#include "DirChanges.hpp"

std::atomic<bool> run = false;

std::filesystem::path FindProcessPath() {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    LPCTSTR fullPath = NULL;
    DWORD targetPID = -1;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (Process32First(snapshot, &entry)) {
        while (Process32Next(snapshot, &entry)) {
            if (_wcsicmp(entry.szExeFile, L"WJMAX.exe") == 0) {
                MODULEENTRY32 module;
                module.dwSize = sizeof(MODULEENTRY32);
                HANDLE moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, entry.th32ProcessID);
                targetPID = entry.th32ProcessID;
                if (Module32First(moduleSnapshot, &module)) {
                    fullPath = module.szExePath;
                    break;
                }
            }
        }
    }
    CloseHandle(snapshot);
    if (fullPath == NULL) return std::filesystem::path();

    std::filesystem::path path = std::filesystem::path(fullPath).parent_path();
    path = path.wstring() + L"\\WJMAX_Data\\Music\\";

    return path;
}

void ListenFileEventLoop() {
    std::filesystem::path path = FindProcessPath();
    if (path.empty()) {
        LogMessage(3, "Failed to find WJMAX.exe");
        return;
    }

    HANDLE hDir = CreateFile(
        path.wstring().c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL
    );

    BYTE buffer[4096];
    DWORD bytesReturned;

    while (run) {
        if (ReadDirectoryChangesW(
            hDir, buffer, sizeof(buffer), TRUE, FILE_NOTIFY_CHANGE_LAST_ACCESS,
            &bytesReturned, NULL, NULL
        )) {
            FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)buffer;
            ProcessFileEvent(fni, path);    
        } else {
            std::cout << "Error: " << GetLastError() << std::endl;
        }
    }
}

void ProcessFileEvent(FILE_NOTIFY_INFORMATION *pfni, std::filesystem::path parentPath) {
    while (pfni->NextEntryOffset != 0) {
        std::wstring filename(pfni->FileName, pfni->FileNameLength / sizeof(WCHAR));
        if (filename.find(L".ogg") != std::wstring::npos) {
            std::wstring fullPath = parentPath.wstring() + filename;
            // Update CaptureThread's gamestate
            CaptureThreadManager::GetInstance().UpdateGameStatus(1); // 0 = MENU, 1 = PLAYING, 2 = PAUSED
        } else if (filename.find(L".mp4") != std::wstring::npos) {
            std::wstring fullPath = parentPath.wstring() + filename;
            // Update CaptureThread's gamestate
            CaptureThreadManager::GetInstance().UpdateGameStatus(0); // 0 = MENU, 1 = PLAYING, 2 = PAUSED
        }
        pfni = (FILE_NOTIFY_INFORMATION*)((BYTE*)pfni + pfni->NextEntryOffset);
    }
}