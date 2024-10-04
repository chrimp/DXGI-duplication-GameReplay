#include <conio.h>

#include "./includes/MyTools/ThreadManager.hpp"
#include "./includes/MyTools/LogMessage.hpp"
#include "./includes/MyTools/RawInputCapture.hpp"

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

CaptureThreadManager* threadManager = nullptr;

void CallThreadForSave();

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

    DUPLICATIONMANAGER* duplMgr = new DUPLICATIONMANAGER();
    if (duplMgr->InitDupl(device, 0) != DUPL_RETURN_SUCCESS) {
        LogMessage(3, "Failed to initialize duplication manager");
        return -1;
    }
    
    bool run = true;
    std::vector<uint8_t> frameData;

    threadManager = new CaptureThreadManager(*duplMgr);
    threadManager->StartThread();
    RegisterEnterCallback(CallThreadForSave);
    StartMessageLoop();

    while (run) {
        char key = _getch();
        switch (key) {
            case 'f':
            threadManager->ToggleFPS();
            continue;
            case 'q':
            threadManager->StopThread();
            run = false;
            continue;
            case 'c':
            if (threadManager->GetFrame(frameData)) {
                LogMessage(1, "Got frame");
            }
            continue;
            return 0;
        }
    }

    StopMessageLoop();

    device->Release();
    context->Release();

	//delete duplMgr;
}

void CallThreadForSave() {
    return;
    bool abort = (threadManager == nullptr || !threadManager->m_Run);

    if (abort) {
        LogMessage(3, "CallThreadForSave is with illegal ThreadManager");
        return;
    }

    threadManager->SaveFrame();
}