#include "FollowCamera.h"
#include "MathFunc.h"
#include <Systems/Input/Input.h>
#include <algorithm>
#include <cmath>

#include "CameraState.h"
#include "DefaultCameraState.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Systems/GameTime/GameTime.h>
#include <Systems/Camera/CameraDirector.h>
#include <Collision/Core/CollisionTypeIdDef.h>

// ============================================================
// 初期化処理
// ============================================================
void FollowCamera::Initialize() {
	VirtualCamera::Initialize();
	currentScale_ = 1.0f;

	// ------------------------------------------------------------
	// デフォルトステートの設定
	// ------------------------------------------------------------
	currentState_ = std::make_unique<DefaultCameraState>();
	currentState_->Enter(this);

	// ------------------------------------------------------------
	// めり込み防止コンポーネントの初期化と無視設定
	// ------------------------------------------------------------
	collisionResolver_.Initialize();
	collisionResolver_.SetIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
}

// ============================================================
// 更新処理
// ============================================================
void FollowCamera::Update() {
	// ------------------------------------------------------------
	// ターゲットの再検索
	// ------------------------------------------------------------
	if (target_ == nullptr && !targetName_.empty()) {
		target_ = CameraDirector::GetInstance()->FindTarget(targetName_);
	}

	if (!target_) return;

	// ------------------------------------------------------------
	// ステートの更新と終了判定
	// ------------------------------------------------------------
	if (currentState_) {
		currentState_->Update(this);

		if (currentState_->IsFinished()) {
			isPreviewMode_ = false;
			ChangeState(std::make_unique<DefaultCameraState>());
		}
	}
}

// ============================================================
// 入力情報の更新処理
// ============================================================
void FollowCamera::UpdateInput() {
	if (isCloseUp_) return;

	if (YoRigine::Input::GetInstance()->IsControllerConnected()) {
		XINPUT_STATE joyState;
		if (YoRigine::Input::GetInstance()->GetJoystickState(0, joyState)) {
			Vector3 move{ 0.0f, 0.0f, 0.0f };

			// ------------------------------------------------------------
			// スティック入力の取得
			// ------------------------------------------------------------
			move.y += static_cast<float>(joyState.Gamepad.sThumbRX);
			move.x -= static_cast<float>(joyState.Gamepad.sThumbRY);

			// ------------------------------------------------------------
			// デッドゾーン処理と回転の適用
			// ------------------------------------------------------------
			if (Length(move) > 5000.0f) {
				move = Normalize(move) * kRotateSpeed_;
				transform_.rotate += move;
			}
		}
	}
}

// ============================================================
// ターゲットへの追従とカメラ座標の計算
// ============================================================
void FollowCamera::FollowProcess() {
	if (target_ == nullptr) {
		return;
	}

	// ------------------------------------------------------------
	// スケール（ズームイン/アウト）の補間計算
	// ------------------------------------------------------------
	float targetScale = isCloseUp_ ? closeUpScale_ : 1.0f;
	currentScale_ += (targetScale - currentScale_) * std::clamp(interpSpeed_ * YoRigine::GameTime::GetDeltaTime(), 0.0f, 1.0f);

	// ------------------------------------------------------------
	// 本来行きたい理想のオフセットと座標を計算
	// ------------------------------------------------------------
	Vector3 offset = offset_ * currentScale_;
	Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
	Vector3 rotatedOffset = TransformNormal(offset, rotateMat);

	Vector3 idealCameraPos = target_->translate_ + rotatedOffset;

	// ------------------------------------------------------------
	// レイの始点となる注視点（プレイヤーの頭の高さなど）を設定
	// ------------------------------------------------------------
	Vector3 targetPivot = target_->translate_ + Vector3(0.0f, targetPivot_Height_, 0.0f);

	// ------------------------------------------------------------
	// コンポーネントに地形や障害物の回避計算を依頼する
	// ------------------------------------------------------------
	Vector3 safePos = collisionResolver_.Resolve(idealCameraPos, targetPivot);

	// ------------------------------------------------------------
	// 最終的な座標の適用（最後にカメラシェイクの揺れを足す）
	// ------------------------------------------------------------
	transform_.translate = safePos + shakeOffset_;
}

// ============================================================
// カメラステートの変更処理
// ============================================================
void FollowCamera::ChangeState(std::unique_ptr<CameraState> newState) {
	if (currentState_) {
		currentState_->Exit(this);
	}

	currentState_ = std::move(newState);

	if (currentState_) {
		currentState_->Enter(this);
	}
}

// ============================================================
// デフォルトのカメラパラメータを取得する処理
// ============================================================
void FollowCamera::GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const {
	if (!target_) {
		outPos = transform_.translate;
		outRot = transform_.rotate;
		outFov = fovY_;
		return;
	}

	Vector3 offset = offset_ * currentScale_;
	Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
	offset = TransformNormal(offset, rotateMat);

	outPos = target_->translate_ + offset;
	outRot = transform_.rotate;
	outFov = fovY_;
}

// ============================================================
// デバッグGUI描画処理
// ============================================================
void FollowCamera::DrawDebugGui() {
#ifdef USE_IMGUI
	ImGui::Text("追従カメラ設定");
	ImGui::Separator();

	// ------------------------------------------------------------
	// 追従パラメータ
	// ------------------------------------------------------------
	if (ImGui::TreeNode("追従パラメータ")) {
		ImGui::DragFloat3("オフセット距離", &offset_.x, 0.1f, -100.0f, 100.0f);
		ImGui::DragFloat3("角度", &transform_.rotate.x, 0.01f, -6.28f, 6.28f);
		ImGui::DragFloat("旋回速度 (パッド)", &kRotateSpeed_, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("注視点の高さ", &targetPivot_Height_, 0.1f, 0.0f, 100.0f);
		ImGui::TreePop();
	}

	ImGui::Separator();

	// ------------------------------------------------------------
	// 演出設定
	// ------------------------------------------------------------
	if (ImGui::TreeNode("演出設定")) {
		ImGui::Checkbox("クローズアップ有効", &isCloseUp_);
		ImGui::DragFloat("クローズアップ倍率", &closeUpScale_, 0.01f, 0.1f, 1.0f);
		ImGui::DragFloat("補間速度", &interpSpeed_, 0.1f, 0.1f, 20.0f);
		ImGui::TreePop();
	}

	ImGui::Separator();

	// ------------------------------------------------------------
	// めり込み防止（Collision Resolver）
	// ------------------------------------------------------------
	collisionResolver_.DrawDebugGui();

	ImGui::Separator();

	// ------------------------------------------------------------
	// 戦闘開始カメラ演出
	// ------------------------------------------------------------
	if (ImGui::TreeNode("戦闘開始カメラ演出")) {

		if (!battleStartState_) {
			battleStartState_ = std::make_shared<BattleStartCameraState>();
		}

		battleStartState_->DrawEditGui();
		ImGui::Separator();

		if (!isPreviewMode_) {
			if (ImGui::Button("プレビュー再生")) {
				auto preview = std::make_unique<BattleStartCameraState>();
				nlohmann::json j;
				battleStartState_->Save(j);
				preview->Load(j);
				preview->RebuildControlPoints(this);
				ChangeState(std::move(preview));
				isPreviewMode_ = true;
			}
		}
		else {
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "再生中");
			ImGui::SameLine();
			if (ImGui::Button("停止")) {
				ChangeState(std::make_unique<DefaultCameraState>());
				isPreviewMode_ = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("最初から")) {
				ChangeState(std::make_unique<DefaultCameraState>());
				auto preview = std::make_unique<BattleStartCameraState>();
				nlohmann::json j;
				battleStartState_->Save(j);
				preview->Load(j);
				preview->RebuildControlPoints(this);
				ChangeState(std::move(preview));
				isPreviewMode_ = true;
			}

			if (currentState_ && currentState_->IsFinished()) {
				isPreviewMode_ = false;
			}
		}

		ImGui::TreePop();
	}

	ImGui::Separator();

	// ------------------------------------------------------------
	// デバッグ情報
	// ------------------------------------------------------------
	ImGui::Text("現在のステート: %s", currentState_ ? currentState_->GetStateName() : "なし");
	if (target_) {
		ImGui::Text("追従対象: セット済み");
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "追従対象: なし");
	}

#endif
}

// ============================================================
// セーブ処理
// ============================================================
void FollowCamera::Save(nlohmann::json& j) const {
	VirtualCamera::Save(j);
	j["targetName"] = targetName_;
	j["offset"] = { offset_.x, offset_.y, offset_.z };
	j["rotate"] = { transform_.rotate.x, transform_.rotate.y, transform_.rotate.z };
	j["interpSpeed"] = interpSpeed_;
	j["rotateSpeed"] = kRotateSpeed_;
	j["closeUpScale"] = closeUpScale_;

	// めり込み防止設定の保存
	nlohmann::json resolverJson;
	collisionResolver_.Save(resolverJson);
	j["collisionResolver"] = resolverJson;

	// BattleStartState の設定保存
	if (battleStartState_) {
		nlohmann::json stateJson;
		battleStartState_->Save(stateJson);
		j["battleStartState"] = stateJson;
	}
}

// ============================================================
// ロード処理
// ============================================================
void FollowCamera::Load(const nlohmann::json& j) {
	VirtualCamera::Load(j);
	targetName_ = j.value("targetName", "");
	if (j.contains("offset")) {
		offset_ = { j["offset"][0], j["offset"][1], j["offset"][2] };
	}
	if (j.contains("rotate")) {
		transform_.rotate = { j["rotate"][0], j["rotate"][1], j["rotate"][2] };
	}
	kRotateSpeed_ = j.value("rotateSpeed", 0.1f);
	interpSpeed_ = j.value("interpSpeed", 5.0f);
	closeUpScale_ = j.value("closeUpScale", 0.3f);

	// めり込み防止設定の復元
	if (j.contains("collisionResolver")) {
		collisionResolver_.Load(j["collisionResolver"]);
	}

	// BattleStartState の設定復元
	if (j.contains("battleStartState")) {
		if (!battleStartState_) {
			battleStartState_ = std::make_shared<BattleStartCameraState>();
		}
		battleStartState_->Load(j["battleStartState"]);
	}
}