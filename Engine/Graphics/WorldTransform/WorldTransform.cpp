#include "WorldTransform.h"
// Engine
#include "DX./DirectXCommon.h"
#include "Object3D/Object3dCommon.h"

// Math
#include "MathFunc.h"

void WorldTransform::Initialize()
{
	useAnchorPoint_ = false;

	// ワールド行列の初期化
	matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);

	// 定数バッファ生成
	CreateConstBuffer();
}

void WorldTransform::CreateConstBuffer()
{
	// MVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	constBuffer_ = Object3dCommon::GetInstance()->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
	// 書き込むためのアドレスを取得
	constBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
	// 単位行列を書き込んでおく
	transformData_->WVP = MakeIdentity4x4();
	transformData_->World = MakeIdentity4x4();
	transformData_->WorldInverse = TransPose(Inverse(transformData_->World));
}

void WorldTransform::UpdateMatrix()
{
	/// アンカーポイントが指定されている場合
	// スケール・回転後のアンカーポイントの位置
	// アンカーポイント補正後の平行移動
	// 固定された行列合成順序でワールド行列作成
	/// else
	// 固定された行列合成順序でワールド行列作成
	if (useAnchorPoint_) {
		Vector3 offset = ScaleRotateToAnchor(anchorPoint_, scale_, rotation_);

		Vector3 anchoredTranslation = translation_ + anchorPoint_ - offset;

		matWorld_ = MakeAffineMatrix(scale_, rotation_, anchoredTranslation);

	} else {
		
		matWorld_ = MakeAffineMatrix(scale_, rotation_, translation_);
	}


	// 親の合成
	if (transformData_ != nullptr) {
		if (parent_) {
			matWorld_ = matWorld_ * parent_->matWorld_;
		}

		transformData_->World = matWorld_;
		transformData_->WorldInverse = Inverse(matWorld_);
	}
}


/// <summary>
/// アンカーポイントの取得
/// </summary>
const Vector3& WorldTransform::GetAnchorPoint() const {
	return anchorPoint_;
}

/// <summary>
/// アンカーポイントの設定
/// </summary>
void WorldTransform::SetAnchorPoint(const Vector3& anchorPoint) {
	anchorPoint_ = anchorPoint;
}

Vector3 WorldTransform::ScaleRotateToAnchor(const Vector3& point, const Vector3& scale, const Vector3& rotation)
{
	Matrix4x4 scaleM = MakeScaleMatrix(scale);
	Matrix4x4 rotateM = MakeRotateMatrixXYZ(rotation);
	Matrix4x4 transform = rotateM * scaleM;

	return Transform(point, transform);
}

