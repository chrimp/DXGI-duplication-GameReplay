#include "ThreadManager.hpp"

ThreadManager::ThreadManager(DUPLICATIONMANAGER& duplMgr) : m_Run(false) {
    m_DuplicationManager = duplMgr;
}

ThreadManager::~ThreadManager() {
    StopThread();
}

void ThreadManager::StartThread() {
    m_Run = true;
    m_Thread = std::thread(&ThreadManager::DuplicationLoop, this);
}

void ThreadManager::StopThread() {
    if (m_Run) {
        m_Run = false;
        m_CV.notify_all();
        m_Thread.join();

    }
}

void ThreadManager::ToggleFPS() {
    m_FPSEnabled = !m_FPSEnabled;
    return;
}

_Success_(return)
bool ThreadManager::GetFrame(_Out_ std::vector<uint8_t>& data) {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_CV.wait(lock, [this] { return !m_FrameQueue.empty(); });
    if (!m_FrameQueue.empty()) {
        data = std::move(m_FrameQueue.front());
        m_FrameQueue.pop();
        return true;
    }
    return false;
}

void ThreadManager::DuplicationLoop() {
    int frameCount = 0;
    ID3D11DeviceContext* deviceContext;
    m_DuplicationManager.GetDevice()->GetImmediateContext(&deviceContext);

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
    while (m_Run) {
        _FRAME_DATA data;
        bool timeout;
        DUPL_RETURN ret = m_DuplicationManager.GetFrame(&data, &timeout);
        if (ret == DUPL_RETURN_SUCCESS && !timeout) {
            frameCount++;            
            D3D11_TEXTURE2D_DESC desc;
            data.Frame->GetDesc(&desc);

            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = deviceContext->Map(data.Frame, 0, D3D11_MAP_READ, 0, &mappedResource);

            if (SUCCEEDED(hr)) {
                uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);
                std::vector<uint8_t> frameBuffer(desc.Width * desc.Height * 4);

                for (UINT y = 0; y < desc.Height; y++) {
                    memcpy(&frameBuffer[y * desc.Width * 4], src + y * mappedResource.RowPitch, desc.Width * 4);
                }

                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    m_FrameQueue.push(frameBuffer);
                    m_CV.notify_one();
                }

                deviceContext->Unmap(data.Frame, 0);
            }

            m_DuplicationManager.DoneWithFrame();
            m_CV.notify_one();
        }

        if (m_FPSEnabled) {
            std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - lastTime;
            if (elapsed.count() >= 1.0) {
                lastTime = now;
                printf("FPS: %d\n", frameCount);
                frameCount = 0;
            }
        }
    }
}