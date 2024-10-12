#ifndef DIRCHANGES_HPP
#define DIRCHANGES_HPP

#include <string>
#include <thread>
#include <filesystem>
#include <windows.h>
#include <fileapi.h>
#include <TlHelp32.h>
#include <atomic>
#include "../MyTools/LogMessage.hpp"
#include "ThreadManager.hpp"


void ListenFileEventLoop();
void ProcessFileEvent(FILE_NOTIFY_INFORMATION *pfni, std::filesystem::path parentPath);
void StartListenLoop();
void StopListenLoop();
std::filesystem::path FindProcessPath();


#endif // DIRCHANGES_HPP