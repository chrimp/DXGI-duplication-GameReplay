#ifndef THREADMANAGER_HPP
#define THREADMANAGER_HPP

#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <d3dcompiler.h>

#pragma comment(lib, "D3DCompiler.lib")

#include "../DuplicationSamples/DuplicationManager.h"
#include "LogMessage.hpp"
#include "opencv2/opencv.hpp"

enum GameState {
	MENU,
	PLAYING,
	PAUSED
};

class CaptureThreadManager {
	public:
	static CaptureThreadManager& GetInstance() {
		static CaptureThreadManager instance;
		return instance;
	}
	CaptureThreadManager(CaptureThreadManager const&) = delete;
	void operator=(CaptureThreadManager const&) = delete;

	void Init(HWND hWnd);
	void StartThread();
	void StopThread();
	void ToggleFPS();
	void SaveFrame();
	void SethWnd(HWND hWnd) { m_hWnd = hWnd; }
	void UpdateGameStatus(unsigned int status) { m_IsGamePaused = static_cast<GameState>(status); }
	bool ResumeCallback();
	bool QueueForCopy();
	ComPtr<ID3D11Device> GetDevice() { return m_Device; }

    _Success_(return)
    bool GetFrame(_Out_ std::vector<uint8_t>& data);
	std::atomic<bool> m_Run;

	private:
	CaptureThreadManager() : m_Run(false) {};
	~CaptureThreadManager();

	unsigned long m_FrameCount = 0;
	bool m_Draw = false;
	
	ComPtr<ID3D11Texture2D> m_CopyDest;
	ComPtr<ID3D11Texture2D> m_StagingTexture;
	std::deque<ComPtr<ID3D11Texture2D>> m_ReplayDeque;
	std::thread m_Thread;
	std::thread m_SaveThread;
	std::mutex m_Mutex;
	std::condition_variable m_CV;
	std::atomic<bool> m_FPSEnabled = true;
	GameState m_IsGamePaused = MENU;
	DUPLICATIONMANAGER m_DuplicationManager;

	ComPtr<ID3D11Device> m_Device;
	ComPtr<ID3D11DeviceContext> m_DeviceContext;
	ComPtr<IDXGISwapChain> m_swapChain;
	ComPtr<ID3D11RenderTargetView> m_RenderTargetView;
	ComPtr<ID3D11ShaderResourceView> m_ShaderResourceView;
	ComPtr<ID3D11SamplerState> m_SamplerState;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	ComPtr<ID3D11InputLayout> m_InputLayout;
	ComPtr<ID3D11Texture2D> m_Texture;
	ComPtr<ID3D11Buffer> m_VertexBuffer;

	HWND m_hWnd;
	void DuplicationLoop();
	void CreateShader();
	void CreateQuad();
};

#endif