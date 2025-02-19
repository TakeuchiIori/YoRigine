#pragma once

// 前方宣言
class DirectXCommon;
// 3Dモデル共通部
class ModelCommon
{
public: // メンバ関数
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize(DirectXCommon* dxCommon);

public: // アクセッサ
	DirectXCommon* GetDxCommon() const { return dxCommon_; }


private: // メンバ関数
	DirectXCommon* dxCommon_;
};

