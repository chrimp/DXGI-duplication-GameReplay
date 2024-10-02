#ifndef THREADMANAGER_HPP
#define THREADMANAGER_HPP

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <chrono>

#include "../DuplicationSamples/DuplicationManager.h"

class ThreadManager {
	public:
	ThreadManager(DUPLICATIONMANAGER& duplMgr);
	~ThreadManager();
	void StartThread();
	void StopThread();
	void ToggleFPS();
    _Success_(return)
    bool GetFrame(_Out_ std::vector<uint8_t>& data);

	private:
	std::queue<std::vector<uint8_t>> m_FrameQueue;
	std::thread m_Thread;
	std::mutex m_Mutex;
	std::condition_variable m_CV;
	std::atomic<bool> m_Run;
	std::atomic<bool> m_FPSEnabled;
	DUPLICATIONMANAGER m_DuplicationManager;
	void DuplicationLoop();
};

#endif