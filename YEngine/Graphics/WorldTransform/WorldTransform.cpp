#include "WorldTransform.h"
// Engine
#include "DirectXCommon.h"
#include "Object3D/Object3dCommon.h"

// Math
#include "MathFunc.h"

/// <summary>
/// ワールド変換の初期化
/// </summary>
void WorldTransform::Initialize()
{
	constBuffer_ = Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	constBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));

	useAnchorPoint_ = true;

	// ワールド行列初期化
	matWorld_ = MakeAffineMatrix(scale_, rotate_, translate_);
}

/// <summary>
/// オイラー角で回転を設定
/// </summary>
void WorldTransform::SetRotationEuler(const Vector3& euler)
{
	rotate_ = euler;
	quaternion_ = Normalize(EulerToQuaternion(euler));
	useQuaternion_ = false;
}

/// <summary>
/// クォータニオンで回転を設定
/// </summary>
void WorldTransform::SetRotationQuaternion(const Quaternion& q)
{
	quaternion_ = Normalize(q);
	rotate_ = QuaternionToEuler(q);
	useQuaternion_ = true;
}

/// <summary>
/// 定数バッファへ行列を転送
/// </summary>
void WorldTransform::TransferData()
{
	transformData_->WVP = MakeIdentity4x4();
	transformData_->World = matWorld_;
	transformData_->WorldInverse = TransPose(Inverse(transformData_->World));
}

/// <summary>
/// ワールド行列更新
/// </summary>
void WorldTransform::UpdateMatrix()
{
	Matrix4x4 rotM = useQuaternion_
		? MakeRotateMatrix(quaternion_)
		: MakeRotateMatrixXYZ(rotate_);

	if (useAnchorPoint_) {
		Vector3 offset = useQuaternion_
			? ScaleRotateToAnchor(anchorPoint_, scale_)
			: ScaleRotateToAnchor(anchorPoint_, scale_, rotate_);

		Vector3 anchoredTranslation = translate_ + anchorPoint_ - offset;
		matWorld_ = MakeScaleMatrix(scale_) * rotM * MakeTranslateMatrix(anchoredTranslation);
	} else {
		matWorld_ = MakeScaleMatrix(scale_) * rotM * MakeTranslateMatrix(translate_);
	}

	if (parent_) {
		matWorld_ = matWorld_ * parent_->matWorld_;
	}

	TransferData();
}

/// <summary>
/// アンカーポイント取得
/// </summary>
const Vector3& WorldTransform::GetAnchorPoint() const {
	return anchorPoint_;
}

/// <summary>
/// アンカーポイント設定
/// </summary>
void WorldTransform::SetAnchorPoint(const Vector3& anchorPoint) {
	anchorPoint_ = anchorPoint;
}

/// <summary>
/// スケール＋回転にアンカー位置を変換（オイラー角版）
/// </summary>
Vector3 WorldTransform::ScaleRotateToAnchor(const Vector3& point, const Vector3& scale, const Vector3& rotation)
{
	Matrix4x4 scaleM = MakeScaleMatrix(scale);
	Matrix4x4 rotateM = MakeRotateMatrixXYZ(rotation);
	Matrix4x4 transform = rotateM * scaleM;

	return Transform(point, transform);
}

/// <summary>
/// スケール＋回転にアンカー位置を変換（クォータニオン版）
/// </summary>
Vector3 WorldTransform::ScaleRotateToAnchor(const Vector3& point, const Vector3& scale)
{
	Matrix4x4 scaleM = MakeScaleMatrix(scale);
	Matrix4x4 rotateM = MakeRotateMatrix(quaternion_);
	Matrix4x4 transform = rotateM * scaleM;
	return Transform(point, transform);
}
