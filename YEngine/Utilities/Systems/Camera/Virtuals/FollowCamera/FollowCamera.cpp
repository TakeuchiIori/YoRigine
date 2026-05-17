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
#include <Collision/Core/CollisionManager.h>
#include <cstdlib>

// ============================================================
// 初期化処理
// ============================================================
void FollowCamera::Initialize() {
	VirtualCamera::Initialize();
	currentScale_ = 1.0f;
	baseFovY_ = fovY_;

	// ------------------------------------------------------------
	// デフォルトステートの設定
	// ------------------------------------------------------------
	currentState_ = std::make_unique<DefaultCameraState>();
	currentState_->Enter(this);

	// ------------------------------------------------------------
	// めり込み防止コンポーネントの初期化と無視設定
	// ------------------------------------------------------------
	collisionResolver_.Initialize();
	collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
	collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield));
	collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon));
	collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy));
	collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kFieldEnemy));
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

	UpdateZoom();
}

// ============================================================
// 入力情報の更新処理
// ============================================================
void FollowCamera::UpdateInput() {
	if (isCloseUp_) return;
	
	UpdateLockOn();

	if (YoRigine::Input::GetInstance()->IsControllerConnected() && !isLockOn_) {
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

			// ------------------------------------------------------------
			// カメラの縦回転を制限する
			// ------------------------------------------------------------
			transform_.rotate.x = std::clamp(transform_.rotate.x, minPitch_, maxPitch_);
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

	// ------------------------------------------------------------
	// レイの始点であり、カメラが回る中心（プレイヤーの頭の高さなど）を設定
	// ------------------------------------------------------------
	Vector3 targetPivot = target_->translate_ + Vector3(0.0f, targetPivot_Height_, 0.0f);

	// ------------------------------------------------------------
	// ★修正箇所：理想のカメラ位置は「足元」ではなく「注視点（頭）」を基準にする！
	// ------------------------------------------------------------
	Vector3 idealCameraPos = targetPivot + rotatedOffset;

	// ------------------------------------------------------------
	// コンポーネントに地形や障害物の回避計算を依頼する
	// ------------------------------------------------------------
	Vector3 safePos = collisionResolver_.Resolve(idealCameraPos, targetPivot);

	// ------------------------------------------------------------
	// シェイクの更新と最終座標の適用
	// ------------------------------------------------------------
	UpdateShake();
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

	// ★修正箇所：ここも「注視点」を基準にする
	Vector3 targetPivot = target_->translate_ + Vector3(0.0f, targetPivot_Height_, 0.0f);
	outPos = targetPivot + offset;
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
		ImGui::DragFloat("見上げ限界 (Min Pitch)", &minPitch_, 0.01f, -1.5f, 0.0f);
		ImGui::DragFloat("見下ろし限界 (Max Pitch)", &maxPitch_, 0.01f, 0.0f, 1.5f);
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
	j["targetPivot_Height"] = targetPivot_Height_;
	j["minPitch"] = minPitch_;
	j["maxPitch"] = maxPitch_;
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
	targetPivot_Height_ = j.value("targetPivot_Height", 1.0f);
	minPitch_ = j.value("minPitch", -1.5f);
	maxPitch_ = j.value("maxPitch", 1.5f);
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

// ============================================================
// カメラシェイク開始
// ============================================================
void FollowCamera::StartShake(float intensity, float duration) {
	shakeIntensity_ = intensity;
	shakeDuration_ = duration;
	shakeTimer_ = 0.0f;
}

// ============================================================
// カメラシェイク更新（減衰するランダム揺れ）
// ============================================================
void FollowCamera::UpdateShake() {
	if (shakeTimer_ < shakeDuration_) {
		shakeTimer_ += YoRigine::GameTime::GetUnscaledDeltaTime();

		// 時間経過で揺れが弱まる減衰係数
		float decay = 1.0f - (shakeTimer_ / shakeDuration_);
		if (decay < 0.0f) decay = 0.0f;

		// -1.0 〜 1.0 のランダム値
		float rx = ((std::rand() % 2001) - 1000) / 1000.0f;
		float ry = ((std::rand() % 2001) - 1000) / 1000.0f;

		shakeOffset_ = {
			rx * shakeIntensity_ * decay,
			ry * shakeIntensity_ * decay,
			0.0f
		};
	}
	else {
		shakeOffset_ = { 0.0f, 0.0f, 0.0f };
	}
}

// ============================================================
// FOVズーム開始
// ============================================================
void FollowCamera::StartZoom(float targetFov, float duration) {
	baseFovY_ = 0.45f; // 必要に応じて固定、あるいは呼び出し時の fovY_ を記憶
	targetZoomFov_ = targetFov;
	zoomDuration_ = duration;
	zoomTimer_ = duration;
}

// ============================================================
// FOVズーム更新
// ============================================================
void FollowCamera::UpdateZoom() {
	if (zoomTimer_ > 0.0f) {
		zoomTimer_ -= YoRigine::GameTime::GetUnscaledDeltaTime();
		
		float t = 1.0f - (zoomTimer_ / zoomDuration_);
		if (t < 0.0f) t = 0.0f;
		if (t > 1.0f) t = 1.0f;
		
		// イージングで滑らかに戻るように (t*t)
		float easeT = t * t; 
		fovY_ = targetZoomFov_ + (baseFovY_ - targetZoomFov_) * easeT;

		if (zoomTimer_ <= 0.0f) {
			zoomTimer_ = 0.0f;
			fovY_ = baseFovY_;
		}
	} else {
		fovY_ = baseFovY_;
	}
}

// ============================================================
// マルチロックオン更新
// ============================================================
void FollowCamera::UpdateLockOn() {
	auto input = YoRigine::Input::GetInstance();
	
	// R3押し込みでロックオン切り替え
	if (input->IsPadTriggered(0, GamePadButton::R_Stick)) {
		isLockOn_ = !isLockOn_;
		if (isLockOn_) {
			lockedTarget_ = nullptr;
			SwitchLockOnTarget(0); // 0は「一番近い敵」を探す
			if (!lockedTarget_) {
				isLockOn_ = false; // 敵がいなければキャンセル
			}
		} else {
			lockedTarget_ = nullptr;
		}
	}

	if (!isLockOn_ || !lockedTarget_ || !target_) {
		isLockOn_ = false;
		lockedTarget_ = nullptr;
		return;
	}

	// ターゲットが有効か確認
	bool isTargetValid = false;
	const auto& colliders = YoRigine::CollisionManager::GetInstance()->GetColliders();
	for (auto* col : colliders) {
		if (col == lockedTarget_ && col->GetIsActive()) {
			isTargetValid = true;
			break;
		}
	}
	
	if (!isTargetValid) {
		// 死んだら一番近い敵に切り替える
		lockedTarget_ = nullptr;
		SwitchLockOnTarget(0);
		if (!lockedTarget_) {
			isLockOn_ = false;
			return;
		}
	}

	// スティック左右で切り替え
	if (lockOnSwitchCooldown_ > 0.0f) {
		lockOnSwitchCooldown_ -= YoRigine::GameTime::GetUnscaledDeltaTime();
	} else {
		float rx = input->GetRightStickX(0);
		if (std::abs(rx) > 0.6f) {
			SwitchLockOnTarget(rx > 0.0f ? 1 : -1);
			lockOnSwitchCooldown_ = 0.3f;
		}
	}

	// ------------------------------------------------------------
	// カメラをターゲットに向ける処理
	// ------------------------------------------------------------
	Vector3 targetPivot = target_->translate_ + Vector3(0.0f, targetPivot_Height_, 0.0f);
	Vector3 enemyPos = lockedTarget_->GetCenterPosition();
	
	Vector3 dir = enemyPos - targetPivot;
	dir = Normalize(dir);

	float targetYaw = atan2f(dir.x, dir.z);
	float targetPitch = asinf(-dir.y) + 0.15f; // 少し見下ろす
	targetPitch = std::clamp(targetPitch, minPitch_, maxPitch_);

	// イージングで滑らかに向かせる
	float t = std::clamp(10.0f * YoRigine::GameTime::GetUnscaledDeltaTime(), 0.0f, 1.0f);
	transform_.rotate.y = Lerp(transform_.rotate.y, targetYaw, t);
	transform_.rotate.x = Lerp(transform_.rotate.x, targetPitch, t);
}

// ============================================================
// ロックオンターゲットの切り替え (dir = -1: 左, 1: 右, 0: 一番近い敵)
// ============================================================
void FollowCamera::SwitchLockOnTarget(int direction) {
	if (!target_) return;

	const auto& colliders = YoRigine::CollisionManager::GetInstance()->GetColliders();
	std::vector<BaseCollider*> enemies;

	// アクティブな敵を収集
	for (auto* col : colliders) {
		if (col->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy) && col->GetIsActive()) {
			enemies.push_back(col);
		}
	}

	if (enemies.empty()) {
		lockedTarget_ = nullptr;
		return;
	}

	Vector3 playerPos = target_->translate_;

	if (direction == 0 || !lockedTarget_) {
		// 一番近い敵を探す
		BaseCollider* closest = nullptr;
		float minDist = (std::numeric_limits<float>::max)();
		for (auto* e : enemies) {
			float dist = Length(e->GetCenterPosition() - playerPos);
			if (dist < minDist) {
				minDist = dist;
				closest = e;
			}
		}
		lockedTarget_ = closest;
	} else {
		// 左右の切り替え
		Vector3 currentEnemyPos = lockedTarget_->GetCenterPosition();
		Vector3 playerToCurrent = Normalize(currentEnemyPos - playerPos);
		float currentYaw = atan2f(playerToCurrent.x, playerToCurrent.z);

		BaseCollider* bestCandidate = nullptr;
		float minAngleDiff = (std::numeric_limits<float>::max)();

		for (auto* e : enemies) {
			if (e == lockedTarget_) continue;
			
			Vector3 playerToE = Normalize(e->GetCenterPosition() - playerPos);
			float eYaw = atan2f(playerToE.x, playerToE.z);

			// 角度差を計算 (-π ～ π)
			float diff = eYaw - currentYaw;
			while (diff <= -3.14159265f) diff += 6.2831853f;
			while (diff > 3.14159265f) diff -= 6.2831853f;

			// 右に倒した (direction == 1) なら diff > 0 の中で最小を探す
			// 左に倒した (direction == -1) なら diff < 0 の中で最大を探す（絶対値が最小）
			if (direction == 1 && diff > 0.05f) {
				if (diff < minAngleDiff) {
					minAngleDiff = diff;
					bestCandidate = e;
				}
			} else if (direction == -1 && diff < -0.05f) {
				if (std::abs(diff) < minAngleDiff) {
					minAngleDiff = std::abs(diff);
					bestCandidate = e;
				}
			}
		}

		if (bestCandidate) {
			lockedTarget_ = bestCandidate;
		}
	}
}