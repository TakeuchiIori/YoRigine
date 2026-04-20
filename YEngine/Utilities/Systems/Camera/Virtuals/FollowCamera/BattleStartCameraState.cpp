#include "BattleStartCameraState.h"
#include "FollowCamera.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// ステート開始時
// ============================================================
void BattleStartCameraState::Enter(FollowCamera* camera) {
	// ------------------------------------------------------------
	// 制御点の準備とターゲットの追従反映
	// ------------------------------------------------------------
	if (GetControlPoints().empty()) {
		RebuildControlPoints(camera);
	}
	else {
		ApplyTargetOffset(camera);
	}
	CinematicCameraState::Enter(camera);
}

// ============================================================
// メンバ変数の値から制御点を再構築
// ============================================================
void BattleStartCameraState::RebuildControlPoints(FollowCamera* camera) {
	ClearControlPoints();

	Vector3 targetPos = { 0.0f, 0.0f, 0.0f };
	if (camera && camera->GetTarget()) {
		targetPos = camera->GetTarget()->translate_;
	}
	lastTargetPos_ = targetPos;

	SetReturnInterpTime(returnInterpTime_);
	SetLookAtTarget(lookAtTargetFlag_);

	// ------------------------------------------------------------
	// 各ポイントを追加
	// ------------------------------------------------------------
	for (int i = 0; i < kPointCount; ++i) {
		AddControlPoint({
			targetPos + offsets_[i],
			rotations_[i],
			fovs_[i],
			controlPointTimes_[i]
			});
	}
}

// ============================================================
// デフォルトパラメータへのリセット
// ============================================================
void BattleStartCameraState::SetupDefaultControlPoints(FollowCamera* camera) {
	offsets_[0] = { 0.0f, 15.0f, -20.0f };
	offsets_[1] = { 10.0f,  5.0f,  -5.0f };
	offsets_[2] = { -2.0f,  4.0f, -12.0f };
	rotations_[0] = { 0.6f,  0.0f, 0.0f };
	rotations_[1] = { 0.2f, -0.8f, 0.0f };
	rotations_[2] = { 0.1f,  0.0f, 0.0f };
	fovs_[0] = 0.5f;  fovs_[1] = 0.45f; fovs_[2] = 0.45f;
	controlPointTimes_[0] = 0.6f;
	controlPointTimes_[1] = 0.5f;
	controlPointTimes_[2] = 0.6f;
	returnInterpTime_ = 0.5f;
	lookAtTargetFlag_ = true;
	RebuildControlPoints(camera);
}

// ============================================================
// ターゲットの移動分を反映
// ============================================================
void BattleStartCameraState::ApplyTargetOffset(FollowCamera* camera) {
	if (!camera || !camera->GetTarget()) return;

	Vector3 targetPos = camera->GetTarget()->translate_;
	Vector3 delta = targetPos - lastTargetPos_;

	for (auto& pt : GetControlPoints()) {
		pt.position = pt.position + delta;
	}
	lastTargetPos_ = targetPos;
}

// ============================================================
// 保存
// ============================================================
void BattleStartCameraState::Save(nlohmann::json& j) const {
	CinematicCameraState::Save(j);

	j["lookAtTargetFlag"] = lookAtTargetFlag_;
	j["returnInterpTime"] = returnInterpTime_;

	for (int i = 0; i < kPointCount; ++i) {
		j["offsets"][i] = { offsets_[i].x,   offsets_[i].y,   offsets_[i].z };
		j["rotations"][i] = { rotations_[i].x, rotations_[i].y, rotations_[i].z };
		j["fovs"][i] = fovs_[i];
		j["controlPointTimes"][i] = controlPointTimes_[i];
	}
}

// ============================================================
// 読み込み
// ============================================================
void BattleStartCameraState::Load(const nlohmann::json& j) {
	CinematicCameraState::Load(j);

	lookAtTargetFlag_ = j.value("lookAtTargetFlag", true);
	returnInterpTime_ = j.value("returnInterpTime", 0.5f);

	for (int i = 0; i < kPointCount; ++i) {
		if (j.contains("offsets") && static_cast<int>(j["offsets"].size()) > i) {
			offsets_[i] = { j["offsets"][i][0], j["offsets"][i][1], j["offsets"][i][2] };
		}
		if (j.contains("rotations") && static_cast<int>(j["rotations"].size()) > i) {
			rotations_[i] = { j["rotations"][i][0], j["rotations"][i][1], j["rotations"][i][2] };
		}
		if (j.contains("fovs") && static_cast<int>(j["fovs"].size()) > i) {
			fovs_[i] = j["fovs"][i];
		}
		if (j.contains("controlPointTimes") && static_cast<int>(j["controlPointTimes"].size()) > i) {
			controlPointTimes_[i] = j["controlPointTimes"][i];
		}
	}
}

// ============================================================
// エディタ用GUI描画
// ============================================================
void BattleStartCameraState::DrawEditGui() {
#ifdef USE_IMGUI
	ImGui::Text("戦闘開始カメラ設定");
	ImGui::Separator();

	if (ImGui::Checkbox("ターゲットを常に見る", &lookAtTargetFlag_)) {
		SetLookAtTarget(lookAtTargetFlag_);
	}
	if (ImGui::DragFloat("復帰補間時間", &returnInterpTime_, 0.01f, 0.1f, 5.0f)) {
		SetReturnInterpTime(returnInterpTime_);
	}

	ImGui::Separator();
	ImGui::Text("制御点（ターゲットからの相対座標）");

	const char* labels[] = { "制御点1: 俯瞰", "制御点2: 回り込み", "制御点3: 後方" };

	// ------------------------------------------------------------
	// 各ポイントのパラメータ調整
	// ------------------------------------------------------------
	for (int i = 0; i < kPointCount; ++i) {
		ImGui::PushID(i);
		if (ImGui::TreeNode(labels[i])) {
			ImGui::DragFloat3("オフセット", &offsets_[i].x, 0.1f, -100.0f, 100.0f);
			ImGui::DragFloat3("回転", &rotations_[i].x, 0.01f, -6.28f, 6.28f);
			ImGui::DragFloat("FOV", &fovs_[i], 0.005f, 0.1f, 1.5f);
			ImGui::DragFloat("到達時間(秒)", &controlPointTimes_[i], 0.01f, 0.05f, 10.0f);
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	ImGui::Separator();
	if (ImGui::Button("デフォルト設定にリセット")) {
		SetupDefaultControlPoints(nullptr);
	}
#endif
}