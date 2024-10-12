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
    m_ReplayDeque.clear();

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
        0,
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
    //m_SaveThread = std::thread(&CaptureThreadManager::SaveFrame, this);
}

void CaptureThreadManager::StopThread() {
    if (m_Run) {
        m_Run = false;
        m_CV.notify_all();
        m_Thread.join();
        //m_SaveThread.join();
    }
}

void CaptureThreadManager::ToggleFPS() {
    m_FPSEnabled = !m_FPSEnabled;
    return;
}

void CaptureThreadManager::SaveFrame() {
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
	std::chrono::time_point<std::chrono::high_resolution_clock> saveTime = std::chrono::high_resolution_clock::now();
    while (m_Run) {
        std::chrono::time_point<std::chrono::high_resolution_clock> nowTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = nowTime - lastTime;
        std::vector<uint8_t> frameData;

        //if (elapsed.count() < 1.0/360) continue;
        if (!GetFrame(frameData)) {
            //LogMessage(2, "Failed to get frame");
            continue;
        }
        lastTime = nowTime;

        cv::Mat frame(1440, 2560, CV_8UC4, frameData.data());
        cv::Mat image;
        cv::cvtColor(frame, image, cv::COLOR_BGRA2BGR);

        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm localTime;
        localtime_s(&localTime, &now);
        std::string time = std::to_string(localTime.tm_mday) + std::to_string(localTime.tm_hour) + std::to_string(localTime.tm_min) + std::to_string(localTime.tm_sec);
        
        if (nowTime - saveTime > std::chrono::seconds(30)) {
            cv::imwrite(time + ".jpg", image);
            saveTime = nowTime;
        }
        //std::cout << "Image written" << std::endl;
    }
}

uint8_t count = -1;
/*
_Success_(return)
bool CaptureThreadManager::GetFrame(_Out_ std::vector<uint8_t>& data) {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_CV.wait(lock, [this] { return !m_FrameQueue.empty(); });
    if (!m_FrameQueue.empty()) {
        count++;
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        D3D11_TEXTURE2D_DESC desc;
        ComPtr<ID3D11Texture2D> stagingTexture = std::move(m_FrameQueue.front());
        m_FrameQueue.pop();
        stagingTexture->GetDesc(&desc);
        
        HRESULT hr = m_DeviceContext->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
        if (FAILED(hr)) {
            LogMessage(3, "Failed to Map: 0x%x", hr);
            abort();
        }
        uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);
        std::vector<uint8_t> frameBuffer(desc.Width * desc.Height * 4);

        for (UINT y = 0; y < desc.Height; y++) {
            memcpy(&frameBuffer[y * desc.Width * 4], src + y * mappedResource.RowPitch, desc.Width * 4);
        }

        data = frameBuffer;
        m_DeviceContext->Unmap(stagingTexture.Get(), 0);
        return true;
    }
    return false;
}
*/
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

    D3D11_BOX box;
    box.left = 187;
    box.top = 0;
    box.front = 0;
    box.right = 879;
    box.bottom = 1440;
    box.back = 1;

    HRESULT hr;

    std::chrono::duration<double> copyElapsed;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
    while (m_Run) {
        _FRAME_DATA data;
        bool timeout;
        DUPL_RETURN ret = m_DuplicationManager.GetFrame(&data, &timeout);
        if (ret == DUPL_RETURN_SUCCESS && !timeout) {
            ComPtr<ID3D11Texture2D> frameTexture;
			m_Device->CreateTexture2D(&texDesc, nullptr, frameTexture.GetAddressOf());
            std::chrono::time_point<std::chrono::high_resolution_clock> copyStart = std::chrono::high_resolution_clock::now();
            std::unique_lock lockForStaging(m_Mutex);
            m_DeviceContext->CopyResource(m_StagingTexture.Get(), data.Frame);
            lockForStaging.unlock();
            m_DeviceContext->CopySubresourceRegion(frameTexture.Get(), 0, 0, 0, 0, data.Frame, 0, &box);
            frameCount++;
            std::chrono::time_point<std::chrono::high_resolution_clock> copyEnd = std::chrono::high_resolution_clock::now();
            copyElapsed += copyEnd - copyStart;
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                if (m_ReplayDeque.size() >= 360 * 3) { // Expected VRAM usage: 4.3G
					ComPtr<ID3D11Texture2D> drop = std::move(m_ReplayDeque.front());
					m_DeviceContext->CopyResource(m_Texture.Get(), drop.Get());
                    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
                    m_DeviceContext->ClearRenderTargetView(m_RenderTargetView.Get(), clearColor);
                    m_DeviceContext->PSSetShaderResources(0, 1, m_ShaderResourceView.GetAddressOf());
                    m_DeviceContext->PSSetSamplers(0, 1, m_SamplerState.GetAddressOf());

                    m_DeviceContext->VSSetShader(m_VertexShader.Get(), nullptr, 0);
                    m_DeviceContext->PSSetShader(m_PixelShader.Get(), nullptr, 0);
                    m_DeviceContext->Draw(4, 0);
                    hr = m_swapChain->Present(1, DXGI_PRESENT_DO_NOT_WAIT);
                    if (hr == DXGI_ERROR_WAS_STILL_DRAWING);
                    else if (FAILED(hr)) { _CrtDbgBreak(); }
                    m_FrameCount++;
                    drop->Release();
                    m_ReplayDeque.pop_front();
                }
                m_ReplayDeque.push_back(std::move(frameTexture));
            }
            m_DuplicationManager.DoneWithFrame();
            m_CV.notify_one();
        }
        else if (timeout) continue;
        else { abort(); }

        if (m_FPSEnabled) {
            std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - lastTime;
            if (elapsed.count() >= 1.0 && frameCount > 0) {
                lastTime = now;
                printf("\rFPS: %d | Copy waits: %.4f | Deque size: %d | Game status: %s", frameCount, copyElapsed.count(), m_ReplayDeque.size(), GetGameStatusStr(m_IsGamePaused).c_str());
                fflush(stdout); // Ensure the output is immediately written to the console
                frameCount = 0;
                copyElapsed = std::chrono::duration<double>::zero();
            }
        }
    }

    LogMessage(0, "Captured %d frames", m_FrameCount);
}