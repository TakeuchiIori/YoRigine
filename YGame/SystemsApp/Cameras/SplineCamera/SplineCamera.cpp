#include "SplineCamera.h"
#include "Systems/GameTime/GameTime.h"

// C++
#include <numbers>
#include <string>

/// <summary>
/// 初期化処理
/// </summary>
void SplineCamera::Initialize()
{
	translate_.z = -82.5f;
	InitJson();
	t_ = 0.0f;
	hasCalledFinish_ = false;
}

/// <summary>
/// JSON初期化処理（パラメータ登録）
/// </summary>
void SplineCamera::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("SplineCamera", "Resources/Json/Cameras");
	jsonManager_->SetCategory("Cameras");
	jsonManager_->SetSubCategory("SplineCamera");

	jsonManager_->Register("Rotate", &rotate_);
	jsonManager_->Register("カメラの移動速度", &speed_);
	jsonManager_->Register("制御点", &controlPoints_);
}

/// <summary>
/// 更新処理
/// </summary>
void SplineCamera::Update()
{
	t_ += speed_;
	if (t_ > float(controlPoints_.size() - 1)) {
		t_ = float(controlPoints_.size() - 1);
	}

	// スプライン補間により座標を更新
	translate_ = EvaluateSpline(t_);

	//------------------------------------------------------------
	// ターゲットがある場合は注視方向を計算
	//------------------------------------------------------------
	if (target_) {
		rotate_ = GetEulerAnglesFromToDirection(translate_, target_->translate_);
		matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
	}

	//------------------------------------------------------------
	// カメラ移動完了コールバック呼び出し
	//------------------------------------------------------------
	if (!hasCalledFinish_ && isFinishedMove_ && t_ >= float(controlPoints_.size() - 1.0f)) {
		isFinishedMove_();
		hasCalledFinish_ = true;
	}
}

/// <summary>
/// ターゲット追従処理（オフセット付き）
/// </summary>
void SplineCamera::FollowProsess()
{
	if (target_ == nullptr) {
		return;
	}

	Vector3 offset = offset_;
	Matrix4x4 rotate = MakeRotateMatrixXYZ(rotate_);
	offset = TransformNormal(offset, rotate);

	// このゲームではX、Z軸は無視してY軸中心で追従
	Vector3 targetTranslate = Vector3(0.0f, target_->translate_.y, 0.0f);
	translate_ = targetTranslate + offset;

	matView_ = Inverse(MakeAffineMatrix(scale_, rotate_, translate_));
}

/// <summary>
/// 制御点をもとに可視化オブジェクトを登録
/// </summary>
void SplineCamera::RegisterControlPoints()
{
	if (wt_.size() < controlPoints_.size()) {
		for (size_t i = wt_.size(); i < controlPoints_.size(); ++i) {
			auto wt = std::make_unique<WorldTransform>();
			wt->Initialize();
			wt->translate_ = controlPoints_[i];
			wt_.push_back(std::move(wt));

			auto obj = std::make_unique<Object3d>();
			obj->Initialize();
			obj->SetModel("cube.obj");
			obj->SetMaterialColor({ 1.0f, 0.0f, 1.0f, 1.0f });
			obj_.push_back(std::move(obj));
		}
	}

	// 制御点位置を反映
	for (size_t i = 0; i < std::min(wt_.size(), controlPoints_.size()); ++i) {
		wt_[i]->translate_ = controlPoints_[i];
	}
}

/// <summary>
/// カメラ経路上の位置をスプライン補間で算出
/// </summary>
/// <param name="t">現在の補間パラメータ</param>
/// <returns>補間後の座標</returns>
Vector3 SplineCamera::EvaluateSpline(float t)
{
	if (controlPoints_.size() < 2) return Vector3{};

	int seg = std::clamp(int(t), 0, int(controlPoints_.size() - 2));
	float localT = t - float(seg);

	return Lerp(controlPoints_[seg], controlPoints_[seg + 1], localT);
}

/// <summary>
/// 制御点デバッグ描画
/// </summary>
void SplineCamera::Draw(Camera* camera)
{
	for (size_t i = 0; i < obj_.size(); ++i) {
		wt_[i]->UpdateMatrix();
		obj_[i]->Draw(camera, *wt_[i]);
	}
}
