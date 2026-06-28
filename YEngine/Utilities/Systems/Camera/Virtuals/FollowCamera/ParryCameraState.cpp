#include "ParryCameraState.h"
#include "FollowCamera.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// ステート開始時
// ============================================================
void ParryCameraState::Enter(FollowCamera* camera) {
	// ------------------------------------------------------------
	// カメラの制御点を設定し、基底クラスの開始処理を呼ぶ
	// ------------------------------------------------------------
	SetupControlPoints(camera);
	CinematicCameraState::Enter(camera);
}

// ============================================================
// パリィタイプに基づいた制御点の構築
// ============================================================
void ParryCameraState::SetupControlPoints(FollowCamera* camera) {
	ClearControlPoints();

	if (!camera->GetTarget()) return;

	Vector3 targetPos = camera->GetTarget()->translate_;

	switch (parryType_) {
	case ParryType::Quick:
		SetReturnInterpTime(0.3f);
		SetLookAtTarget(true);

		AddControlPoint({
			targetPos + Vector3{3.0f, 2.0f, -5.0f},
			{0.1f, 0.0f, 0.0f},
			0.5f,
			0.2f
			});

		AddControlPoint({
			targetPos + Vector3{-3.0f, 2.0f, -5.0f},
			{0.1f, 0.0f, 0.0f},
			0.5f,
			0.3f
			});
		break;

	case ParryType::Dramatic:
		SetReturnInterpTime(0.5f);
		SetLookAtTarget(true);

		AddControlPoint({
			targetPos + Vector3{0.0f, 8.0f, -8.0f},
			{0.5f, 0.0f, 0.0f},
			0.4f,
			0.4f
			});

		AddControlPoint({
			targetPos + Vector3{6.0f, 3.0f, 0.0f},
			{0.2f, -1.57f, 0.0f},
			0.45f,
			0.5f
			});

		AddControlPoint({
			targetPos + Vector3{0.0f, 3.0f, -10.0f},
			{0.1f, 0.0f, 0.0f},
			0.45f,
			0.4f
			});
		break;

	case ParryType::SlowMotion:
		SetReturnInterpTime(0.6f);
		SetLookAtTarget(true);

		AddControlPoint({
			targetPos + Vector3{2.0f, 2.0f, -6.0f},
			{0.0f, 0.0f, 0.0f},
			0.35f,
			0.8f
			});

		AddControlPoint({
			targetPos + Vector3{1.0f, 1.5f, -3.0f},
			{0.0f, 0.0f, 0.0f},
			0.3f,
			0.6f
			});
		break;
	}
}

// ============================================================
// 保存
// ============================================================
void ParryCameraState::Save(nlohmann::json& j) const {
	CinematicCameraState::Save(j);
	j["parryType"] = static_cast<int>(parryType_);
}

// ============================================================
// 読み込み
// ============================================================
void ParryCameraState::Load(const nlohmann::json& j) {
	CinematicCameraState::Load(j);
	parryType_ = static_cast<ParryType>(j.value("parryType", 0));
}

// ============================================================
// エディタ用GUI描画
// ============================================================
void ParryCameraState::DrawEditGui() {
#ifdef USE_IMGUI
	ImGui::Text("パリィカメラ設定");
	ImGui::Separator();

	const char* typeNames[] = { "Quick", "Dramatic", "SlowMotion" };
	int currentType = static_cast<int>(parryType_);
	if (ImGui::Combo("パリィタイプ", &currentType, typeNames, 3)) {
		parryType_ = static_cast<ParryType>(currentType);
	}

	if (ImGui::Button("制御点を再生成")) {
		SetupControlPoints(nullptr);
	}

	ImGui::Separator();

	// 基底クラスの編集UIを表示
	CinematicCameraState::DrawEditGui();
#endif
}