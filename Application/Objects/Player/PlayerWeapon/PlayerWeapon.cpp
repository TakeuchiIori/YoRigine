#include "PlayerWeapon.h"
// Engine
#include "Loaders./Model./ModelManager.h"
#include "Object3D./Object3dCommon.h"



// C++
#include <fstream>
#include <iostream>


// ImGui
#ifdef _DEBUG
#include "imgui.h" 
#endif // _DEBUG
#include <Enemy/Enemy.h>



/// <summary>
/// 初期化
/// </summary>
void PlayerWeapon::Initialize(Camera* camera)
{
	camera_ = camera;
	// object3dの初期化
	weapon_ = std::make_unique<Object3d>();
	weapon_->Initialize();
	weapon_->SetModel("weapon.obj");

	// インプット
	input_ = Input::GetInstance();

	worldTransform_.Initialize();
	worldTransform_.translation_.y = -1.0f;
	worldTransform_.translation_.z = -2.0f;

#pragma region 各モーションの初期化
	// 通常攻撃モーション
	attackMotions_.push_back({
		0.5f, 0.2f, 0.8f, {
			{0.0f, {0, 4, 0}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({1, 0, 0}, 0)},   // 初期状態
			{0.25f, {0, 2, 3}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({1, 0, 0}, 45)}, // 振り下ろし途中
			{0.5f, {0, 0, 3}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({1, 0, 0}, 90)} // 完了
		}
		});
	// ダッシュ攻撃モーション
	attackMotions_.push_back({
		0.5f, 0.2f, 0.8f, {
			{0.0f, {-4.0f, 6.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, 45})},   // スタート
			{0.25f, {0.0f, 0.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, 45})},   // 中央
			{0.5f, {4.0f, -6.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, 45})}    // フィニッシュ
		}
		});

	attackMotions_.push_back({
	0.5f, 0.2f, 0.8f, {
		{0.0f, {4.0f, 6.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, -45})},   // スタート
		{0.25f, {0.0f, 0.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, -45})},   // 中央
		{0.5f, {-4.0f, -6.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, -45})}    // フィニッシュ
		}
		});

	// ジャンプ
	attackMotions_.push_back({
		1.0f, 0.2f, 0.8f, {
		{0.0f, {0.0f, 0.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90 + 360 * 0.0f , 0, 0})},   // スタート
		{0.5f, {0.0f, 0.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90 + 360 * 0.5f, 0, 0})},   // 中央
		{1.0f, {0.0f, 0.0f, 3.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90 + 360 * 1.0f, 0, 0})}    // フィニッシュ
		}
		});

	attackMotions_.push_back({
	0.5f, 0.3f, 1.0f, { // モーションの合計時間・開始時間・終了時間を調整
			// モーション1: 真横に振る動作
			{0.0f,  { 4.0f, 0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({0, 0, -90})},   // 準備動作：少し下げて後ろに構える
			{0.125f, {0.0f, 0.0f,  4.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0, -90})},  // 中間動作：前方に大きく振り抜く
			{0.25f, {-4.0f, 0.0f,  0.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({180, 0, -90})}, // 振り抜き後の位置で止まる

			// モーション2: 逆方向に振り返す動作
			{0.25f,  {-4.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({180, 0, -90})}, // 開始位置：振り抜き後の位置
			{0.375f, { 0.0f, 0.0f, 4.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({90, 0,  -90})}, // 中間動作：逆方向に振り返す
			{0.5f,   { 4.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, MakeRotateAxisAngleQuaternion({0, 0,  -90})}   // 終了動作：元の位置に戻る
		}
		});


#pragma endregion

	InitCollision();
	InitJson();

}


void PlayerWeapon::InitJson()
{
	jsonCollider_ = std::make_unique<JsonManager>("PlayerWeaponCollider", "Resources/Json/Colliders");
	jsonCollider_->SetCategory("Colliders");
	//obbCollider_->InitJson(jsonCollider_.get());
	//aabbCollider_->InitJson(jsonCollider_.get());
	//sphereCollider_->InitJson(jsonCollider_.get());
}

void PlayerWeapon::InitCollision()
{
	// OBB
	/*obbCollider_ = std::make_unique<OBBCollider>();
	obbCollider_->SetCamera(camera_);
	obbCollider_->Initialize();
	obbCollider_->SetTransform(&worldTransform_);

	obbCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon));


	obbCollider_->SetOnEnterCollision([this](BaseCollider* self, BaseCollider* other) {
		this->OnEnterCollision(self, other);
		});
	obbCollider_->SetOnCollision([this](BaseCollider* self, BaseCollider* other) {
		this->OnCollision(self, other);
		});
	obbCollider_->SetOnExitCollision([this](BaseCollider* self, BaseCollider* other) {
		this->OnExitCollision(self, other);
		});*/

	//// AABB
	//aabbCollider_ = std::make_unique<AABBCollider>();
	//aabbCollider_->SetTransform(&worldTransform_);
	//aabbCollider_->SetCamera(camera_);
	//aabbCollider_->Initialize();
	//aabbCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon));
	//aabbCollider_->SetOnEnterCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnEnterCollision(self, other);
	//	});
	//aabbCollider_->SetOnCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnCollision(self, other);
	//	});
	//aabbCollider_->SetOnExitCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnExitCollision(self, other);
	//	});

	//// Sphere
	//sphereCollider_ = std::make_unique<SphereCollider>();
	//sphereCollider_->SetTransform(&worldTransform_);
	//sphereCollider_->SetCamera(camera_);
	//sphereCollider_->Initialize();
	//sphereCollider_->SetTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon));
	//sphereCollider_->SetOnEnterCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnEnterCollision(self, other);
	//	});
	//sphereCollider_->SetOnCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnCollision(self, other);
	//	});
	//sphereCollider_->SetOnExitCollision([this](BaseCollider* self, BaseCollider* other) {
	//	this->OnExitCollision(self, other);
	//	});



}


/// <summary>
/// 更新
/// </summary>
void PlayerWeapon::Update()
{
	effects_.remove_if([](Effect* effect) {
		if (!effect->GetIsAlive()) {
			delete effect;
			return true;
		}
		return false;
		});
	for (Effect* effect : effects_) {
		effect->Update();
	}


	// 全状態の初期化
	InitializeState();

	// 全状態の更新処理
	UpdateState();

	// ワールドトランスフォームの更新
	worldTransform_.UpdateMatrix();

	// コリジョンの更新
	//obbCollider_->Update();
	//aabbCollider_->Update();
	//sphereCollider_->Update();

#ifdef _DEBUG
	//DrawDebugUI();
#endif // _DEBUG


}

/// <summary>
/// 描画
/// </summary>
void PlayerWeapon::Draw(Camera* camera)
{
	for (Effect* effect : effects_) {
		effect->Draw(camera);
	}
	weapon_->Draw(camera,worldTransform_);
}

void PlayerWeapon::DrawCollision()
{
	//obbCollider_->Draw();
	//aabbCollider_->Draw();
	//sphereCollider_->Draw();
}

/// <summary>
/// ImGui
/// </summary>
void PlayerWeapon::DrawDebugUI() {
#ifdef _DEBUG
	if (ImGui::Begin("Weapon Debug")) {
		ImGui::Checkbox("Update", &isUpdate_);
		// 現在の状態
		ImGui::Text("Current State: %s",
			state_ == WeaponState::Idle ? "Idle" :
			state_ == WeaponState::Attacking ? "Attacking" :
			state_ == WeaponState::Dashing ? "Dashing" : "Cooldown");

		// クールダウンの時間
		ImGui::SliderFloat("Cooldown Time", &cooldownTime_, 0.1f, 5.0f, "%.2f");

		// 経過クールダウン時間
		ImGui::Text("Elapsed Cooldown Time: %.2f", elapsedCooldownTime_);

		// 攻撃モーションの進行度
		ImGui::Text("Attack Progress: %.2f", attackProgress_);


		// デバッグ用の状態遷移ボタン
		if (ImGui::Button("Set to Idle")) {
			stateRequest_ = WeaponState::Idle;
		}
		if (ImGui::Button("Set to Attacking")) {
			stateRequest_ = WeaponState::Attacking;
		}
		if (ImGui::Button("Set to Left Swing")) {
			stateRequest_ = WeaponState::LSwing;
		}
		if (ImGui::Button("Set to Right Swing")) {
			stateRequest_ = WeaponState::RSwing;
		}
		if (ImGui::Button("Set to Jump Attacking")) {
			stateRequest_ = WeaponState::JumpAttack;
		}
		if (ImGui::Button("Set to Dashing")) {
			stateRequest_ = WeaponState::Dashing;
		}
		if (ImGui::Button("Set to Cooldown")) {
			stateRequest_ = WeaponState::Cooldown;
		}
	}

	// SRTの変更
	ImGui::SliderFloat3("Position", &worldTransform_.translation_.x, 0.0f, 100.0f, "%.2f");
	ImGui::SliderFloat3("Rotation", &worldTransform_.rotation_.x, 0.0f, 360.0f, "%.2f");
	ImGui::SliderFloat3("Scale", &worldTransform_.scale_.x, 0.0f, 100.0f, "%.2f");

	// 各モーションのパラメータ調整
	for (int i = 0; i < attackMotions_.size(); ++i) {
		ImGui::PushID(i);
		if (ImGui::CollapsingHeader(("Motion " + std::to_string(i)).c_str())) {
			ImGui::Text("Motion Parameters:");
			ImGui::SliderFloat("Duration", &attackMotions_[i].duration, 0.1f, 50.0f, "%.2f");
			ImGui::SliderFloat("Hit Start Time", &attackMotions_[i].hitStartTime, 0.0f, attackMotions_[i].duration, "%.2f");
			ImGui::SliderFloat("Hit End Time", &attackMotions_[i].hitEndTime, attackMotions_[i].hitStartTime, attackMotions_[i].duration);
			if (i == 3) {
				ImGui::Checkbox("Jump Frag", &isJumpAttack_);
			}
			// キーフレームの調整
			for (size_t j = 0; j < attackMotions_[i].srtKeyframes.size(); ++j) {
				std::string label;
				if (j == 0) {
					label = "Start Position";
				}
				else if (j == attackMotions_[i].srtKeyframes.size() - 1) {
					label = "End Position";
				}
				else {
					label = "Mid Position " + std::to_string(j);
				}

				if (ImGui::TreeNode((label + " (Keyframe " + std::to_string(j) + ")").c_str())) {
					ImGui::Text("%s", label.c_str());
					ImGui::SliderFloat("Time", &attackMotions_[i].srtKeyframes[j].time, 0.0f, attackMotions_[i].duration, "%.2f");
					ImGui::SliderFloat3("Position", reinterpret_cast<float*>(&attackMotions_[i].srtKeyframes[j].position), -10.0f, 10.0f);
					ImGui::SliderFloat3("Scale", reinterpret_cast<float*>(&attackMotions_[i].srtKeyframes[j].scale), 0.1f, 5.0f);
					ImGui::SliderFloat3("Rotation", reinterpret_cast<float*>(&attackMotions_[i].srtKeyframes[j].rotation), -360.0f, 360.0f);
					ImGui::TreePop();
				}
			}
		}
		ImGui::PopID();
	}

	ImGui::End();
#endif

}



/// <summary>
/// コンボ可能かのフラグ
/// </summary>
/// <returns></returns>
bool PlayerWeapon::IsComboAvailable() const
{
	return canCombo_ && elapsedComboTime_ <= comboWindow_;
}

/// <summary>
/// 全状態の初期化
/// </summary>
void PlayerWeapon::InitializeState()
{
	if (stateRequest_) {
		// 状態の変更（リクエストの変更）
		state_ = stateRequest_.value();
		switch (state_)
		{
		case PlayerWeapon::WeaponState::Idle:
			InitIdle();
			break;
		case PlayerWeapon::WeaponState::Attacking:
			InitAttack();
			break;
		case PlayerWeapon::WeaponState::LSwing:
			InitLeftHorizontalSwing();
			break;
		case PlayerWeapon::WeaponState::RSwing:
			InitRightHorizontalSwiwng();
			break;
		case PlayerWeapon::WeaponState::JumpAttack:
			InitJumpAttack();
			break;
		case PlayerWeapon::WeaponState::Dashing:
			InitDash();
			break;
		case PlayerWeapon::WeaponState::Cooldown:
			InitCooldown();
			break;
		}
		// リクエストをリセット
		stateRequest_ = nullopt;
	}
}

/// <summary>
/// 待機モーションの初期化
/// </summary>
void PlayerWeapon::InitIdle()
{
	idleTime_ = 0.0f;
	worldTransform_.translation_ = { 0.0f, 0.0f, -2.0f };
	worldTransform_.rotation_ = { 0.0f, 0.0f, 0.0f };
	attackProgress_ = 0.0f;
	// 各状態フラグをFalse
	isJumpAttack_ = false;
	isDashAttack_ = false;
}

/// <summary>
/// 通常攻撃の初期化
/// </summary>
void PlayerWeapon::InitAttack()
{
	attackProgress_ = 0.0f;
	currentMotion_ = &attackMotions_[0];

}


void PlayerWeapon::InitLeftHorizontalSwing()
{
	attackProgress_ = 0.0f;
	currentMotion_ = &attackMotions_[1];
}

void PlayerWeapon::InitRightHorizontalSwiwng()
{
	attackProgress_ = 0.0f;
	currentMotion_ = &attackMotions_[2];
}

void PlayerWeapon::InitJumpAttack()
{
	attackProgress_ = 0.0f;
	currentMotion_ = &attackMotions_[3];
	isJumpAttack_ = true;
}

void PlayerWeapon::InitDash()
{
	attackProgress_ = 0.0f;
	currentMotion_ = &attackMotions_[4];
	isDashAttack_ = true;
	isJumpAttack_ = false;
}


/// <summary>
/// _クールダウンの初期化
/// </summary>
void PlayerWeapon::InitCooldown()
{
	elapsedCooldownTime_ = 0.0f;
}

/// <summary>
/// 全体の各状態の更新処理
/// </summary>
void PlayerWeapon::UpdateState()
{
	const float deltaTime = 0.016f;
	switch (state_)
	{
	case PlayerWeapon::WeaponState::Idle:
		IdleMotion(deltaTime);
		break;
	case PlayerWeapon::WeaponState::Attacking:
		UpdateAttackMotion(deltaTime);
		break;
	case PlayerWeapon::WeaponState::LSwing:
		UpdateLeftHorizontalSwing(deltaTime);
		break;
	case PlayerWeapon::WeaponState::RSwing:
		UpdateRightHorizontalSwiwng(deltaTime);
		break;
	case PlayerWeapon::WeaponState::JumpAttack:
		UpdateJumpAttack(deltaTime);
		break;
	case PlayerWeapon::WeaponState::Dashing:
		UpdateDashMotion(deltaTime);
		break;
	case PlayerWeapon::WeaponState::Cooldown:
		UpdateCooldown(deltaTime);
		break;
	}
}

/// <summary>
/// 待機モーション
/// </summary>
/// <param name="deltaTime"></param>
void PlayerWeapon::IdleMotion(float deltaTime)
{
	// スペースキーまたはAボタンを押されたら攻撃
	if (input_->PushKey(DIK_SPACE) || input_->IsPadTriggered(0, GamePadButton::RB)) {
		stateRequest_ = WeaponState::Attacking;
		return;
	}

	idleTime_ += deltaTime;

	// 垂直方向のぷかぷか運動
	float verticalOffset = sin(idleTime_) * 0.1f; // 振幅0.1のsin波

	// 回転運動
	float rotationSpeed = 1.0f; // 回転速度（度/秒）
	worldTransform_.rotation_.y += rotationSpeed * deltaTime;

	// 更新
	worldTransform_.translation_.y =  verticalOffset; // 上下運動を適用
	//worldTransform_.UpdateMatrix();

}

/// <summary>
/// 通常攻撃
/// </summary>
/// <param name="deltaTime"></param>
void PlayerWeapon::UpdateAttackMotion(float deltaTime)
{
	attackProgress_ += deltaTime / currentMotion_->duration;

	// SRTの補間による更新
	const auto& keyframes = currentMotion_->srtKeyframes;
	for (size_t i = 0; i < keyframes.size() - 1; ++i) {
		if (attackProgress_ >= keyframes[i].time && attackProgress_ <= keyframes[i + 1].time) {
			float t = (attackProgress_ - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
			// 位置の補間
			worldTransform_.translation_ = Lerp(keyframes[i].position, keyframes[i + 1].position, t);
			// 回転の補間
			worldTransform_.rotation_ = QuaternionToEuler(Slerp(keyframes[i].rotation, keyframes[i + 1].rotation, t));
		}
	}

	// コンボ可能タイミングの管理
	if (attackProgress_ >= 0.5f && !canCombo_) {
		canCombo_ = true;
		elapsedComboTime_ = 0.0f;
	}

	// コンボ開始
	if (attackProgress_ >= 0.5f) {
		if (IsComboAvailable() && input_->PushKey(DIK_SPACE) || input_->IsPadTriggered(0, GamePadButton::RB)) {
			stateRequest_ = WeaponState::LSwing;
			currentMotion_ = LSwing_;
			attackProgress_ = 0.0f;
			canCombo_ = false;
		}
		// モーション終了
		else if (attackProgress_ >= 0.7f) {
			stateRequest_ = WeaponState::Cooldown; // クールダウン状態へ移行
		}
	}


}

void PlayerWeapon::UpdateLeftHorizontalSwing(float deltaTime)
{
	attackProgress_ += deltaTime / currentMotion_->duration;

	// 攻撃モーションの補間
	const auto& keyframes = currentMotion_->srtKeyframes;
	for (size_t i = 0; i < keyframes.size() - 1; ++i) {
		if (attackProgress_ >= keyframes[i].time && attackProgress_ <= keyframes[i + 1].time) {
			float t = (attackProgress_ - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
			t = std::clamp(t, 0.0f, 1.0f);
			worldTransform_.translation_ = Lerp(keyframes[i].position, keyframes[i + 1].position, t);
			worldTransform_.rotation_ = QuaternionToEuler(Slerp(keyframes[i].rotation, keyframes[i + 1].rotation, t));
		}
	}

	// コンボ可能タイミングの管理
	if (attackProgress_ >= 0.5f && !canCombo_) {
		canCombo_ = true;
		elapsedComboTime_ = 0.0f;
	}


	// コンボ開始
	if (attackProgress_ >= 0.5f) {
		if (IsComboAvailable() && input_->PushKey(DIK_SPACE) || input_->IsPadTriggered(0, GamePadButton::RB)) {
			stateRequest_ = WeaponState::RSwing;
			currentMotion_ = RSwing_; // ダッシュ攻撃モーション
			attackProgress_ = 0.0f;
			canCombo_ = false;
		}
		// モーション終了
		else if (attackProgress_ >= 0.7f) {
			stateRequest_ = WeaponState::Cooldown; // クールダウン状態へ移行
		}
	}

}

void PlayerWeapon::UpdateRightHorizontalSwiwng(float deltaTime)
{
	attackProgress_ += deltaTime / currentMotion_->duration;

	// 攻撃モーションの補間
	const auto& keyframes = currentMotion_->srtKeyframes;
	for (size_t i = 0; i < keyframes.size() - 1; ++i) {
		if (attackProgress_ >= keyframes[i].time && attackProgress_ <= keyframes[i + 1].time) {
			float t = (attackProgress_ - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
			t = std::clamp(t, 0.0f, 1.0f);
			worldTransform_.translation_ = Lerp(keyframes[i].position, keyframes[i + 1].position, t);
			worldTransform_.rotation_ = QuaternionToEuler(Slerp(keyframes[i].rotation, keyframes[i + 1].rotation, t));
		}
	}

	// コンボ可能タイミングの管理
	if (attackProgress_ >= 0.5f && !canCombo_) {
		canCombo_ = true;
		elapsedComboTime_ = 0.0f;
	}

	// コンボ開始
	if (attackProgress_ >= 0.5f) {
		if (IsComboAvailable() && input_->PushKey(DIK_SPACE) || input_->IsPadTriggered(0, GamePadButton::RB)) {
			stateRequest_ = WeaponState::JumpAttack;
			currentMotion_ = jumpAttack_; // ダッシュ攻撃モーション
			attackProgress_ = 0.0f;
			canCombo_ = false;
		}
		// モーション終了
		else if (attackProgress_ >= 0.7f) {
			stateRequest_ = WeaponState::Cooldown; // クールダウン状態へ移行
		}
	}

}

void PlayerWeapon::UpdateJumpAttack(float deltaTime)
{
	attackProgress_ += deltaTime / currentMotion_->duration;

	// 攻撃モーションの補間
	const auto& keyframes = currentMotion_->srtKeyframes;
	for (size_t i = 0; i < keyframes.size() - 1; ++i) {
		if (attackProgress_ >= keyframes[i].time && attackProgress_ <= keyframes[i + 1].time) {
			float t = (attackProgress_ - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
			t = std::clamp(t, 0.0f, 1.0f);
			worldTransform_.translation_ = Lerp(keyframes[i].position, keyframes[i + 1].position, t);
			worldTransform_.rotation_ = QuaternionToEuler(Slerp(keyframes[i].rotation, keyframes[i + 1].rotation, t));
		}
	}

	if (attackProgress_ >= 0.9f) {
		isJumpAttack_ = false;
	}

	// コンボ可能タイミングの管理
	if (attackProgress_ >= 1.0f && !canCombo_) {
		canCombo_ = true;
		elapsedComboTime_ = 0.0f;
	}

	// コンボ開始
	if (attackProgress_ >= 1.0f) {
		if (IsComboAvailable() && input_->PushKey(DIK_SPACE) || input_->IsPadTriggered(0, GamePadButton::RB)) {
			stateRequest_ = WeaponState::Dashing;
			currentMotion_ = dashMotion_; // ダッシュ攻撃モーション
			attackProgress_ = 0.0f;
			canCombo_ = false;
		}
		// モーション終了
		else if (attackProgress_ >= 1.5f) {
			stateRequest_ = WeaponState::Cooldown; // クールダウン状態へ移行
			
		}
	}
}

void PlayerWeapon::UpdateDashMotion(float deltaTime)
{
	attackProgress_ += deltaTime / currentMotion_->duration;

	// ダッシュ攻撃モーションの補間
	const auto& keyframes = currentMotion_->srtKeyframes;
	for (size_t i = 0; i < keyframes.size() - 1; ++i) {
		if (attackProgress_ >= keyframes[i].time && attackProgress_ <= keyframes[i + 1].time) {
			float t = (attackProgress_ - keyframes[i].time) / (keyframes[i + 1].time - keyframes[i].time);
			t = std::clamp(t, 0.0f, 1.0f);
			worldTransform_.translation_ = Lerp(keyframes[i].position, keyframes[i + 1].position, t);
			worldTransform_.rotation_ = QuaternionToEuler(Slerp(keyframes[i].rotation, keyframes[i + 1].rotation, t));
		}
	}

	if (attackProgress_ >= 0.4f) {
		isDashAttack_ = false;
	}

	// コンボ可能タイミングの管理
	if (attackProgress_ >= 0.5f && !canCombo_) {
		canCombo_ = true;
		elapsedComboTime_ = 0.0f;
	}

	// コンボ開始
	if (attackProgress_ >= 0.5f) {
		if (IsComboAvailable() && input_->PushKey(DIK_SPACE) || input_->IsPadTriggered(0, GamePadButton::RB)) {
			//stateRequest_ = WeaponState::Dashing;
			//currentMotion_ = dashMotion_; // ダッシュ攻撃モーション
			//attackProgress_ = 0.0f;
			//canCombo_ = false;
		}
		// モーション終了
		else if (attackProgress_ >= 0.5f) {
			stateRequest_ = WeaponState::Cooldown; // クールダウン状態へ移行
		}
	}
}

/// <summary>
/// クールダウン
/// </summary>
/// <param name="deltaTime"></param>
void PlayerWeapon::UpdateCooldown(float deltaTime)
{
	// 経過時間を更新
	elapsedCooldownTime_ += deltaTime;

	// クールダウンが終了したら状態をIdleに戻す
	if (elapsedCooldownTime_ >= cooldownTime_) {
		stateRequest_ = WeaponState::Idle; // 武器の状態を待機に戻す
		elapsedCooldownTime_ = 0.0f; // 経過時間をリセット
	}
}

void PlayerWeapon::OnEnterCollision(BaseCollider* self, BaseCollider* other)
{
}

void PlayerWeapon::OnCollision(BaseCollider* self, BaseCollider* other)
{
}

void PlayerWeapon::OnExitCollision(BaseCollider* self, BaseCollider* other)
{
}


