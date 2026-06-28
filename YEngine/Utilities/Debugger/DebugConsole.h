#pragma once
#ifdef USE_IMGUI

// C++
#include <string>
#include <chrono>
#include <queue>
#include <numeric>

// デバッグ用コンソールクラス
// FPS・CPU・GPU・メモリ・リソース情報などをリアルタイムで可視化する
class DebugConsole
{
public:
	///************************* シングルトン *************************///

	// インスタンス取得
	static DebugConsole* GetInstance();

public:
	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// 更新
	void Update();

	// 終了処理
	void Finalize();

public:
	///************************* フレーム情報 *************************///

	// フレーム開始（計測リセット）
	void BeginFrame();

	// フレーム終了（計測確定）
	void EndFrame();

public:
	///************************* GPU計測 *************************///

	// ドローコール数を記録
	void RecordDrawCall(uint32_t vertexCount, uint32_t instanceCount = 1);

	// 頂点数を記録
	void RecordVertexCount(uint32_t vertexCount);

	// コンピュートディスパッチ数を記録
	void RecordComputeDispatch();

	// バリア回数を記録
	void RecordResourceBarrier();

	// 読み込まれたテクスチャ数を記録
	void RecordTextureLoad();

public:
	///************************* メモリ情報 *************************///

	// GPUメモリ情報の更新
	void UpdateMemoryInfo();

private:
	///************************* シングルトン管理 *************************///

	DebugConsole() = default;
	~DebugConsole() = default;

	static DebugConsole* instance_;

private:
	///************************* フレーム統計構造 *************************///

	struct FrameStats
	{
		float frameTime = 0.0f;
		float cpuTime = 0.0f;
		float gpuTime = 0.0f;
		uint32_t drawCallCount = 0;
		uint32_t vertexCount = 0;
		uint32_t computeDispatchCount = 0;
		uint32_t barrierCount = 0;
		uint32_t triangleCount = 0;
		uint32_t instanceCount = 0;
	};

	// 現在のフレーム情報
	FrameStats currentFrame_;

private:
	///************************* 計測履歴 *************************///

	static constexpr size_t kHistorySize = 120;  // 約2秒分（60FPS想定）
	std::queue<float> frameTimeHistory_;
	std::queue<float> cpuTimeHistory_;
	std::queue<float> gpuTimeHistory_;

private:
	///************************* タイミング計測 *************************///

	std::chrono::high_resolution_clock::time_point frameStartTime_;
	std::chrono::high_resolution_clock::time_point cpuStartTime_;

private:
	///************************* 平均値とFPS *************************///

	float avgFrameTime_ = 0.0f;
	float avgCpuTime_ = 0.0f;
	float avgGpuTime_ = 0.0f;
	float currentFps_ = 0.0f;

private:
	///************************* メモリ情報 *************************///

	struct MemoryInfo
	{
		size_t dedicatedVideoMemory = 0;
		size_t dedicatedSystemMemory = 0;
		size_t sharedSystemMemory = 0;
		size_t currentUsage = 0;
	} memoryInfo_;

private:
	///************************* リソース統計 *************************///

	uint32_t textureCount_ = 0;
	uint32_t bufferCount_ = 0;
	uint32_t pipelineCount_ = 0;

private:
	///************************* ImGui描画 *************************///

	void DrawDebugWindow();
	void DrawPerformanceTab();
	void DrawMemoryTab();
	void DrawResourceTab();
	void DrawFrameContextTab();

private:
	///************************* 内部処理 *************************///

	// 履歴更新
	void UpdateHistory(std::queue<float>& history, float value);

	// 平均値計算
	float CalculateAverage(const std::queue<float>& history);

	friend class Editor;
};
#endif
