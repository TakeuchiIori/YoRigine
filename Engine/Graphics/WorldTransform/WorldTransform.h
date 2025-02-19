#pragma once
// C++
#include <d3d12.h>
#include <type_traits>
#include <wrl.h>

// Math
#include "Quaternion.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "MathFunc.h"

class WorldTransform {
public:
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverse;
	};

	struct QuaternionTransform {
		Vector3 scale;
		Quaternion rotate;
		Vector3 translate;
	};

	// ローカルスケール
	Vector3 scale_ = { 1, 1, 1 };
	// X,Y,Z軸回りのローカル回転角
	Vector3 rotation_ = { 0, 0, 0 };
	// ローカル座標
	Vector3 translation_ = { 0, 0, 0 };
	// ローカル → ワールド変換行列
	Matrix4x4 matWorld_;
	// 親となるワールド変換へのポインタ
	const WorldTransform* parent_ = nullptr;

	WorldTransform() = default;
	~WorldTransform() = default;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 定数バッファ生成
	/// </summary>
	void CreateConstBuffer();

	/// <summary>
	/// 行列を転送する
	/// </summary>
	void UpdateMatrix();

	/// <summary>
	/// 定数バッファの取得
	/// </summary>
	/// <returns>定数バッファ</returns>
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetConstBuffer() const { return constBuffer_; }

	/// <summary>
	/// マップのセット
	/// </summary>
	/// <param name="wvp">WVP行列</param>
	TransformationMatrix* GetTransformData() { return transformData_; }
	void SetMapWVP(const Matrix4x4& wvp) { transformData_->WVP = wvp; }

	void SetMapWorld(const Matrix4x4& world) { transformData_->World = world; }
	const Matrix4x4& GetMatWorld() { return matWorld_; }

private:
	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
	// マッピング済みアドレス
	TransformationMatrix* transformData_ = nullptr;
	// コピー禁止
	WorldTransform(const WorldTransform&) = delete;
	WorldTransform& operator=(const WorldTransform&) = delete;
};

static_assert(!std::is_copy_assignable_v<WorldTransform>);