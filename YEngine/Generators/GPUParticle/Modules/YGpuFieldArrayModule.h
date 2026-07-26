#pragma once
// ===========================================================================
// YGpuFieldArrayModule
//
// 「エミッタ1つに対しN個(0〜kMax個)のインスタンス配列をSRVで供給する」という
// GPUパーティクルの環境影響モジュール（ForceField/Noise等）に共通の骨格をまとめた
// テンプレート基底。
//
// 位置づけ: このバッファは常に「同一レイアウトの要素が連続する1本の配列」＝
// それ自体がSoA(Structure of Arrays)の最小単位である。ParticleHot/Warm/Cold
// (YGpuParticle.h)の粒子SoAとは別物だが、「異種データを1つの構造体に混在させず、
// 目的ごとに専用バッファへ分離する」という設計方針は共通しているため、
// 新しいFieldモジュールを追加する際もこの前提を崩さないこと
// （＝1モジュール1バッファ、CPU編集用構造体とGPU転送用構造体を分離する）。
//
// 各Fieldモジュール（ForceField/Noise等）はこれを継承せず、代わりに
// GpuFieldArrayModule<TParams, TGpu, kMax> をメンバとして持ち、
// ToGPU変換関数だけを渡す（継承より合成の方がテンプレート特殊化が少なく済むため）。
// ===========================================================================
#include <DirectXCommon.h>
#include <SrvManager.h>
#include <Vector3.h>
#include <wrl.h>
#include <vector>
#include <functional>
#include <cstring>

template<typename TParams, typename TGpu, uint32_t kMax>
class YGpuFieldArrayModule
{
public:
	// GPUバッファ確保 + SRV登録。YGpuEmitter::Initialize() から1回だけ呼ぶ。
	void CreateBuffer()
	{
		auto* dx = YoRigine::DirectXCommon::GetInstance();
		const size_t bufferSize = sizeof(TGpu) * kMax;

		resource_ = dx->CreateBufferResource(bufferSize);
		resource_->Map(0, nullptr, reinterpret_cast<void**>(&data_));
		std::memset(data_, 0, bufferSize);

		auto* srvManager = YoRigine::SrvManager::GetInstance();
		srvIndex_ = srvManager->Allocate();
		srvManager->CreateSRVforStructuredBuffer(
			srvIndex_,
			resource_.Get(),
			static_cast<UINT>(kMax),
			sizeof(TGpu)
		);
		srvHandle_ = srvManager->GetGPUDescriptorHandle(srvIndex_);
	}

	// fields: CPU側パラメータ配列（isEnable==falseの要素はスキップされ、詰めて転送される）。
	// baseOffset: グループ原点等のワールドオフセット（toGpu内で center 等へ加算するかはモジュール次第）。
	// toGpu: TParams 1個 + baseOffset → TGpu 1個 への変換（centerへのoffset加算など各モジュール固有の処理を含む）。
	void Upload(const std::vector<TParams>& fields, const Vector3& baseOffset,
		const std::function<TGpu(const TParams&, const Vector3&)>& toGpu)
	{
		if (!data_) return;

		uint32_t written = 0;
		for (const auto& src : fields) {
			if (!src.isEnable) continue;
			if (written >= kMax) break;
			data_[written] = toGpu(src, baseOffset);
			written++;
		}
		count_ = written;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const { return srvHandle_; }
	uint32_t GetCount() const { return count_; }
	static constexpr uint32_t GetMax() { return kMax; }

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
	TGpu* data_ = nullptr;
	uint32_t srvIndex_ = 0;
	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle_ = {};
	uint32_t count_ = 0;
};
