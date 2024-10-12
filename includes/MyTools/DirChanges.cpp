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

HANDLE hDir;

void ListenFileEventLoop() {
    std::filesystem::path path = FindProcessPath();
    if (path.empty()) {
        LogMessage(3, "Failed to find WJMAX.exe");
        return;
    }

    hDir = CreateFile(
        path.wstring().c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL
    );

    if (hDir == INVALID_HANDLE_VALUE) {
		ULONG err = GetLastError();
        _CrtDbgBreak();
    }

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
    CloseHandle(hDir);
}

std::chrono::system_clock::time_point lastPlayTime;
bool isPlaying = false;

void ProcessFileEvent(FILE_NOTIFY_INFORMATION *pfni, std::filesystem::path parentPath) {
    do {
        std::wstring filename(pfni->FileName, pfni->FileNameLength / sizeof(WCHAR));
        if (filename.find(L".ogg") != std::wstring::npos) {
			lastPlayTime = std::chrono::system_clock::now();
            isPlaying = true;
            std::wstring fullPath = parentPath.wstring() + filename;
            CaptureThreadManager::GetInstance().UpdateGameState(1); // 0 = MENU, 1 = PLAYING, 2 = PAUSED
        } else if (filename.find(L".mp4") != std::wstring::npos) {
            std::chrono::seconds gap{ std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - lastPlayTime) };
            if (isPlaying && gap.count() > 1) {
                isPlaying = false;
                std::wstring fullPath = parentPath.wstring() + filename;
                CaptureThreadManager::GetInstance().UpdateGameState(0);
            }
        }
        pfni = (FILE_NOTIFY_INFORMATION*)((BYTE*)pfni + pfni->NextEntryOffset);
    } while (pfni->NextEntryOffset != 0);
}

std::thread loop;

void StartListenLoop() {
    run = true;
    loop = std::thread(ListenFileEventLoop);
    loop.detach();
}

void StopListenLoop() {
    run = false;
    CloseHandle(hDir);
}