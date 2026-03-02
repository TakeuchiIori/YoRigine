#include "CameraDirector.h"
#include <cmath>
#include "Easing.h"

CameraDirector* CameraDirector::GetInstance() {
	static CameraDirector instance;
	return &instance;
}

void CameraDirector::Initialize() {
	cameras_.clear();
	activeCamera_ = nullptr;
	prevCamera_ = nullptr;
	isBlending_ = false;
	blendTimer_ = 0.0f;
}

void CameraDirector::AddCamera(const std::string& name, std::shared_ptr<VirtualCamera> camera) {
	camera->SetName(name);
	cameras_[name] = camera;

	// 最初のカメラならアクティブにする
	if (!activeCamera_) {
		activeCamera_ = camera;
	}
}

std::shared_ptr<VirtualCamera> CameraDirector::GetCamera(const std::string& name) {
	if (cameras_.find(name) != cameras_.end()) return cameras_[name];
	return nullptr;
}

void CameraDirector::SetPriority(const std::string& name, int priority) {
	if (cameras_.count(name)) {
		cameras_[name]->SetPriority(priority);
	}
}

void CameraDirector::Update(float deltaTime) {
	// 全ての仮想カメラを個別に更新（追従などは各カメラが計算）
	for (auto& [name, cam] : cameras_) {
		cam->Update();
		cam->UpdateMatrix();
	}

	// 優先度をチェックし、必要なら切り替え（ブレンド）を開始
	RefreshActiveCamera();

	if (!activeCamera_) return;

	// 補間が無効、または強制的にスナップしたい場合
	if (!enableBlending_) {
		isBlending_ = false;
	}

	// 3行列の合成
	if (isBlending_ && prevCamera_) {
		blendTimer_ += deltaTime;
		float t = std::clamp(blendTimer_ / blendDuration_, 0.0f, 1.0f);

		// イージング
		float easeT = Easing::linear(t);

		// 座標・回転・FOVを補間
		Vector3 pos = Lerp(prevCamera_->GetTranslate(), activeCamera_->GetTranslate(), easeT);
		Vector3 rot = Lerp(prevCamera_->GetRotate(), activeCamera_->GetRotate(), easeT);
		float fov = Lerp(prevCamera_->GetFovY(), activeCamera_->GetFovY(), easeT);

		// 補間結果から行列を作成
		Matrix4x4 viewMat = Inverse(MakeAffineMatrix({ 1.0f,1.0f,1.0f }, rot, pos));
		Matrix4x4 projMat = MakePerspectiveFovMatrix(fov, activeCamera_->GetAspectRatio(), activeCamera_->GetNearClip(), activeCamera_->GetFarClip());

		activeCameraPos_ = pos;
		activeCameraRot_ = rot; // 回転も保存
		currentFovY_ = fov;     // FOVも保存
		finalVP_ = Multiply(viewMat, projMat);
		activeCameraPos_ = pos;

		if (t >= 1.0f) isBlending_ = false;
	} else {
		// 通常時（単一カメラ）
		finalVP_ = activeCamera_->GetViewProjectionMatrix();
		activeCameraPos_ = activeCamera_->GetTranslate();
		activeCameraRot_ = activeCamera_->GetRotate();
		currentFovY_ = activeCamera_->GetFovY();
	}
}

// CameraDirector.cpp

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
		// もし前のカメラが全くなければ、ブレンドせずに即座に切り替える
		if (activeCamera_ == nullptr) {
			activeCamera_ = highestCam;
			isBlending_ = false;
		}
		else {
			prevCamera_ = activeCamera_;
			activeCamera_ = highestCam;
			isBlending_ = true;
			blendTimer_ = 0.0f;
		}
	}
}

void CameraDirector::SnapToActiveCamera() {
	// その時点での最高優先度のカメラを特定する
	RefreshActiveCamera();

	// 補間フラグを強制的に折る
	isBlending_ = false;
	prevCamera_ = nullptr;

	// アクティブなカメラがあれば、その情報を最終出力に即座に反映
	if (activeCamera_) {
		activeCamera_->Update(); // 追従などの更新
		activeCamera_->UpdateMatrix(); // 行列生成

		finalVP_ = activeCamera_->GetViewProjectionMatrix();
		activeCameraPos_ = activeCamera_->GetTranslate();
		activeCameraRot_ = activeCamera_->GetRotate();
		currentFovY_ = activeCamera_->GetFovY();
	}
}