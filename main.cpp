#include <iostream>
#include <format>
#include <conio.h>
#include "./includes/MyTools/ThreadManager.hpp"

void LogMessage(int level, const std::string& message, ...);

int main() {
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    IDXGIOutputDuplication* deskDupl;

    D3D_FEATURE_LEVEL featureLevel;
    D3D11CreateDevice(nullptr,
    D3D_DRIVER_TYPE_HARDWARE,
    nullptr,
    0,
    nullptr,
    0,
    D3D11_SDK_VERSION,
    &device,
    &featureLevel,
    &context);

    DUPLICATIONMANAGER duplMgr;
    if (duplMgr.InitDupl(device, 0) != DUPL_RETURN_SUCCESS) {
        LogMessage(3, "Failed to initialize duplication manager");
        return -1;
    }

    _FRAME_DATA frameData;
    ID3D11Texture2D* texture;
    bool run = true;

    ThreadManager threadManager(duplMgr);
    threadManager.StartThread();

    while (run) {
        char key = _getch();
        switch (key) {
            case 'f':
            threadManager.ToggleFPS();
            break;
            case 'q':
            threadManager.StopThread();
            run = false;
            break;
            case 'c':
            frameData = threadManager.GetFrame();

            texture = frameData.Frame;

            break;
            return 0;
        }
    }

    device->Release();
    context->Release();
}

template<typename... Args>
void LogMessage(int level, const std::string& message, Args&&... args) {
    auto formattedMessage = std::format(message, std::forward<Args>(args)...);

    switch (level) {
        case 0:
        std::cout << "[DEBUG]: " << formattedMessage << std::endl;
        break;
        case 1:
        std::cout << "[INFO]: " << formattedMessage << std::endl;
        break;
        case 2:
        std::cout << "\033[0;93m[WARNING]: " << formattedMessage << "\033[0m" << std::endl;
        break;
        case 3:
        std::cout << "\033[0;01m[ERROR]: " << formattedMessage << "\033[0m" << std::endl;
        break;
    }

    return;
}