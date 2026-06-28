#pragma once

// モデル共通クラス
namespace YoRigine {
	class DirectXCommon;
}
class ModelCommon
{
public:
	///************************* 基本関数 *************************///

	// インスタンス取得
	static ModelCommon* GetInstance();

	// 初期化
	void Initialize(YoRigine::DirectXCommon* dxCommon);

public:
	///************************* アクセッサ *************************///

	// DirectX共通取得
	YoRigine::DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	///************************* コンストラクタ禁止 *************************///

	ModelCommon() = default;
	~ModelCommon() = default;
	ModelCommon(const ModelCommon&) = delete;
	ModelCommon& operator=(const ModelCommon&) = delete;

private:
	///************************* メンバ変数 *************************///

	// DirectX共通クラス
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
};
