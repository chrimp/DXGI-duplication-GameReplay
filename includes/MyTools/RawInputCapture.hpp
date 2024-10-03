#ifndef RAWINPUTCAPTURE_HPP
#define RAWINPUTCAPTURE_HPP

#include "../DuplicationSamples/DuplicationManager.h"
#include "ThreadManager.hpp"
#include <windows.h>
#include <thread>
#include <atomic>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HWND CreateHiddenWindow();

void RegisterEnterCallback(void* callback);
void RegisterRawInput(HWND hwnd);
void ProcessRawInput(LPARAM lParam);
void StartMessageLoop();
void StopMessageLoop();
void MessageLoop();

#endif