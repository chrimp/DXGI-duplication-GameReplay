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
    m_SaveThread = std::thread(&CaptureThreadManager::SaveFrame, this);
}

void CaptureThreadManager::StopThread() {
    if (m_Run) {
        m_Run = false;
        m_CV.notify_all();
        m_Thread.join();
        m_SaveThread.join();
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
#if FALSE
        if (mappedResource.RowPitch >= 10240) {
            LogMessage(3, "Frame %d has invalid RowPitch: %d", count, mappedResource.RowPitch);
            LogMessage(3, "Its stagingTexture has %dx%d, %d(should be 87), %d", desc.Width, desc.Height, desc.Format, desc.ArraySize);
			//m_DeviceContext->Unmap(stagingTexture.Get(), 0);
            stagingTexture.Reset();
            return false;
        }
#endif
        uint8_t* src = static_cast<uint8_t*>(mappedResource.pData);
        std::vector<uint8_t> frameBuffer(desc.Width * desc.Height * 4);

        for (UINT y = 0; y < desc.Height; y++) {
            memcpy(&frameBuffer[y * desc.Width * 4], src + y * mappedResource.RowPitch, desc.Width * 4);
        }

        data = frameBuffer;
        m_DeviceContext->Unmap(stagingTexture.Get(), 0);
        //stagingTexture->Release();
        //data = std::move(m_FrameQueue.back());
        return true;
    }
    return false;
}

void CaptureThreadManager::DuplicationLoop() {
    int frameCount = 0;
    ID3D11DeviceContext* deviceContext;
    m_DuplicationManager.GetDevice()->GetImmediateContext(&m_DeviceContext);

    ComPtr<ID3D11Texture2D> stagingTexture = nullptr;
    std::chrono::duration<double> copyElapsed;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
    while (m_Run) {
        _FRAME_DATA data;
        bool timeout;
        DUPL_RETURN ret = m_DuplicationManager.GetFrame(&data, &timeout);
        if (ret == DUPL_RETURN_SUCCESS && !timeout) {
            stagingTexture.Reset();
            //LogMessage(0, "Got Frame %d", frameCount);
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
            std::chrono::time_point<std::chrono::high_resolution_clock> copyStart = std::chrono::high_resolution_clock::now();
            m_DeviceContext->CopyResource(stagingTexture.Get(), data.Frame);
            std::chrono::time_point<std::chrono::high_resolution_clock> copyEnd = std::chrono::high_resolution_clock::now();
            copyElapsed += copyEnd - copyStart;
            m_DuplicationManager.DoneWithFrame();
            {
                std::unique_lock<std::mutex> lock(m_Mutex);
                if (m_FrameQueue.size() >= 20) m_FrameQueue.pop();
                m_FrameQueue.push(std::move(stagingTexture));
            }
            m_CV.notify_one();
            m_FrameCount++;
        }
        else if (timeout) continue;
        else if (ret == DUPL_RETURN_ERROR_EXPECTED) {
			LogMessage(2, "Failed to get frame: %x", ret);
            continue;
        }
        else { abort(); }

        if (m_FPSEnabled) {
            std::chrono::time_point<std::chrono::high_resolution_clock> now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = now - lastTime;
            if (elapsed.count() >= 1.0) {
                lastTime = now;
                printf("\rFPS: %d | Copy waits: %f", frameCount, copyElapsed.count());
                fflush(stdout); // Ensure the output is immediately written to the console
                frameCount = 0;
                copyElapsed = std::chrono::duration<double>::zero();
            }
        }
    }

    LogMessage(0, "Captured %d frames", m_FrameCount);
}