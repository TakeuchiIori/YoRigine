#include "CameraDirector.h"
#include <cmath>
#include <algorithm> // std::clampに必要
#include "Easing.h"

// ============================================================
// シングルトンインスタンスの取得
// ============================================================
CameraDirector* CameraDirector::GetInstance() {
	static CameraDirector instance;
	return &instance;
}

// ============================================================
// 初期化
// ============================================================
void CameraDirector::Initialize() {
	cameras_.clear();
	activeCamera_ = nullptr;
	prevCamera_ = nullptr;
	isBlending_ = false;
	blendTimer_ = 0.0f;
}

// ============================================================
// カメラの追加登録
// ============================================================
void CameraDirector::AddCamera(const std::string& name, std::shared_ptr<VirtualCamera> camera) {
	camera->SetName(name);
	cameras_[name] = camera;

	// ------------------------------------------------------------
	// 最初のカメラなら自動的にアクティブにする
	// ------------------------------------------------------------
	if (!activeCamera_) {
		activeCamera_ = camera;
	}
}

// ============================================================
// 名前指定でのカメラ取得
// ============================================================
std::shared_ptr<VirtualCamera> CameraDirector::GetCamera(const std::string& name) {
	if (cameras_.find(name) != cameras_.end()) return cameras_[name];
	return nullptr;
}

// ============================================================
// 優先度の変更
// ============================================================
void CameraDirector::SetPriority(const std::string& name, int priority) {
	if (cameras_.count(name)) {
		cameras_[name]->SetPriority(priority);
	}
}

// ============================================================
// 更新処理
// ============================================================
void CameraDirector::Update(float deltaTime) {
	// ------------------------------------------------------------
	// 全ての仮想カメラを個別に更新（追従計算など）
	// ------------------------------------------------------------
	for (auto& [name, cam] : cameras_) {
		cam->Update();
		cam->UpdateMatrix();
	}

	// ------------------------------------------------------------
	// 優先度をチェックし、必要なら切り替え（ブレンド）を開始
	// ------------------------------------------------------------
	RefreshActiveCamera();

	if (!activeCamera_) return;

	// 補間が無効、または強制的にスナップしたい場合はブレンドをキャンセル
	if (!enableBlending_) {
		isBlending_ = false;
	}

	// ------------------------------------------------------------
	// 行列と各種パラメータの合成・出力
	// ------------------------------------------------------------
	if (isBlending_ && prevCamera_) {
		blendTimer_ += deltaTime;
		float t = std::clamp(blendTimer_ / blendDuration_, 0.0f, 1.0f);
		float easeT = Easing::linear(t);

		Vector3 pos = Lerp(prevCamera_->GetTranslate(), activeCamera_->GetTranslate(), easeT);
		Vector3 rot = Lerp(prevCamera_->GetRotate(), activeCamera_->GetRotate(), easeT);
		float fov = Lerp(prevCamera_->GetFovY(), activeCamera_->GetFovY(), easeT);

		Matrix4x4 viewMat = Inverse(MakeAffineMatrix({ 1.0f,1.0f,1.0f }, rot, pos));
		Matrix4x4 projMat = MakePerspectiveFovMatrix(fov, activeCamera_->GetAspectRatio(), activeCamera_->GetNearClip(), activeCamera_->GetFarClip());

		finalVP_ = Multiply(viewMat, projMat);
		activeCameraPos_ = pos;
		activeCameraRot_ = rot;
		currentFovY_ = fov;

		if (t >= 1.0f) isBlending_ = false;
	}
	else {
		// 通常時（単一カメラの情報をそのまま流し込む）
		finalVP_ = activeCamera_->GetViewProjectionMatrix();
		activeCameraPos_ = activeCamera_->GetTranslate();
		activeCameraRot_ = activeCamera_->GetRotate();
		currentFovY_ = activeCamera_->GetFovY();
	}
}

// ============================================================
// 一番優先度が高いカメラを探してアクティブにする処理
// ============================================================
void CameraDirector::RefreshActiveCamera() {
	std::shared_ptr<VirtualCamera> highestCam = nullptr;
	int maxPriority = -1;

	for (auto& [name, cam] : cameras_) {
		if (cam->GetPriority() > maxPriority) {
			maxPriority = cam->GetPriority();
			highestCam = cam;
		}
	}

	if (highestCam && highestCam != activeCamera_) {
		// ------------------------------------------------------------
		// 前のカメラが存在しない場合は即座に切り替え
		// ------------------------------------------------------------
		if (activeCamera_ == nullptr) {
			activeCamera_ = highestCam;
			isBlending_ = false;
		}
		// ------------------------------------------------------------
		// 切り替え発生時に補間を開始する
		// ------------------------------------------------------------
		else {
			prevCamera_ = activeCamera_;
			activeCamera_ = highestCam;
			isBlending_ = true;
			blendTimer_ = 0.0f;
		}
	}
}

// ============================================================
// 補間をスキップし、現在の最高優先度カメラに強制スナップする
// ============================================================
void CameraDirector::SnapToActiveCamera() {
	RefreshActiveCamera();

	isBlending_ = false;
	prevCamera_ = nullptr;

	if (activeCamera_) {
		activeCamera_->Update();
		activeCamera_->UpdateMatrix();

		finalVP_ = activeCamera_->GetViewProjectionMatrix();
		activeCameraPos_ = activeCamera_->GetTranslate();
		activeCameraRot_ = activeCamera_->GetRotate();
		currentFovY_ = activeCamera_->GetFovY();
	}
}