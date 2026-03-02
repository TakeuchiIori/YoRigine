#include "FollowCamera.h"
#include "MathFunc.h"
#include <Systems/Input/Input.h>
#include <algorithm>
#include <cmath>

// ステート関連のインクルード
#include "CameraState.h"
#include "DefaultCameraState.h"
#include "CameraStatePresetManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Systems/Camera/CameraDirector.h>

void FollowCamera::Initialize() {
	VirtualCamera::Initialize();
	currentScale_ = 1.0f;
	
	// デフォルトステートを設定
	currentState_ = std::make_unique<DefaultCameraState>();
	currentState_->Enter(this);
}

void FollowCamera::Update() {
	if (target_ == nullptr && !targetName_.empty()) {
		target_ = CameraDirector::GetInstance()->FindTarget(targetName_);
	}

	if (!target_) return;
	
	// ステート更新
	if (currentState_) {
		currentState_->Update(this);
		
		// ステートが終了したらデフォルトに戻る
		if (currentState_->IsFinished()) {
			ChangeState(std::make_unique<DefaultCameraState>());
		}
	}
}

void FollowCamera::UpdateInput() {
	if (isCloseUp_) return;

	if (YoRigine::Input::GetInstance()->IsControllerConnected()) {
		XINPUT_STATE joyState;
		if (YoRigine::Input::GetInstance()->GetJoystickState(0, joyState)) {
			Vector3 move{ 0.0f, 0.0f, 0.0f };

			// 左右の入力（Y軸まわりの回転）
			move.y += static_cast<float>(joyState.Gamepad.sThumbRX);
			// 上下の入力（X軸まわりの回転）を追加
			move.x -= static_cast<float>(joyState.Gamepad.sThumbRY);

			// デッドゾーン処理（微小な入力を無視しないと勝手に動く場合があります）
			if (Length(move) > 5000.0f) { // 5000は目安です
				move = Normalize(move) * kRotateSpeed_;
				transform_.rotate += move;
			}
		}
	}
}

void FollowCamera::FollowProcess() {
	if (target_ == nullptr) return;

	// クローズアップ補間
	float targetScale = isCloseUp_ ? closeUpScale_ : 1.0f;
	currentScale_ += (targetScale - currentScale_) * std::clamp(interpSpeed_ * 0.016f, 0.0f, 1.0f);

	// 基本オフセット計算
	Vector3 offset = offset_ * currentScale_;

	// カメラシェイクを加算
	offset += shakeOffset_;

	// 回転行列を考慮
	Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
	offset = TransformNormal(offset, rotateMat);

	// 座標更新
	transform_.translate = target_->translate_ + offset;

	// FOV更新(エフェクト用 + コンボ用)
}

void FollowCamera::ChangeState(std::unique_ptr<CameraState> newState) {
	if (currentState_) {
		currentState_->Exit(this);
	}
	
	currentState_ = std::move(newState);
	
	if (currentState_) {
		currentState_->Enter(this);
	}
}

void FollowCamera::GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const {
	if (!target_) {
		outPos = transform_.translate;
		outRot = transform_.rotate;
		outFov = fovY_;
		return;
	}
	
	// デフォルトの追従位置を計算
	Vector3 offset = offset_ * currentScale_;
	Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
	offset = TransformNormal(offset, rotateMat);
	
	outPos = target_->translate_ + offset;
	outRot = transform_.rotate;
	outFov = fovY_;
}

void FollowCamera::DrawDebugGui() {
#ifdef USE_IMGUI
	ImGui::Text("追従カメラ設定");
	
	// 現在のステート表示
	if (currentState_) {
		ImGui::Text("現在のステート: %s", currentState_->GetStateName());
		
		// ステートの編集UI
		if (ImGui::TreeNode("ステート編集")) {
			currentState_->DrawEditGui();
			
			ImGui::Separator();
			
			// プリセット保存
			static char presetName[64] = "";
			ImGui::InputText("プリセット名", presetName, sizeof(presetName));
			
			if (ImGui::Button("プリセットとして保存")) {
				if (strlen(presetName) > 0) {
					CameraStatePresetManager::GetInstance()->SavePreset(
						presetName, 
						currentState_->GetStateName(), 
						currentState_.get()
					);
				}
			}
			
			ImGui::Separator();
			
			// プリセット読込
			auto presetNames = CameraStatePresetManager::GetInstance()->GetPresetNames();
			if (!presetNames.empty()) {
				static int selectedPreset = 0;
				std::vector<const char*> presetNamePtrs;
				for (const auto& name : presetNames) {
					presetNamePtrs.push_back(name.c_str());
				}
				
				ImGui::Combo("プリセット選択", &selectedPreset, presetNamePtrs.data(), static_cast<int>(presetNamePtrs.size()));
				
				if (ImGui::Button("プリセットを読込")) {
					if (selectedPreset >= 0 && selectedPreset < static_cast<int>(presetNames.size())) {
						auto loadedState = CameraStatePresetManager::GetInstance()->LoadPreset(presetNames[selectedPreset]);
						if (loadedState) {
							ChangeState(std::move(loadedState));
						}
					}
				}
			}
			
			ImGui::TreePop();
		}
	}
	ImGui::Separator();
	
	if (ImGui::TreeNode("追従パラメータ")) {
		ImGui::DragFloat3("オフセット距離", &offset_.x, 0.1f, -100.0f, 100.0f);
		ImGui::DragFloat3("角度", &transform_.rotate.x, 0.01f, -6.28f, 6.28f);
		ImGui::DragFloat("旋回速度 (パッド)", &kRotateSpeed_, 0.01f, 0.0f, 1.0f);
		ImGui::TreePop();
	}

	ImGui::Separator();
	ImGui::Text("演出設定");
	ImGui::Checkbox("クローズアップ有効", &isCloseUp_);
	ImGui::DragFloat("クローズアップ倍率", &closeUpScale_, 0.01f, 0.1f, 1.0f);
	ImGui::DragFloat("補間速度", &interpSpeed_, 0.1f, 0.1f, 20.0f);

	ImGui::Separator();
	ImGui::Text("現在の状態");
	ImGui::Value("現在の拡大率", currentScale_);

	ImGui::Text("シェイク状態");
	ImGui::Value("シェイク強度", shakeIntensity_);
	ImGui::Value("シェイクタイマー", shakeTimer_);
	ImGui::DragFloat3("シェイクオフセット", &shakeOffset_.x, 0.01f);

	if (target_) {
		ImGui::Text("追従対象: セット済み");
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), "追従対象: なし");
	}

#endif
}

void FollowCamera::Save(nlohmann::json& j) const {
	VirtualCamera::Save(j);
	j["targetName"] = targetName_;
	j["offset"] = { offset_.x, offset_.y, offset_.z };
	j["rotate"] = { transform_.rotate.x, transform_.rotate.y, transform_.rotate.z };
	j["interpSpeed"] = interpSpeed_;
	j["rotateSpeed"] = kRotateSpeed_;
	j["closeUpScale"] = closeUpScale_;
}

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
}
