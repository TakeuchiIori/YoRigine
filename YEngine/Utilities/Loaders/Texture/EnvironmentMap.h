#pragma once

// C++
#include <string>

// DirectX
#include <d3d12.h>
#include <wrl.h>

///************************* 環境マップ管理クラス *************************///
///
/// シーン全体で使用する環境マップ（Skybox / IBL / ReflectionMap）を管理する。
/// テクスチャの読み込み・SRVインデックス管理・GPUハンドル取得を一元化。
///
class EnvironmentMap final
{
public:
	///************************* コピー・ムーブ禁止 *************************///
	EnvironmentMap(const EnvironmentMap&) = delete;
	EnvironmentMap& operator=(const EnvironmentMap&) = delete;
	EnvironmentMap(EnvironmentMap&&) = delete;
	EnvironmentMap& operator=(EnvironmentMap&&) = delete;

public:
	///************************* シングルトン *************************///

	// インスタンス取得
	static EnvironmentMap* GetInstance();

public:
	///************************* 環境マップ処理 *************************///

	// 指定パスの環境テクスチャを読み込む
	void LoadEnvironmentTexture(const std::string& filePath);

public:
	///************************* アクセッサ *************************///

	// SRVインデックス取得
	uint32_t GetSrvIndex() const { return srvIndex_; }

	// SRVハンドル取得（GPU用）
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const;

private:
	///************************* メンバ変数 *************************///

	EnvironmentMap() = default;
	~EnvironmentMap() = default;

	std::string filePath_;	// 環境マップのファイルパス
	uint32_t srvIndex_ = UINT32_MAX; // SRV登録インデックス
};
