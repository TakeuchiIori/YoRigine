#include "CameraCollisionResolver.h"
#include "Collision/AreaCollision/Base/AreaManager.h"
#include "Collision/Core/CollisionManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// 初期化
// ============================================================
void CameraCollisionResolver::Initialize() {
	currentDistanceRatio_ = 1.0f;
}

// ============================================================
// 理想のカメラ座標から地形や障害物のめり込みを計算し、安全な座標を返す
// ============================================================
Vector3 CameraCollisionResolver::Resolve(const Vector3& idealPos, const Vector3& targetPivot) {
	if (!isEnabled_) {
		return idealPos;
	}

	Vector3 rayDir = idealPos - targetPivot;
	float maxDistance = Length(rayDir);
	float hitDistance = maxDistance;

	if (maxDistance > 0.001f) {
		rayDir = Normalize(rayDir);

		// ------------------------------------------------------------
		// 物理コライダー（壁、岩、敵など）とのRaycast判定
		// ------------------------------------------------------------
		Ray cameraRay;
		cameraRay.origin - targetPivot;
		cameraRay.direction - rayDir;

		RaycastHit hitInfo;
		// Raycastで衝突した場合、ヒット距離をカメラ半径分だけ手前にする
		if (YoRigine::CollisionManager::GetInstance()->Raycast(cameraRay, maxDistance, &hitInfo, ignoreTypeIDs_)) {
			hitDistance = hitInfo.distance - cameraRadius_;
		}

		// ------------------------------------------------------------
		// AreaManager を使った見えない壁（闘技場・エリア制限）の判定
		// ------------------------------------------------------------
		AreaManager* areaManager = AreaManager::GetInstance();

		if (!areaManager->IsInsideAreaByPurpose(idealPos, AreaPurpose::Boundary)) {
			// エリア外に出ていたら、境界線上の座標にクランプ（補正）する
			Vector3 clampedPos = areaManager->ClampToNearestArea(idealPos);

			// クランプされた座標までの距離を計算し、カメラ半径分だけさらに手前にする
			float areaDistance = Length(clampedPos - targetPivot) - cameraRadius_;

			// 物理コライダー(Raycast)とエリアの壁、より手前にある方を最終的なヒット距離として採用する
			if (areaDistance < hitDistance) {
				hitDistance = areaDistance;
			}
		}

		if (hitDistance < 0.0f) {
			hitDistance = 0.0f;
		}
	}

	// ============================================================
	// 距離の割合計算とスムーズな補間
	// ============================================================
	float targetDistanceRatio = (maxDistance > 0.0f) ? (hitDistance / maxDistance) : 1.0f;

	if (targetDistanceRatio < currentDistanceRatio_) {
		currentDistanceRatio_ = Lerp(currentDistanceRatio_, targetDistanceRatio, avoidSpeed_);
	}
	else {
		currentDistanceRatio_ = Lerp(currentDistanceRatio_, targetDistanceRatio, returnSpeed_);
	}

	Vector3 finalOffset = (idealPos - targetPivot) * currentDistanceRatio_;

	// ------------------------------------------------------------
	// 壁際でのハイアングル（見下ろし）化
	// ------------------------------------------------------------
	if (enableHighAngle_ && currentDistanceRatio_ < highAngleThreshold_) {
		float closeFactor = (highAngleThreshold_ - currentDistanceRatio_) / highAngleThreshold_;
		finalOffset.y += maxPushUpHeight_ * closeFactor;
	}

	Vector3 finalCameraPos = targetPivot + finalOffset;

	// ------------------------------------------------------------
	// 地面（Y軸）のめり込み防止
	// ------------------------------------------------------------
	if (finalCameraPos.y < minGroundHeight_) {
		finalCameraPos.y = minGroundHeight_;
	}

	return finalCameraPos;
}

// ============================================================
// エディタ設定用
// ============================================================
void CameraCollisionResolver::DrawDebugGui() {
#ifdef USE_IMGUI
	if (ImGui::TreeNode("めり込み防止 (Collision Resolver)")) {
		ImGui::Checkbox("コリジョン有効化", &isEnabled_);
		if (isEnabled_) {
			ImGui::DragFloat("カメラ半径 (Radius)", &cameraRadius_, 0.1f, 0.0f, 5.0f);
			ImGui::DragFloat("地面の高さ (Min Y)", &minGroundHeight_, 0.1f, -10.0f, 10.0f);
			ImGui::DragFloat("回避スピード", &avoidSpeed_, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("復帰スピード", &returnSpeed_, 0.01f, 0.01f, 1.0f);

			ImGui::Separator();
			ImGui::Checkbox("壁際ハイアングル化", &enableHighAngle_);
			if (enableHighAngle_) {
				ImGui::DragFloat("発動しきい値", &highAngleThreshold_, 0.05f, 0.1f, 1.0f);
				ImGui::DragFloat("持ち上げ高さ", &maxPushUpHeight_, 0.1f, 0.0f, 10.0f);
			}
		}
		ImGui::TreePop();
	}
#endif
}

// ============================================================
// 保存
// ============================================================
void CameraCollisionResolver::Save(nlohmann::json& j) const {
	j["isEnabled"] = isEnabled_;
	j["cameraRadius"] = cameraRadius_;
	j["minGroundHeight"] = minGroundHeight_;
	j["avoidSpeed"] = avoidSpeed_;
	j["returnSpeed"] = returnSpeed_;
	j["enableHighAngle"] = enableHighAngle_;
	j["highAngleThreshold"] = highAngleThreshold_;
	j["maxPushUpHeight"] = maxPushUpHeight_;
}

// ============================================================
// 読み込み
// ============================================================
void CameraCollisionResolver::Load(const nlohmann::json& j) {
	isEnabled_ = j.value("isEnabled", true);
	cameraRadius_ = j.value("cameraRadius", 0.5f);
	minGroundHeight_ = j.value("minGroundHeight", 0.5f);
	avoidSpeed_ = j.value("avoidSpeed", 0.3f);
	returnSpeed_ = j.value("returnSpeed", 0.05f);
	enableHighAngle_ = j.value("enableHighAngle", true);
	highAngleThreshold_ = j.value("highAngleThreshold", 0.5f);
	maxPushUpHeight_ = j.value("maxPushUpHeight", 3.0f);
}