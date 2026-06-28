#include "CommandManager.h"

#include <Debugger/Logger.h>
#include "DeviceManager.h"

/// <summary>
/// コマンドキュー・コマンドリスト・フェンス・フレームコンテキストの初期化
/// </summary>
void CommandManager::Initialize(DeviceManager* deviceManager)
{
	// デバイスの保存
	deviceManager_ = deviceManager;

	// コマンドキュー・アロケータ・リスト作成
	CreateCommands();

	// フェンスの作成
	CreateFence();

	// フレームコンテキストの初期化（フレームごとのコマンドアロケータ）
	InitializeFrameContexts();
}

/// <summary>
/// CommandManager の終了処理（フェンス待ち＋イベント破棄）
/// </summary>
void CommandManager::Finalize()
{
	// 全フレームが GPU 側で完了するまで待つ
	WaitForAllFrames();

	// フェンス用イベントの破棄
	if (fenceEvent_ != nullptr) {
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
}

/// <summary>
/// 1フレーム開始処理（アロケータとコマンドリストの準備）
/// </summary>
void CommandManager::BeginFrame(uint32_t frameindex)
{
	// 使用するフレームのインデックスを計算
	currentFrameIndex_ = frameindex % kFrameCount;
	auto& context = frameContexts_[currentFrameIndex_];

	// GPU が前フレームの処理を完了するまで待つ
	context.WaitForGPU(fence_.Get(), fenceEvent_);

	//-----------------------------------------
	// アロケータとコマンドリストのリセット
	//-----------------------------------------

	// コマンドアロケータのリセット
	context.Reset();

	// 初回以外はコマンドリストの Reset を行う
	if (!isFirstFrame_) {

		HRESULT hr = commandList_->Reset(context.commandAllocator.Get(), nullptr);
		if (FAILED(hr)) {
			Logger("CommandManager: Failed to reset command list. It may not have been closed.\n");
		}
		assert(SUCCEEDED(hr));
	} else {
		// 初回フレーム：Reset 済み扱いに移行
		isFirstFrame_ = false;
	}

	context.isProcessing = true;
}

/// <summary>
/// 1フレーム終了処理（フェンスを進めて GPU にシグナル）
/// </summary>
void CommandManager::EndFrame()
{
	auto& context = frameContexts_[currentFrameIndex_];

	// このフレームのフェンス値を設定
	context.fenceValue = ++fenceValue_;

	// GPU にシグナルを送信
	HRESULT hr = commandQueue_->Signal(fence_.Get(), context.fenceValue);
	assert(SUCCEEDED(hr));
	(void)hr;
}

/// <summary>
/// 全フレーム処理が終わるまで待機
/// </summary>
void CommandManager::WaitForAllFrames()
{
	for (auto& context : frameContexts_) {
		context.WaitForGPU(fence_.Get(), fenceEvent_);
	}
}

/// <summary>
/// 現在フレームが終わるまで待機
/// </summary>
void CommandManager::WaitForCurrentFrame()
{
	auto& context = frameContexts_[currentFrameIndex_];
	context.WaitForGPU(fence_.Get(), fenceEvent_);
}

/// <summary>
/// コマンドリストの完全リセット
/// </summary>
void CommandManager::Reset(uint32_t frameindex)
{
	currentFrameIndex_ = frameindex % kFrameCount;
	auto& context = frameContexts_[currentFrameIndex_];

	// GPU の完了待ち
	context.WaitForGPU(fence_.Get(), fenceEvent_);

	// コマンドアロケータのリセット
	context.Reset();

	// コマンドリストのリセット
	HRESULT hr = commandList_->Reset(context.commandAllocator.Get(), nullptr);
	if (FAILED(hr)) {
		Logger("CommandManager: Failed to reset command list. It may not have been closed.\n");
	}
	assert(SUCCEEDED(hr));
}

/// <summary>
/// 次のフレームのあるけーたが使い終わっているかだけ待つ
/// </summary>
/// <param name="backBufferIndex"></param>
void CommandManager::PrepareNextFrame(uint32_t backBufferIndex)
{
	// 次のフレームコンテキスト
	auto& nextContext = frameContexts_[backBufferIndex];

	// そのコンテキストに関連付けられたFenceが完了しているか確認
	if (fence_->GetCompletedValue() < nextContext.fenceValue) {
		// 完了していないなら待機
		fence_->SetEventOnCompletion(nextContext.fenceValue, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}

	// 終ったらバックバッファ用のアロケータをリセット
	nextContext.commandAllocator->Reset();
	commandList_->Reset(nextContext.commandAllocator.Get(), nullptr);
}

// CommandManager.cpp に実装を追加
void CommandManager::WaitFrame(uint32_t frameIndex) {
	uint64_t waitValue = frameFenceValues_[frameIndex];

	// まだGPUがそのFence値に到達していなければ待機
	if (fence_->GetCompletedValue() < waitValue) {
		fence_->SetEventOnCompletion(waitValue, fenceEvent_);
		WaitForSingleObjectEx(fenceEvent_, INFINITE, FALSE);
	}
}

void CommandManager::Signal(uint32_t frameIndex) {
	// 現在の値をコマンドキューに送る
	commandQueue_->Signal(fence_.Get(), nextFenceValue_);
	// このフレームが完了した時の値をメモしておく
	frameFenceValues_[frameIndex] = nextFenceValue_;
	// 次回用の値を増やす
	nextFenceValue_++;
}

/// <summary>
/// コマンドキュー・アロケータ・コマンドリストの生成
/// </summary>
void CommandManager::CreateCommands()
{
	HRESULT hr;

	//-------------------- コマンドキュー作成 --------------------//
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = deviceManager_->GetDevice()->CreateCommandQueue(
		&commandQueueDesc,
		IID_PPV_ARGS(&commandQueue_)
	);
	assert(SUCCEEDED(hr));

	//-------------------- コマンドアロケータ作成（仮） --------------------//
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> tempAllocator;
	hr = deviceManager_->GetDevice()->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&tempAllocator)
	);
	assert(SUCCEEDED(hr));

	//-------------------- コマンドリスト作成 --------------------//
	hr = deviceManager_->GetDevice()->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		tempAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&commandList_)
	);
	assert(SUCCEEDED(hr));
}

/// <summary>
/// フェンスとフェンスイベントの作成
/// </summary>
void CommandManager::CreateFence()
{
	HRESULT hr;

	// フェンス作成
	hr = deviceManager_->GetDevice()->CreateFence(
		fenceValue_,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&fence_)
	);
	assert(SUCCEEDED(hr));

	// GPU シグナルを待機するためのイベント
	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);
}

/// <summary>
/// フレームコンテキストの初期化
/// </summary>
void CommandManager::InitializeFrameContexts()
{
	for (uint32_t i = 0; i < kFrameCount; ++i) {
		frameContexts_[i].Initialize(deviceManager_->GetDevice().Get(), i);
	}
}
