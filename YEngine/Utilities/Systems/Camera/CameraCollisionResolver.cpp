#include "CameraCollisionResolver.h"
#include "Collision/AreaCollision/Base/AreaManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "Systems/GameTime/GameTime.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// 初期化
// ============================================================
void CameraCollisionResolver::Initialize() {
	currentDistanceRatio_ = 1.0f;
	if (blockTypeIDs_.empty()) {
		SetDefaultBlockTypes();
	}
}

// ============================================================
// 遮蔽対象タイプIDの管理
// ============================================================
void CameraCollisionResolver::AddBlockTypeID(uint32_t typeID) {
	if (std::find(blockTypeIDs_.begin(), blockTypeIDs_.end(), typeID) == blockTypeIDs_.end()) {
		blockTypeIDs_.push_back(typeID);
	}
}

void CameraCollisionResolver::SetDefaultBlockTypes() {
	// 既定でカメラを遮るのは「移動を物理的に塞ぐ」型だけ。
	// 壁・建物と NavObstacle(通行不可の障害物)。トリガー領域・飛道弾・敵などは含めない。
	blockTypeIDs_ = {
		static_cast<uint32_t>(CollisionTypeIdDef::kStaticWall),
		static_cast<uint32_t>(CollisionTypeIdDef::kNavObstacle),
	};
}

// ============================================================
// 理想のカメラ座標から地形や障害物のめり込みを計算し、安全な座標を返す
// ============================================================
Vector3 CameraCollisionResolver::Resolve(const Vector3& idealPos, const Vector3& targetPivot,
	float deltaTime, float clearanceScale, bool allowHighAngle) {
	if (!isEnabled_) {
		fadeHits_.clear();
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
		cameraRay.origin = targetPivot;
		cameraRay.direction = rayDir;

		// フェードコライダーはカメラ近距離でも検出できるよう延長距離でレイキャスト。
		// ブロックコライダーは maxDistance 内のヒットのみカメラをずらす。
		const float fadeCheckDist = maxDistance + proximityFadeExtend_;
		fadeHits_.clear();
		const auto hits = YoRigine::CollisionManager::GetInstance()
			->RaycastAllAllowTypes(cameraRay, fadeCheckDist, blockTypeIDs_);
		for (const auto &entry : hits) {
			if (entry.collider->IsCameraFadeMode()) {
				// レイ上の全フェード対象を収集する。家とその奥の木などが重なっても
				// PlayerCamera 側でそれぞれ独立して透明化できる。
				const float depth = std::max(0.0f, maxDistance - entry.hit.distance);
				const float fadeT = (fullyTransparentDepth_ > 0.0f)
					? std::clamp(depth / fullyTransparentDepth_, 0.0f, 1.0f)
					: (depth > 0.0f ? 1.0f : 0.0f);
				fadeHits_.push_back(
					{entry.collider, std::lerp(occludeTargetAlpha_, 0.0f, fadeT)});
			} else if (entry.hit.distance <= maxDistance) {
				// 距離順なので最初の非フェードブロッカーが最も近い。
				hitDistance = entry.hit.distance - cameraRadius_ * clearanceScale;
				break;
			}
		}

		// ------------------------------------------------------------
		// AreaManager を使った見えない壁（闘技場・エリア制限）の判定
		// ------------------------------------------------------------
		AreaManager* areaManager = AreaManager::GetInstance();

		if (!areaManager->IsInsideAreaByPurpose(idealPos, AreaPurpose::Boundary)) {
			// エリア外に出ていたら、境界線上の座標にクランプ（補正）する
			Vector3 clampedPos = areaManager->ClampToNearestArea(idealPos);

			// クランプされた座標までの距離を計算し、カメラ半径分だけさらに手前にする
			float areaDistance = Length(clampedPos - targetPivot) - cameraRadius_ * clearanceScale;

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

	// 最小距離率を確保: ピボットに張り付くと追従ターゲットがカメラに埋まり画面いっぱいになる
	if (targetDistanceRatio < minDistanceRatio_) {
		targetDistanceRatio = minDistanceRatio_;
	}

	// フレームレート非依存の指数減衰補間。
	// 旧仕様は Lerp(a, b, speed) の speed を「60fps の 1 フレームあたり Lerp 係数」として
	// 扱っていたため、fps 変動でカクついていた。speed * 60 を /sec のレートに換算し、
	// dt を使った exponential smoothing に置き換える。
	const float dt = std::max(0.0f, deltaTime);
	const float baseSpeed = (targetDistanceRatio < currentDistanceRatio_) ? avoidSpeed_ : returnSpeed_;
	const float ratePerSec = baseSpeed * 60.0f;
	const float t = 1.0f - std::exp(-ratePerSec * dt);
	currentDistanceRatio_ = Lerp(currentDistanceRatio_, targetDistanceRatio, t);

	Vector3 finalOffset = (idealPos - targetPivot) * currentDistanceRatio_;

	// ------------------------------------------------------------
	// 壁際でのハイアングル（見下ろし）化
	// 注意: rotation 補正を行っていないため Y だけ持ち上げると pivot が画面外に流れる。
	// 既定では off。意図的に有効化したい場合のみ JSON から enableHighAngle=true にする。
	// ------------------------------------------------------------
	if (allowHighAngle && enableHighAngle_ && currentDistanceRatio_ < highAngleThreshold_) {
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
			ImGui::DragFloat("最小距離率", &minDistanceRatio_, 0.01f, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("これ以下にはカメラを target に寄せない (画面いっぱいになる回避)");
			ImGui::DragFloat("回避スピード", &avoidSpeed_, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("復帰スピード", &returnSpeed_, 0.01f, 0.01f, 1.0f);
			ImGui::DragFloat("フェード近距離延長", &proximityFadeExtend_, 0.1f, 0.0f, 5.0f);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("カメラ近くのフェードコライダーを検出するための延長距離。0 にすると近距離フェード無効。");
			ImGui::DragFloat("遮蔽アルファ (通常)", &occludeTargetAlpha_, 0.01f, 0.0f, 1.0f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("壁がカメラを遮っているときの半透明度。0=完全透明, 1=不透明");
			ImGui::DragFloat("完全透明侵入深度", &fullyTransparentDepth_, 0.05f, 0.01f, 3.0f, "%.2f");
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("カメラがこの距離だけ壁に入ったら alpha=0 になる (NieR風)。小さいほど素早く透明になる");

			ImGui::Separator();
			// --- カメラを遮る対象（許可リスト）の設定 ---
			if (ImGui::TreeNode("遮る対象タイプ (許可リスト)")) {
				ImGui::TextDisabled("チェックした型だけがカメラを押し戻す");
				for (CollisionTypeIdDef type : kPlacedObjectColliderTypes) {
					if (type == CollisionTypeIdDef::kNone) continue; // None は判定対象外
					const uint32_t id = static_cast<uint32_t>(type);
					bool blocking = std::find(blockTypeIDs_.begin(), blockTypeIDs_.end(), id) != blockTypeIDs_.end();
					if (ImGui::Checkbox(CollisionTypeIdToString(type), &blocking)) {
						if (blocking) {
							AddBlockTypeID(id);
						} else {
							blockTypeIDs_.erase(std::remove(blockTypeIDs_.begin(), blockTypeIDs_.end(), id), blockTypeIDs_.end());
						}
					}
				}
				if (ImGui::Button("既定に戻す (壁 + NavObstacle)")) {
					SetDefaultBlockTypes();
				}
				ImGui::TreePop();
			}

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
	j["minDistanceRatio"] = minDistanceRatio_;
	j["avoidSpeed"] = avoidSpeed_;
	j["returnSpeed"] = returnSpeed_;
	j["enableHighAngle"] = enableHighAngle_;
	j["highAngleThreshold"] = highAngleThreshold_;
	j["maxPushUpHeight"] = maxPushUpHeight_;
	j["blockTypeIDs"] = blockTypeIDs_;
	j["proximityFadeExtend"]    = proximityFadeExtend_;
	j["occludeTargetAlpha"]     = occludeTargetAlpha_;
	j["fullyTransparentDepth"]  = fullyTransparentDepth_;
}

// ============================================================
// 読み込み
// ============================================================
void CameraCollisionResolver::Load(const nlohmann::json& j) {
	isEnabled_ = j.value("isEnabled", true);
	cameraRadius_ = j.value("cameraRadius", 0.5f);
	minGroundHeight_ = j.value("minGroundHeight", 0.5f);
	minDistanceRatio_ = j.value("minDistanceRatio", 0.15f);
	avoidSpeed_ = j.value("avoidSpeed", 0.3f);
	returnSpeed_ = j.value("returnSpeed", 0.05f);
	// 旧 JSON では既定 true で保存されていた可能性があるため、明示的に false に丸める。
	// 必要なら JSON を手で書き換えて true に戻す。
	enableHighAngle_ = j.value("enableHighAngle", false);
	highAngleThreshold_ = j.value("highAngleThreshold", 0.5f);
	maxPushUpHeight_ = j.value("maxPushUpHeight", 3.0f);

	proximityFadeExtend_   = j.value("proximityFadeExtend",   1.5f);
	occludeTargetAlpha_    = j.value("occludeTargetAlpha",    0.25f);
	fullyTransparentDepth_ = j.value("fullyTransparentDepth", 0.5f);

	// 遮蔽対象タイプID（許可リスト）。旧 JSON には無いので、その場合は既定(壁+NavObstacle)。
	if (j.contains("blockTypeIDs") && j["blockTypeIDs"].is_array()) {
		blockTypeIDs_ = j["blockTypeIDs"].get<std::vector<uint32_t>>();
	} else {
		SetDefaultBlockTypes();
	}
}
