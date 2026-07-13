#pragma once

// C++
#include <string>
#include <wrl.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <d3d12.h>

// Engine
#include "DirectXCommon.h"
#include "DirectXTex.h"
#include <SrvManager.h>

///************************* テクスチャ管理クラス *************************///
///
/// テクスチャを読み込み・管理する  
/// DirectX12リソースとSRVをまとめて扱い、描画時の参照を簡略化する
///
class TextureManager
{
private:
	///************************* テクスチャデータ構造 *************************///
	struct TextureData {
		DirectX::TexMetadata metadata;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
		uint32_t srvIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

public:
	///************************* 基本関数 *************************///

	static TextureManager* GetInstance();
	TextureManager() = default;
	~TextureManager() = default;

	// 初期化
	void Initialize(YoRigine::DirectXCommon* dxCommon, YoRigine::SrvManager* srvManager);

	// 終了
	void Finalize();

	// テクスチャ読み込み
	void LoadTexture(const std::string& filePath);

	// テクスチャ再読み込み（同じSRVインデックスを保持しGPUリソースだけ差し替える）。
	// 同名ファイルを上書き保存した後のホットリロード用。GPUハンドルは不変なので
	// 既にそのテクスチャを参照している Sprite / ImGui::Image はそのまま最新を映す。
	// 未ロードなら LoadTexture と同じ挙動。
	void ReloadTexture(const std::string& filePath);

public:
	///************************* アクセッサ *************************///

	// ファイルパスからSRVインデックス取得
	uint32_t GetTextureIndexByFilePath(const std::string& filePath);

	// ファイルパスからGPUハンドル取得
	D3D12_GPU_DESCRIPTOR_HANDLE GetsrvHandleGPU(const std::string& filePath);
	D3D12_CPU_DESCRIPTOR_HANDLE GetsrvHandleCPU(const std::string& filePath);

	// 文字列変換
	std::wstring ConvertString(const std::string& str);
	std::string ConvertString(const std::wstring& str);

	// メタデータ取得
	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

	/// <summary>
   /// 指定パスのテクスチャが既にロード済みか確認
   /// TexturePreloader のスキップ判定に使用
   /// </summary>
	bool IsLoaded(const std::string& filePath) const
	{
		return textureDatas.contains(filePath);
	}

	/// <summary>
	/// TexturePreloader から呼び出す GPU アップロード専用メソッド
	/// CPUロード済みの ScratchImage を受け取りGPU側の処理のみ行う
	/// ★ 必ずメインスレッドから呼ぶこと（CommandList を使用するため）
	/// </summary>
	/// <param name="filePath">テクスチャファイルパス（キー）</param>
	/// <param name="mipImages">ミップマップ生成済みの画像データ</param>
	void UploadFromPreloader(const std::string& filePath,
		const DirectX::ScratchImage& mipImages);

public:
	///************************* 内部処理 *************************///

	// テクスチャリソース生成
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);

	// テクスチャデータ転送
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages);

private:
	///************************* メンバ変数 *************************///

	static std::unique_ptr<TextureManager> instance;
	static std::once_flag initInstanceFlag;

	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	std::unordered_map<std::string, TextureData> textureDatas;
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	YoRigine::SrvManager* srvManager_ = nullptr;

	static uint32_t kSRVIndexTop;
};
