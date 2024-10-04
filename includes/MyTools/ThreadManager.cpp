#include "ThreadManager.hpp"

CaptureThreadManager::CaptureThreadManager(DUPLICATIONMANAGER& duplMgr) : m_Run(false) {
    m_DuplicationManager = duplMgr;
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
        m_CV.notify_all();
        m_Thread.join();

    }
}

void CaptureThreadManager::ToggleFPS() {
    m_FPSEnabled = !m_FPSEnabled;
    return;
}

void CaptureThreadManager::SaveFrame() {
    std::vector<uint8_t> frameData;
    if (!GetFrame(frameData)) {
        LogMessage(2, "Failed to get frame");
    }

    cv::Mat frame(1440, 2560, CV_8UC4, frameData.data());
    cv::Mat image;
    cv::cvtColor(frame, image, cv::COLOR_BGRA2BGR);

    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm localTime;
    localtime_s(&localTime, &now);
    std::string time = std::to_string(localTime.tm_mday) + std::to_string(localTime.tm_hour) + std::to_string(localTime.tm_min) + std::to_string(localTime.tm_sec);
    
    cv::imwrite(time + ".png", image);
    std::cout << "Image written" << std::endl;
}

_Success_(return)
bool CaptureThreadManager::GetFrame(_Out_ std::vector<uint8_t>& data) {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_CV.wait(lock, [this] { return !m_FrameQueue.empty(); });
    if (!m_FrameQueue.empty()) {
        //data = std::move(m_FrameQueue.back());
        return true;
    }
    return false;
}

void CaptureThreadManager::DuplicationLoop() {
    int frameCount = 0;
    ID3D11DeviceContext* deviceContext;
    m_DuplicationManager.GetDevice()->GetImmediateContext(&deviceContext);

    ComPtr<ID3D11Texture2D> stagingTexture = nullptr;

    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
    while (m_Run) {
        _FRAME_DATA data;
        bool timeout;
        m_DuplicationManager.DoneWithFrame();
        DUPL_RETURN ret = m_DuplicationManager.GetFrame(&data, &timeout);
        if (ret == DUPL_RETURN_SUCCESS && !timeout) {
            frameCount++;
            D3D11_TEXTURE2D_DESC desc;
            data.Frame->GetDesc(&desc);

            if (!stagingTexture) {
                if (stagingTexture) stagingTexture->Release();

                D3D11_TEXTURE2D_DESC newDesc = desc;
                newDesc.Usage = D3D11_USAGE_STAGING;
                newDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                newDesc.BindFlags = 0;
                newDesc.MiscFlags = 0;

                HRESULT stagingHr = m_DuplicationManager.GetDevice()->CreateTexture2D(&newDesc, nullptr, &stagingTexture);
                if (FAILED(stagingHr)) {
                    LogMessage(3, "Failed to create staging texture: 0x%x", stagingHr);
                    abort();
                }
            }

            deviceContext->CopyResource(stagingTexture.Get(), data.Frame);

            std::unique_lock<std::mutex> lock(m_Mutex);
            m_FrameQueue.push(stagingTexture);
            m_CV.notify_one();
            lock.unlock();
            /*
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            HRESULT hr = deviceContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mappedResource);

            if (SUCCEEDED(hr)) {
                uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);
                std::vector<uint8_t> frameBuffer(desc.Width * desc.Height * 4);

                for (UINT y = 0; y < desc.Height; y++) {
                    memcpy(&frameBuffer[y * desc.Width * 4], src + y * mappedResource.RowPitch, desc.Width * 4);
                }

                {
                    std::unique_lock<std::mutex> lock(m_Mutex);
                    if (m_FrameQueue.size() >= 200) m_FrameQueue.pop();
                    m_FrameQueue.push(frameBuffer);
                    m_CV.notify_one();
                }

                deviceContext->Unmap(stagingTexture, 0);
            }
            else {
                LogMessage(3, "Failed to Map: 0x%x", hr);
                abort();
            }
            */
        }
        else if (timeout) continue;
        else {
			LogMessage(3, "Failed to get frame: %x", ret);
			abort();
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