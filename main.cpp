#include <iostream>
#include <format>
#include <conio.h>
#include "./includes/MyTools/ThreadManager.hpp"

#pragma comment(lib,"d3d11.lib")

HRESULT SystemTransitionsExpectedErrors[] = {
                                                DXGI_ERROR_DEVICE_REMOVED,
                                                DXGI_ERROR_ACCESS_LOST,
                                                static_cast<HRESULT>(WAIT_ABANDONED),
                                                S_OK                                    // Terminate list with zero valued HRESULT
};

// These are the errors we expect from IDXGIOutput1::DuplicateOutput due to a transition
HRESULT CreateDuplicationExpectedErrors[] = {
                                                DXGI_ERROR_DEVICE_REMOVED,
                                                static_cast<HRESULT>(E_ACCESSDENIED),
                                                DXGI_ERROR_UNSUPPORTED,
                                                DXGI_ERROR_SESSION_DISCONNECTED,
                                                S_OK                                    // Terminate list with zero valued HRESULT
};

// These are the errors we expect from IDXGIOutputDuplication methods due to a transition
HRESULT FrameInfoExpectedErrors[] = {
                                        DXGI_ERROR_DEVICE_REMOVED,
                                        DXGI_ERROR_ACCESS_LOST,
                                        S_OK                                    // Terminate list with zero valued HRESULT
};

// These are the errors we expect from IDXGIAdapter::EnumOutputs methods due to outputs becoming stale during a transition
HRESULT EnumOutputsExpectedErrors[] = {
                                          DXGI_ERROR_NOT_FOUND,
                                          S_OK                                    // Terminate list with zero valued HRESULT
};

template<typename... Args>
void LogMessage(int level, const std::string& message, Args&&... args) {
    auto formattedMessage = std::vformat(message, std::make_format_args(std::forward<Args>(args)...));

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

    bool run = true;
    std::vector<uint8_t> frameData;

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
            if (threadManager.GetFrame(frameData)) {
                LogMessage(1, "Got frame");
            }
            break;
            return 0;
        }
    }

    device->Release();
    context->Release();
}