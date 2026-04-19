#include "CinematicCameraState.h"
#include "FollowCamera.h"
#include "MathFunc.h"
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// ステート開始時
// ============================================================
void CinematicCameraState::Enter([[maybe_unused]] FollowCamera* camera) {
	stateTimer_ = 0.0f;
	currentPointIndex_ = 0;
	pointTimer_ = 0.0f;
	isFinished_ = false;
	isReturning_ = false;
	returnTimer_ = 0.0f;

	if (controlPoints_.empty()) {
		isFinished_ = true;
	}
}

// ============================================================
// 更新処理
// ============================================================
void CinematicCameraState::Update(FollowCamera* camera) {
	stateTimer_ += 0.016f;

	// ------------------------------------------------------------
	// デフォルト状態に戻る補間処理
	// ------------------------------------------------------------
	if (isReturning_) {
		returnTimer_ += 0.016f;
		float t = std::clamp(returnTimer_ / returnInterpTime_, 0.0f, 1.0f);

		t = 1.0f - (1.0f - t) * (1.0f - t);

		Vector3 defaultPos, defaultRot;
		float defaultFov;
		camera->GetDefaultCameraParams(defaultPos, defaultRot, defaultFov);

		Vector3 currentPos = Lerp(returnStartPos_, defaultPos, t);
		Vector3 currentRot = Lerp(returnStartRot_, defaultRot, t);
		float currentFov = std::lerp(returnStartFov_, defaultFov, t);

		camera->SetTranslate(currentPos);
		camera->SetRotate(currentRot);
		camera->SetFovY(currentFov);

		if (t >= 1.0f) {
			isFinished_ = true;
		}
		return;
	}

	// ------------------------------------------------------------
	// すべての制御点を通過したかどうかの判定
	// ------------------------------------------------------------
	if (currentPointIndex_ >= static_cast<int>(controlPoints_.size())) {
		isReturning_ = true;
		returnTimer_ = 0.0f;
		returnStartPos_ = camera->GetTranslate();
		returnStartRot_ = camera->GetRotate();
		returnStartFov_ = camera->GetFovY();
		return;
	}

	// ------------------------------------------------------------
	// 制御点間の補間処理の実行
	// ------------------------------------------------------------
	InterpolateControlPoints(camera);
}

// ============================================================
// ステート終了時
// ============================================================
void CinematicCameraState::Exit([[maybe_unused]] FollowCamera* camera) {
}

// ============================================================
// 制御点の補間計算
// ============================================================
void CinematicCameraState::InterpolateControlPoints(FollowCamera* camera) {
	if (controlPoints_.empty()) return;

	const CameraControlPoint& currentPoint = controlPoints_[currentPointIndex_];
	pointTimer_ += 0.016f;

	Vector3 startPos, startRot;
	float startFov;

	// ------------------------------------------------------------
	// 開始点の決定
	// ------------------------------------------------------------
	if (currentPointIndex_ == 0) {
		startPos = camera->GetTranslate();
		startRot = camera->GetRotate();
		startFov = camera->GetFovY();
	}
	else {
		const CameraControlPoint& prevPoint = controlPoints_[currentPointIndex_ - 1];
		startPos = prevPoint.position;
		startRot = prevPoint.rotation;
		startFov = prevPoint.fov;
	}

	// ------------------------------------------------------------
	// 補間係数の計算（イーズインアウト）
	// ------------------------------------------------------------
	float t = std::clamp(pointTimer_ / currentPoint.arrivalTime, 0.0f, 1.0f);
	t = t * t * (3.0f - 2.0f * t);

	Vector3 interpolatedPos = Lerp(startPos, currentPoint.position, t);
	Vector3 interpolatedRot = Lerp(startRot, currentPoint.rotation, t);
	float interpolatedFov = std::lerp(startFov, currentPoint.fov, t);

	// ------------------------------------------------------------
	// ターゲットを注視する処理
	// ------------------------------------------------------------
	if (lookAtTarget_ && camera->GetTarget()) {
		Vector3 targetPos = camera->GetTarget()->translate_;
		Vector3 direction = targetPos - interpolatedPos;

		interpolatedRot.y = std::atan2(direction.x, direction.z);

		float horizontalDistance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
		interpolatedRot.x = std::atan2(-direction.y, horizontalDistance);
	}

	camera->SetTranslate(interpolatedPos);
	camera->SetRotate(interpolatedRot);
	camera->SetFovY(interpolatedFov);

	// ------------------------------------------------------------
	// 次の制御点へ移行
	// ------------------------------------------------------------
	if (t >= 1.0f) {
		currentPointIndex_++;
		pointTimer_ = 0.0f;
	}
}

// ============================================================
// 制御点の追加
// ============================================================
void CinematicCameraState::AddControlPoint(const CameraControlPoint& point) {
	controlPoints_.push_back(point);
}

// ============================================================
// 制御点のクリア
// ============================================================
void CinematicCameraState::ClearControlPoints() {
	controlPoints_.clear();
	currentPointIndex_ = 0;
	pointTimer_ = 0.0f;
}

// ============================================================
// 保存
// ============================================================
void CinematicCameraState::Save(nlohmann::json& j) const {
	j["returnInterpTime"] = returnInterpTime_;
	j["lookAtTarget"] = lookAtTarget_;

	nlohmann::json pointsArray = nlohmann::json::array();
	for (const auto& point : controlPoints_) {
		nlohmann::json pointJson;
		pointJson["position"] = { point.position.x, point.position.y, point.position.z };
		pointJson["rotation"] = { point.rotation.x, point.rotation.y, point.rotation.z };
		pointJson["fov"] = point.fov;
		pointJson["arrivalTime"] = point.arrivalTime;
		pointsArray.push_back(pointJson);
	}
	j["controlPoints"] = pointsArray;
}

// ============================================================
// 読み込み
// ============================================================
void CinematicCameraState::Load(const nlohmann::json& j) {
	returnInterpTime_ = j.value("returnInterpTime", 0.5f);
	lookAtTarget_ = j.value("lookAtTarget", false);

	controlPoints_.clear();
	if (j.contains("controlPoints")) {
		for (const auto& pointJson : j["controlPoints"]) {
			CameraControlPoint point;
			if (pointJson.contains("position")) {
				point.position = {
					pointJson["position"][0],
					pointJson["position"][1],
					pointJson["position"][2]
				};
			}
			if (pointJson.contains("rotation")) {
				point.rotation = {
					pointJson["rotation"][0],
					pointJson["rotation"][1],
					pointJson["rotation"][2]
				};
			}
			point.fov = pointJson.value("fov", 0.45f);
			point.arrivalTime = pointJson.value("arrivalTime", 1.0f);
			controlPoints_.push_back(point);
		}
	}
}

// ============================================================
// エディタ設定用GUI描画
// ============================================================
void CinematicCameraState::DrawEditGui() {
#ifdef USE_IMGUI
	ImGui::Text("シネマティックカメラ設定");
	ImGui::Separator();

	ImGui::Checkbox("ターゲットを常に見る", &lookAtTarget_);
	ImGui::DragFloat("復帰時の補間時間", &returnInterpTime_, 0.01f, 0.1f, 5.0f);

	ImGui::Separator();
	ImGui::Text("制御点の編集");

	// ------------------------------------------------------------
	// 制御点の追加・クリア
	// ------------------------------------------------------------
	if (ImGui::Button("制御点を追加")) {
		CameraControlPoint newPoint;
		newPoint.position = { 0.0f, 5.0f, -10.0f };
		newPoint.rotation = { 0.0f, 0.0f, 0.0f };
		newPoint.fov = 0.45f;
		newPoint.arrivalTime = 1.0f;
		controlPoints_.push_back(newPoint);
	}

	ImGui::SameLine();
	if (ImGui::Button("全てクリア")) {
		controlPoints_.clear();
	}

	ImGui::Separator();

	// ------------------------------------------------------------
	// 各制御点の編集と並び替え
	// ------------------------------------------------------------
	for (size_t i = 0; i < controlPoints_.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));

		if (ImGui::TreeNode(("制御点 " + std::to_string(i + 1)).c_str())) {
			CameraControlPoint& point = controlPoints_[i];

			ImGui::DragFloat3("位置", &point.position.x, 0.1f, -100.0f, 100.0f);
			ImGui::DragFloat3("回転", &point.rotation.x, 0.01f, -6.28f, 6.28f);
			ImGui::DragFloat("FOV", &point.fov, 0.01f, 0.1f, 1.5f);
			ImGui::DragFloat("到達時間", &point.arrivalTime, 0.01f, 0.1f, 10.0f);

			if (ImGui::Button("この制御点を削除")) {
				controlPoints_.erase(controlPoints_.begin() + i);
				ImGui::TreePop();
				ImGui::PopID();
				break;
			}

			if (i > 0) {
				ImGui::SameLine();
				if (ImGui::Button("上へ")) {
					std::swap(controlPoints_[i], controlPoints_[i - 1]);
				}
			}
			if (i < controlPoints_.size() - 1) {
				ImGui::SameLine();
				if (ImGui::Button("下へ")) {
					std::swap(controlPoints_[i], controlPoints_[i + 1]);
				}
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	ImGui::Separator();
	ImGui::Text("制御点数: %zu", controlPoints_.size());
#endif
}