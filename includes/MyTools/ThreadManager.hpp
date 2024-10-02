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
	_FRAME_DATA GetFrame();

	private:
	std::queue<_FRAME_DATA> m_FrameQueue;
	std::thread m_Thread;
	std::mutex m_Mutex;
	std::condition_variable m_CV;
	std::atomic<bool> m_Run;
	std::atomic<bool> m_FPSEnabled;
	DUPLICATIONMANAGER m_DuplicationManager;
	void DuplicationLoop();
};