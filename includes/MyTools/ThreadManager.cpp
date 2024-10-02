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
    m_Run = false;
    m_CV.notify_all();
    m_Thread.join();
}

void ThreadManager::ToggleFPS() {
    m_FPSEnabled = !m_FPSEnabled;
}

_FRAME_DATA ThreadManager::GetFrame() {
    std::unique_lock<std::mutex> lock(m_Mutex);
    m_CV.wait(lock, [this] { return !m_FrameQueue.empty(); });
    _FRAME_DATA data = m_FrameQueue.front();
    m_FrameQueue.pop();
    return data;
}

void ThreadManager::DuplicationLoop() {
    int frameCount = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTime = std::chrono::high_resolution_clock::now();
    while (m_Run) {
        _FRAME_DATA data;
        bool timeout;
        DUPL_RETURN ret = m_DuplicationManager.GetFrame(&data, &timeout);
        if (ret == DUPL_RETURN_SUCCESS && !timeout) {
            frameCount++;
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_FrameQueue.push(data);
            m_DuplicationManager.DoneWithFrame();
            m_CV.notify_one();

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
}