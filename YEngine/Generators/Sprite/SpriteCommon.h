#pragma once
#include "DirectXCommon.h"
#include <memory>
#include <mutex>

/// <summary>
/// スプライトのパイプライン設定クラス
/// </summary>
class SpriteCommon
{
public:
	///************************* 基本関数 *************************///
	static SpriteCommon* GetInstance();
	SpriteCommon() = default;
	~SpriteCommon() = default;
	void Initialize(YoRigine::DirectXCommon* dxCommon);
	void DrawPreference();

	// コマンドリストにセット
	void SetRootSignature();
	void SetGraphicsCommand();
	void SetPrimitiveTopology();

public:
	///************************* アクセッサ *************************///
	YoRigine::DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	// シングルトンインスタンス
	static std::unique_ptr<SpriteCommon> instance;
	static std::once_flag initInstanceFlag;
	SpriteCommon(SpriteCommon&) = delete;
	SpriteCommon& operator=(const SpriteCommon&) = delete;

	///************************* メンバ変数 *************************///
	YoRigine::DirectXCommon* dxCommon_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;

};
