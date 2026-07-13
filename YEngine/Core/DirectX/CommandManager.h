#pragma once

// Engine
#include "FrameContext.h"

// C++
#include <d3d12.h>
#include <wrl.h>
#include <array>

class DeviceManager;

namespace YoRigine {

/// <summary>
/// コマンド管理クラス
/// </summary>
class CommandManager
{
public:

	// バックバッファ数（ダブルバッファだから2）
	static constexpr uint32_t kFrameCount = 2;

	///************************* 基本関数 *************************///

	/// <summary>
	/// 初期化（コマンドキュー・コマンドリスト・フェンス・フレームコンテキストを生成する）
	/// </summary>
	/// <param name="deviceManager">デバイス生成済みの DeviceManager</param>
	void Initialize(DeviceManager* deviceManager);

	/// <summary>
	/// 終了（全フレームの GPU 完了を待ってからフェンスイベントを破棄する）
	/// </summary>
	void Finalize();

	/// <summary>
	/// フレーム開始（対象フレームの GPU 完了待ち → アロケータ/コマンドリストのリセット）
	/// </summary>
	/// <param name="frameindex">開始するフレームのインデックス（kFrameCount で剰余される）</param>
	void BeginFrame(uint32_t frameindex);

	/// <summary>
	/// フレームの終了（フェンス値を進めて GPU にシグナルを送る）
	/// </summary>
	void EndFrame();

	/// <summary>
	/// 全フレームの完了待機
	/// </summary>
	void WaitForAllFrames();

	/// <summary>
	/// 現在のフレームの完了待機
	/// </summary>
	void WaitForCurrentFrame();

	/// <summary>
	/// コマンドリストの完全リセット（GPU 完了待ち → アロケータ/コマンドリストのリセット）
	/// </summary>
	/// <param name="frameindex">リセット対象のフレームインデックス（kFrameCount で剰余される）</param>
	void Reset(uint32_t frameindex);

	/// <summary>
	/// 次のフレームのアロケータが使い終わっているかだけ待つ
	/// </summary>
	/// <param name="backBufferIndex">次に使用するバックバッファのインデックス</param>
	void PrepareNextFrame(uint32_t backBufferIndex);

	/// <summary>
	/// 指定したフレームインデックスに対応する Fence 値の完了を待つ
	/// </summary>
	/// <param name="frameIndex">待機対象のフレームインデックス</param>
	void WaitFrame(uint32_t frameIndex);

	/// <summary>
	/// 現在のFence値をインクリメントしてSignalを送る
	/// </summary>
	/// <param name="frameIndex">シグナル完了時の Fence 値を記録する対象フレームインデックス</param>
	void Signal(uint32_t frameIndex);

private:
	///************************* 内部処理 *************************///

	/// <summary>
	/// コマンドリストの生成
	/// </summary>
	void CreateCommands();

	/// <summary>
	/// フェンスの生成
	/// </summary>
	void CreateFence();

	/// <summary>
	/// フレームコンテキストの生成
	/// </summary>
	void InitializeFrameContexts();

public:
	///************************* アクセッサ *************************///

	/// <returns>現在のコマンドリスト</returns>
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() { return commandList_; }
	/// <returns>コマンドキュー</returns>
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() { return commandQueue_; }
	/// <returns>現在のフレームに対応するコマンドアロケータ</returns>
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> GetCurrentCommandAllocator()
	{
		return frameContexts_[currentFrameIndex_].commandAllocator;
	}

	/// <returns>フェンス</returns>
	Microsoft::WRL::ComPtr<ID3D12Fence> GetFence() { return fence_; }
	/// <returns>現在のフェンス値</returns>
	uint64_t GetFenceValue() { return fenceValue_; }
	/// <returns>フェンス完了待ち用イベントハンドル</returns>
	HANDLE GetFenceEvent() { return fenceEvent_; }
	/// <returns>現在のフレームインデックス</returns>
	uint32_t GetCurrentFrameIndex() const { return currentFrameIndex_; }

private:
	///************************* メンバ変数 *************************///

	DeviceManager* deviceManager_ = nullptr;

	// コマンドキュー
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	// コマンドリスト
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
	// フェンス
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	uint64_t fenceValue_ = 0;
	HANDLE fenceEvent_ = nullptr;

	// フレームコンテキストの配列
	std::array<FrameContext, kFrameCount> frameContexts_;

	// 現在のフレームインデックス
	uint32_t currentFrameIndex_ = 0;

	// 初回フレームかどうか
	bool isFirstFrame_ = true;

	// 各フレーム（0 or 1）が最後に投げた時のFence値を保持する
	std::array<uint64_t, kFrameCount> frameFenceValues_ = { 0, 0 };
	uint64_t nextFenceValue_ = 1; // 次に発行するFence値
};

} // namespace YoRigine

