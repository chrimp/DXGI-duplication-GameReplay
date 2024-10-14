#include <conio.h>
#include <filesystem>

#include "./includes/MyTools/ThreadManager.hpp"
#include "./includes/MyTools/LogMessage.hpp"
#include "./includes/MyTools/RawInputCapture.hpp"
#include "./includes/MyTools/DirChanges.hpp"

#pragma comment(lib,"d3d11.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib, "windowsapp.lib")

using namespace DirectX;

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

ComPtr<ID3D11DeviceContext> context;
ComPtr<ID3D11Texture2D> texture;
HWND hWnd;

// Here for creating a new window to draw frames

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            ShowCursor(FALSE);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_INPUT:
            ProcessRawInput(lParam);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HWND CreateWindowInstance(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DDAPIWindowClass";

    RegisterClass(&wc);

    HWND hWnd = CreateWindowEx(
        WS_EX_NOACTIVATE, //WS_EX_LAYERED,
        wc.lpszClassName,
        L"DDAPI Frame Preview",
        WS_POPUP,
        187, 0,
        692, 1440,
        NULL, NULL, hInstance, NULL);
    
    if (hWnd) ShowWindow(hWnd, nCmdShow);
    return hWnd;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    std::filesystem::path here = "./";
    try {
        for (const auto& entry: std::filesystem::directory_iterator(here)) {
            if (entry.is_regular_file() && entry.path().extension() == ".jpg") {
                std::filesystem::remove(entry.path());
            }
        }
    } catch (std::filesystem::filesystem_error& e) {
        LogMessage(3, "Error: %s", e.what());
    } catch (std::exception& e) {
        LogMessage(3, "Error: %s", e.what());
    }

    AllocConsole();
    FILE *pCout, *pCerr;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
    std::cout << "Console is ready" << std::endl;

    hWnd = CreateWindowInstance(hInstance, nCmdShow);
    CaptureThreadManager::GetInstance().Init(hWnd);

    RegisterRawInput(hWnd);

    StartListenLoop();
    CaptureThreadManager::GetInstance().StartThread();
    MSG msg = {};
    while (WM_QUIT != msg.message) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

	CaptureThreadManager::GetInstance().StopThread();
	StopListenLoop();

    if (pCout) fclose(pCout);
    if (pCerr) fclose(pCerr);
    FreeConsole();
    return 0;
}