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

/// <summary>
/// 座標変換クラス
/// </summary>
class WorldTransform {
public:
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverse;
	};

	// アンカーポイント
	Vector3 anchorPoint_ = { 0, 0, 0 };
	bool useAnchorPoint_ = false;
	// ローカルスケール
	Vector3 scale_ = { 1, 1, 1 };
	// X,Y,Z軸回りのローカル回転角
	Vector3 rotate_ = { 0, 0, 0 };
	// ローカル座標
	Vector3 translate_ = { 0, 0, 0 };
	// ローカル → ワールド変換行列
	Matrix4x4 matWorld_;
	// 親となるワールド変換へのポインタ
	const WorldTransform* parent_ = nullptr;

	// クォータニオン回転
	Quaternion quaternion_ = { 0,0,0,1 };
	bool useQuaternion_ = false;

	WorldTransform() = default;
	~WorldTransform() = default;
	// コピー禁止
	WorldTransform(WorldTransform&&) = default;
	WorldTransform& operator=(WorldTransform&&) = default;
	WorldTransform(const WorldTransform&) = delete;
	WorldTransform& operator=(const WorldTransform&) = delete;

	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 行列を更新
	/// </summary>
	void UpdateMatrix();

	/// <summary>
	/// アンカーポイントの設定・取得
	/// </summary>
	/// <returns></returns>
	const Vector3& GetAnchorPoint() const;
	void SetAnchorPoint(const Vector3& anchorPoint);

	/// <summary>
	/// 定数バッファの取得
	/// </summary>
	/// <returns>定数バッファ</returns>
	const Microsoft::WRL::ComPtr<ID3D12Resource>& GetConstBuffer() const { return constBuffer_; }

	/// <summary>
	/// マップの設定・取得
	/// </summary>
	/// <param name="wvp">WVP行列</param>
	TransformationMatrix* GetTransformData() { return transformData_; }
	void SetMapWVP(const Matrix4x4& wvp) { transformData_->WVP = wvp; }
	void SetMapWorld(const Matrix4x4& world) { transformData_->World = world; }
	const Matrix4x4& GetMatWorld() { return matWorld_; }


	void SetRotationEuler(const Vector3& euler);
	void SetRotationQuaternion(const Quaternion& q);
	Quaternion GetRotationQuaternion() const { return quaternion_; }
	Vector3    GetRotationEuler() const {
		return useQuaternion_ ? QuaternionToEuler(quaternion_) : rotate_;
	};

private:

	// 定数バッファ生成
	void TransferData();

	// スケール・回転を適用した座標を計算
	Vector3 ScaleRotateToAnchor(const Vector3& point, const Vector3& scale, const Vector3& rotation);
	Vector3 ScaleRotateToAnchor(const Vector3& point, const Vector3& scale);
private:
	// 定数バッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
	// マッピング済みアドレス
	TransformationMatrix* transformData_ = nullptr;
};

static_assert(!std::is_copy_assignable_v<WorldTransform>);
