#include "ThreadManager.hpp"
#include "../wil/resource.h"

std::string GetGameStatusStr(GameState status) {
    switch (status) {
        case MENU:
            return "MENU";
        case PLAYING:
            return "PLAYING";
        case PAUSED:
            return "PAUSED";
        default:
            return "UNKNOWN";
    }
}

void throwIfFailed(HRESULT hr) {
    std::stringstream ss;
    ss << std::hex << hr;
    std::string hex = ss.str();
    if (FAILED(hr) || hr == S_FALSE) _CrtDbgBreak();
}

void CaptureThreadManager::CreateShader() {
    const char* vsSrc =
        "struct VSInput { float3 position : POSITION; float2 texcoord : TEXCOORD0; };"
        "struct PSInput { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };"
        "PSInput main(VSInput input) {"
        "    PSInput output;"
        "    output.position = float4(input.position, 1.0);"
        "    output.texcoord = input.texcoord;"
        "    return output;"
        "}";

    const char* psSrc =
        "struct PSInput { float4 position : SV_POSITION; float2 texcoord : TEXCOORD0; };"
        "Texture2D tex : register(t0);"
        "SamplerState samplerState : register(s0);"
        "float4 main(PSInput input) : SV_Target {"
        "    return tex.Sample(samplerState, input.texcoord);"
        "}";
    UINT compileFlags = 0; //D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    HRESULT hr = D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
			char* error = static_cast<char*>(errorBlob->GetBufferPointer());
			LogMessage(3, "Error compiling vertex shader: %s", error);
			errorBlob->Release();
        }
        _CrtDbgBreak();
    }
    hr = D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            char* error = static_cast<char*>(errorBlob->GetBufferPointer());
            LogMessage(3, "Error compiling vertex shader: %s", error);
            errorBlob->Release();
        }
        _CrtDbgBreak();
    }
    throwIfFailed(m_Device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_VertexShader.ReleaseAndGetAddressOf()));
    throwIfFailed(m_Device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_PixelShader.ReleaseAndGetAddressOf()));

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    throwIfFailed(m_Device->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_InputLayout.ReleaseAndGetAddressOf()));
    m_DeviceContext->IASetInputLayout(m_InputLayout.Get());

    return;
}

void CaptureThreadManager::CreateQuad() {
    struct Vertex {
        float position[3];
        float texcoord[2];
    };

    Vertex quadVertices[] = {
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } },
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(quadVertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = quadVertices;

    throwIfFailed(m_Device->CreateBuffer(&bufferDesc, &initData, m_VertexBuffer.ReleaseAndGetAddressOf()));

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    m_DeviceContext->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
    m_DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    return;
}

void CaptureThreadManager::Init(HWND hWnd) {
    m_hWnd = hWnd;

    m_gameHWND = FindWindow(NULL, L"WJMAX.exe");
    if (m_gameHWND == INVALID_HANDLE_VALUE) _CrtDbgBreak();

    UINT flag = 0;
    #ifdef _DEBUG
    flag |= D3D11_CREATE_DEVICE_DEBUG;
    #endif

    DXGI_SWAP_CHAIN_DESC scDesc = {};
    scDesc.BufferCount = 1;
    scDesc.BufferDesc.Width = 692;
    scDesc.BufferDesc.Height = 1440;
    scDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.OutputWindow = hWnd;
    scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;
	scDesc.Windowed = TRUE;

    throwIfFailed(D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flag,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &scDesc,
        m_swapChain.GetAddressOf(),
        m_Device.GetAddressOf(),
        nullptr,
        m_DeviceContext.GetAddressOf()
    ));
	m_DuplicationManager.InitDupl(m_Device.Get(), 0);

    ComPtr<ID3D11Texture2D> backbuffer;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf()));
    m_Device->CreateRenderTargetView(backbuffer.Get(), nullptr, m_RenderTargetView.GetAddressOf());

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = 692;
    texDesc.Height = 1440;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

    m_Device->CreateTexture2D(&texDesc, nullptr, m_Texture.GetAddressOf());
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
    m_Device->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_ShaderResourceView.GetAddressOf());

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    m_Device->CreateSamplerState(&samplerDesc, m_SamplerState.GetAddressOf());
    m_DeviceContext->OMSetRenderTargets(1, m_RenderTargetView.GetAddressOf(), nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = 692.0f;
    viewport.Height = 1440.0f;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    m_DeviceContext->RSSetViewports(1, &viewport);

    CreateShader();
    CreateQuad();

    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = 2560;
    stagingDesc.Height = 1440;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags = 0;
    stagingDesc.MiscFlags = 0;
    m_Device->CreateTexture2D(&stagingDesc, nullptr, m_StagingTexture.GetAddressOf());
    return;
}

CaptureThreadManager::~CaptureThreadManager() {
    StopThread();
}

void CaptureThreadManager::StartThread() {
    m_Run = true;
    m_Thread = std::thread(&CaptureThreadManager::DuplicationLoop, this);
}

void CaptureThreadManager::StopThread() {
    if (m_Run) {
        m_Run = false;
        m_Thread.join();
        //m_SaveThread.join();
    }
}

// Consider changing below two methods to void

GameState CaptureThreadManager::PauseCallback() {
    // This function will be called when Escape (VK_ESCAPE) is pressed
    // This will not be executed (returns) if the game is not in the PLAYING state

    if (m_GameState == PLAYING) {
        m_GameState = PAUSED;
        LogMessage(0, "Game paused");
    }

	return m_GameState;
}

int xcoords[8] = { 1040, 1100, 1160, 1190, 1260, 1330, 1365, 1470 };
int ycoords[4] = { 540, 690, 830, 960 };

GameState CaptureThreadManager::ResumeCallback() {
    if (m_GameState != PAUSED) {
        LogMessage(0, "Game is not paused");
        return m_GameState;
    }

	std::unique_lock lockForStaging(m_Mutex);
    m_BlockLoop = true;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = m_DeviceContext->Map(m_StagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    throwIfFailed(hr);
    uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);
    cv::Mat frame(1440, 2560, CV_8UC4, src); // frame and mappedResource share the same memory
    cv::Mat image;
    cv::cvtColor(frame, image, cv::COLOR_BGRA2GRAY); // Now image is copied from frame, they don't share the same memory
    cv::threshold(image, image, 160, 255, cv::THRESH_BINARY);
    m_DeviceContext->Unmap(m_StagingTexture.Get(), 0);
    m_BlockLoop = false;
    m_BlockLoopCV.notify_one();
	lockForStaging.unlock();

    bool selectionCheck = false;
    bool pause = true;
    for (const int& x: xcoords) {
        if (image.at<uint8_t>(270, x) != 255) {
            pause = false;
            m_GameState = PAUSED;
            LogMessage(0, "Can't detect PAUSE on top: ResumeCallback() | %d", x);
            goto clean;
        }
    }
    
    for (const int& y: ycoords) {
        LogMessage(0, "Checking %d", y);
        if (image.at<uint8_t>(y, 1597) == 255) {
            if (y == 540) {
                m_GameState = PLAYING;
                // Set flag here for buffer exhuastion
                if (!m_ShowReplay) {
                    m_ShowReplay = true;
                    playReplay();
                    m_ShowReplay = false;
                }
                LogMessage(0, "Game resumed: ResumeCallback() | %d", y);
                break;
            }
            else if (y == 690) {
                m_GameState = PAUSED;
                LogMessage(0, "Game is being restarted: ResumeCallback() | %d", y);
                break;
            }
            else if (y == 830) {
                m_GameState = PAUSED;
                LogMessage(0, "Game is in settings: ResumeCallback() | %d", y);
                break;
            }
            else if (y == 960) {
                UpdateGameState((unsigned int)GameState::MENU);
                LogMessage(0, "User quitted the play: ResumeCallback() | %d", y);
                break;
            }
        }
    }
clean:
    return m_GameState;
    // Should implement esc to resume in future
}

void CaptureThreadManager::UpdateGameState(unsigned int status) {
    if (m_GameState == (GameState)status) return;

    if (status == (unsigned int)GameState::MENU) {
        m_BlockLoop = true;
        m_ReplayDeque.clear();
		m_BlockLoop = false;
		m_BlockLoopCV.notify_one();
    }

    m_WaitDuration = 1.0 / 360;
    m_GameState = static_cast<GameState>(status); 
    LogMessage(0, "Updated Game status: %d", status);
}

void SetCursorPosition(int x, int y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

int GetConsoleHeight() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

void CaptureThreadManager::DuplicationLoop() {
    int frameCount = 0;

    D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = 692;
	texDesc.Height = 1440;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	D3D11_BOX box; box.left = 187; box.right = 879; box.top = 0; box.bottom = 1440; box.front = 0; box.back = 1;

    HRESULT hr;
    std::chrono::duration<double> copyElapsed = std::chrono::duration<double>::zero();
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> lastCapture = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> sleepElapsed = std::chrono::duration<double>::zero();

    SetWindowPos(m_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    while (m_Run) {
        _FRAME_DATA data; bool timeout;

        DUPL_RETURN ret = m_DuplicationManager.GetFrame(&data, &timeout);
        if (ret == DUPL_RETURN_SUCCESS && !timeout) {
            ComPtr<ID3D11Texture2D> frameTexture;
			m_Device->CreateTexture2D(&texDesc, nullptr, frameTexture.GetAddressOf());
            std::chrono::time_point<std::chrono::high_resolution_clock> copyStart = std::chrono::high_resolution_clock::now();
            
            std::unique_lock lockForStaging(m_Mutex);
			m_BlockLoopCV.wait(lockForStaging, [this] { return !m_BlockLoop; });
			if (m_BlockLoop) {
				lockForStaging.unlock();
                lastCapture = std::chrono::high_resolution_clock::now();
                m_DuplicationManager.DoneWithFrame();
				continue;
			}

            m_DeviceContext->CopyResource(m_StagingTexture.Get(), data.Frame);
            m_DeviceContext->CopySubresourceRegion(frameTexture.Get(), 0, 0, 0, 0, data.Frame, 0, &box);
            if (frameTexture.Get() == NULL) _CrtDbgBreak();
            lockForStaging.unlock();
            frameCount++;
            std::chrono::time_point<std::chrono::high_resolution_clock> copyEnd = std::chrono::high_resolution_clock::now();
            copyElapsed += copyEnd - copyStart;
            if (m_GameState == PLAYING && !m_ShowReplay) {
                FrameData oldest = { nullptr, std::chrono::high_resolution_clock::now() };
                if (!m_ReplayDeque.empty()) oldest = m_ReplayDeque.front();
                if (std::chrono::high_resolution_clock::now() - oldest.Time > std::chrono::seconds(3)) {
                    m_ReplayDeque.pop_front();

                    // Checking twice for cases where the deque size is excessive
                    // Popping once is not enough since new frame is being added soon (size will not be changed then)
                    if (!m_ReplayDeque.empty()) {
                        oldest = m_ReplayDeque.front();
                        if (std::chrono::high_resolution_clock::now() - oldest.Time > std::chrono::seconds(3)) m_ReplayDeque.pop_front();
                    }
                }

                FrameData newFrame = { frameTexture, std::chrono::high_resolution_clock::now() }; // .Frame, .Time
                m_ReplayDeque.push_back(std::move(newFrame));
            }

            std::chrono::time_point<std::chrono::high_resolution_clock> preSleep = std::chrono::high_resolution_clock::now();
			while (std::chrono::high_resolution_clock::now() - lastCapture < std::chrono::duration<double>(m_WaitDuration)) continue;
            sleepElapsed += std::chrono::high_resolution_clock::now() - preSleep;

            lastCapture = std::chrono::high_resolution_clock::now();
            m_DuplicationManager.DoneWithFrame();
        }
        else if (timeout) {
            if (!m_ShowReplay && m_GameState == PLAYING) {
                FrameData oldest = { nullptr, std::chrono::high_resolution_clock::now() };
                if (!m_ReplayDeque.empty()) oldest = m_ReplayDeque.front(); else continue;

                if (std::chrono::high_resolution_clock::now() - oldest.Time > std::chrono::seconds(3)) {
                    std::move(m_ReplayDeque.front());
                    m_ReplayDeque.pop_front();
                }

                ComPtr<ID3D11Texture2D> lastTexture = m_ReplayDeque.back().Frame;
                std::chrono::time_point<std::chrono::high_resolution_clock> nowTime = std::chrono::high_resolution_clock::now();
                ComPtr<ID3D11Texture2D> duplicatedTexture;
                m_Device->CreateTexture2D(&texDesc, nullptr, duplicatedTexture.GetAddressOf());
                m_DeviceContext->CopyResource(duplicatedTexture.Get(), lastTexture.Get());

                FrameData newFrame;
                newFrame.Frame = duplicatedTexture;
                newFrame.Time = nowTime;
                //frameCount++;
                m_ReplayDeque.push_back(std::move(newFrame));
                lastCapture = std::chrono::high_resolution_clock::now();
            }
        }
        else { abort(); }

        if (m_FPSEnabled) {
            std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - lastTime;
            if (elapsed.count() >= 1.0) {
                lastTime = now;
                CONSOLE_SCREEN_BUFFER_INFO csbi;
                GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
                COORD originalPos = csbi.dwCursorPosition;
                SetCursorPosition(0, GetConsoleHeight() - 1);

                // Clear the line
                for (int i = 0; i < csbi.dwSize.X; i++) {
                    std::cout << " ";
                }

                double sleepPerFrame = sleepElapsed.count() / frameCount;
                double currentFrameTime = elapsed.count() / frameCount;
                double targetFrameTime = 1.0 / 360;
                double adjustmentPerFrame = targetFrameTime - currentFrameTime;
                
                if (currentFrameTime < targetFrameTime * 0.7 && m_WaitDuration == 0) {
                    m_WaitDuration = 1.0 / 360;
                }

                // Although this kind of control is good enough, consider embedding the busy-wait within the main loop;
                // I can always grab last DoneWithFrame() call then put a busy-wait loop until 1/360 seconds have passed. <- Turned out that it stays -1 fps off the target
                if (m_WaitDuration <= 0) m_WaitDuration = 0;

                SetCursorPosition(0, GetConsoleHeight() - 1);
                printf("FPS: %d | Sleep adjustment: %.6f | Deque size: %d | Game status: %s | Sleep: %.6f / frame", frameCount, adjustmentPerFrame, m_ReplayDeque.size(), GetGameStatusStr(m_GameState).c_str(), sleepPerFrame);
                fflush(stdout); // Ensure the output is immediately written to the console
                SetCursorPosition(originalPos.X, originalPos.Y);

                frameCount = 0;
				sleepElapsed = std::chrono::duration<double>::zero();
                copyElapsed = std::chrono::duration<double>::zero();
            }
        }
    }
}

void CaptureThreadManager::playReplay() {
    // The Wait duration is not exact. According to high-speed recording it's more like 0.588* seconds, however still indeterministic.
    // Also due to some overhead even with busy-wait animation itself ends just a bit short.
    // Anyway I cannot conclude what is the exact duration without decompiling the original game.
    double animWait = 0.58;
	std::chrono::high_resolution_clock::time_point animWaitStart = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> animWaitDur(animWait);
	while (std::chrono::high_resolution_clock::now() - animWaitStart < animWaitDur) {
		continue;
	}

    SetWindowPos(m_hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
    
    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    std::unique_lock<std::mutex> lock(m_Mutex);

	unsigned int replaySize = m_ReplayDeque.size();
    double frameTime = 1 / (replaySize / 3.0);

    while (!m_ReplayDeque.empty()) {
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime = std::chrono::high_resolution_clock::now();
        ComPtr<ID3D11Texture2D> frame = std::move(m_ReplayDeque.front().Frame);
        m_DeviceContext->CopyResource(m_Texture.Get(), frame.Get());
        m_DeviceContext->ClearRenderTargetView(m_RenderTargetView.Get(), clearColor);
        m_DeviceContext->PSSetShaderResources(0, 1, m_ShaderResourceView.GetAddressOf());
        m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());
        m_DeviceContext->VSSetShader(m_VertexShader.Get(), nullptr, 0);
        m_DeviceContext->PSSetShader(m_PixelShader.Get(), nullptr, 0);
        m_DeviceContext->Draw(4, 0);
        HRESULT hr = m_swapChain->Present(1, 0);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING);
        else if (FAILED(hr)) { _CrtDbgBreak(); }
        m_ReplayDeque.pop_front();
		std::chrono::duration<double> gap = std::chrono::high_resolution_clock::now() - startTime;
        while (gap.count() < frameTime) {
			gap = std::chrono::high_resolution_clock::now() - startTime;
            continue;
        }
    }

    SetWindowPos(m_gameHWND, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE); // Tries to move the game window to the top--Does it really matter?
    SetWindowPos(m_hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}