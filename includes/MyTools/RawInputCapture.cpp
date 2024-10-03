#include "RawInputCapture.hpp"
#include "LogMessage.hpp"

std::atomic<bool> running = true;
void* enterCallback = nullptr;

HWND CreateHiddenWindow() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"RawInputCaptureClass";

    if (!RegisterClass(&wc)) {
        LogMessage(3, "Failed to register window class: %d", GetLastError());
        return NULL;
    }

    HWND hwnd = CreateWindowEx(0, wc.lpszClassName, L"RawInputCapture", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), NULL);

    if (!hwnd) {
        LogMessage(3, "Failed to create window: %d", GetLastError());
        return NULL;
    }

    return hwnd;
}

void RegisterEnterCallback(void* callback) {
    enterCallback = callback;
}

void RegisterRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid;

    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;

    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        LogMessage(3, "Failed to register raw input devices: %d", GetLastError());
    }
}

void ProcessRawInput(LPARAM lParam) {
    UINT dwSize = 0;
    GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

    LPBYTE lpb = new BYTE[dwSize];
    if (lpb == NULL) {
        return;
    }

    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
        LogMessage(2, "GetRawInputData does not return correct size!");
        return;
    }

    RAWINPUT* raw = (RAWINPUT*)lpb;
    if (raw->header.dwType == RIM_TYPEKEYBOARD) {
        RAWKEYBOARD rawKB = raw->data.keyboard;

        switch (rawKB.Message) {
            case WM_KEYDOWN:
                if (rawKB.VKey == VK_RETURN) {
                    if (enterCallback != nullptr) {
                        ((void(*)())enterCallback)();
                    }
                }
                //LogMessage(0, "Key down: %d", rawKB.VKey);
                break;
            case WM_KEYUP:
                //LogMessage(0, "Key up: %d", rawKB.VKey);
                break;
        }
    }

    if (lpb == nullptr) LogMessage(3, "LPB is null at the end of ProcessRawInput");
    else delete[] lpb;
    return;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_INPUT:
            ProcessRawInput(lParam);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

void StartMessageLoop() {
    std::thread messageLoop(MessageLoop);
    messageLoop.detach();
}

void StopMessageLoop() {
    running = false;
    PostQuitMessage(0);
}

void MessageLoop() {
    HWND hwnd = CreateHiddenWindow();
    if (hwnd == NULL) return;

    RegisterRawInput(hwnd);

    MSG msg;

    while (running && GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_INPUT) {
            ProcessRawInput(msg.lParam);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
