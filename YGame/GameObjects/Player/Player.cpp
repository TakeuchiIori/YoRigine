#include "Player.h"

// App
#include "Combo/ComboTypes.h"
#include "Guard/PlayerGuard.h"

// Engine
#include "Systems/GameTime/GameTime.h"
#include <Debugger/Logger.h>
#include "Collision/AreaCollision/Base/AreaManager.h"

#ifdef USE_IMGUI
#include "imgui.h" 
#endif // _DEBUG
#include <Systems/Audio/Audio.h>

// ============================================================
// デストラクタ
// ============================================================
Player::~Player() {
	//obbCollider_->~OBBCollider();
}

// ============================================================
// 初期化
// ============================================================
void Player::Initialize(Camera* camera) {
	camera_ = camera;

	// オブジェクト初期化
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	obj_->SetModel("Player.gltf", true, "Idle4");
	input_ = YoRigine::Input::GetInstance();

	// ワールドトランスフォーム初期化
	wt_.Initialize();
	wt_.useAnchorPoint_ = true;

	AreaManager::GetInstance()->RegisterObject(&wt_, "Player");

	// モデルのスケルトンに親ボーンを設定
	obj_->GetModel()->GetSkeleton()->SetRootParent(&wt_);

	// 武器初期化（剣・盾）
	playerSword_ = std::make_unique<PlayerSword>();
	playerSword_->SetPlayer(this);
	playerSword_->SetObject(obj_.get());
	playerSword_->Initialize(camera_);

	playerShield_ = std::make_unique<PlayerShield>();
	playerShield_->SetPlayer(this);
	playerShield_->SetObject(obj_.get());
	playerShield_->Initialize(camera_);


	healthUI_ = std::make_unique<PlayerHealthBarUI>(this);
	healthUI_->Initialize();

	//------------------------------------------------------------
	// ステート・戦闘・コリジョン等の初期化
	//------------------------------------------------------------
	InitStates();
	InitCombatSystem();
	combat_->GetCombo()->RecoverCC(combat_->GetMaxCC());

	boneLine_ = std::make_unique<Line>();
	boneLine_->Initialize();
	boneLine_->SetCamera(camera_);

	InitCollision();
	InitJson();

	if (obj_ && obj_->GetModel()) {
		auto* ms = obj_->GetModel()->GetMotionSystem();
		auto* skeleton = obj_->GetModel()->GetSkeleton();
		if (ms && skeleton) {
			// ここで確実に上半身のマスクをセットする
			ms->SetUpperBodyMask(skeleton->GetDescendantBones("mixamorig:Hips"));
		}
	}
}

// ============================================================
// Stateの初期化
// ============================================================
void Player::InitStates() {
	movement_ = std::make_unique<PlayerMovement>(this);

	// 入力デバイス変更時のコールバック設定
	movement_->SetInputTypeChangeCallback([this](InputType type) {
		switch (type) {
		case InputType::Keyboard: Logger("Input switched to Keyboard\n"); break;
		case InputType::Gamepad:  Logger("Input switched to Controller\n"); break;
		}
		});
}

// ============================================================
// 戦闘システムの初期化
// ============================================================
void Player::InitCombatSystem() {
	combat_ = std::make_unique<PlayerCombat>(this);

	// アクション変更コールバック（デバッグログ）
	combat_->SetActionCallback([this]([[maybe_unused]] const std::string& action) {
		});
}

// ============================================================
// 戦闘用入力の初期化
// ============================================================
void Player::HandleCombatInput() {

	if (playerCamera_ && playerCamera_->IsInPerformance()) return;

	const bool pressedA = input_->IsPadTriggered(0, GamePadButton::A);
	const bool pressedB = input_->IsPadTriggered(0, GamePadButton::B);

	// 攻撃中は AttackingCombatState 側で先行入力をバッファ／消費するため、
	// ここでは入力をバッファに積むだけにする（直接 TryAttack はしない）
	if (combat_->IsAttacking()) {
		if (pressedA) combat_->BufferAttack(AttackType::A_Arte);
		else if (pressedB) combat_->BufferAttack(AttackType::B_Arte);
		return;
	}

	if (!combat_->IsIdle()) return;

	// A（軽攻撃）
	if (pressedA) {
		combat_->TryAttack(AttackType::A_Arte);
	}

	// B（重攻撃）
	if (pressedB) {
		combat_->TryAttack(AttackType::B_Arte);
	}

	// ガード
	if (input_->IsPadTriggered(0, GamePadButton::X)
		|| input_->GetInstance()->TriggerKey(DIK_N)) {
		combat_->TryGuard();
	}

}

// ============================================================
// コライダー初期化
// ============================================================
void Player::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)
	);
	obbCollider_->SetIsStatic(false);
	obbCollider_->SetMass(100.0f);
	// Player は画面外でも常に当たり判定を回す (落下・カメラ越し攻撃などで invariably 必要)
	obbCollider_->SetCheckOutsideCamera(false);

}

// ============================================================
// 更新処理
// ============================================================
void Player::Update() {
	if (YoRigine::GameTime::IsPause()) {
		return;
	}

	// 入力処理
	HandleCombatInput();

	// HPチェック
	if (hp_ <= 0) {
		isAlive_ = false;
	}

	//------------------------------------------------------------
	// 死亡時処理
	//------------------------------------------------------------
	if (!isAlive_ || combat_->GetCurrentState() == CombatState::Dead) {
		UpdateMotionTime();
		combat_->Update(YoRigine::GameTime::GetDeltaTime());
		obj_->UpdateAnimation();
		wt_.UpdateMatrix();
		playerSword_->Update();
		playerShield_->Update();
		return; // 死亡中は処理停止
	}

	//------------------------------------------------------------
	// 生存時処理
	//------------------------------------------------------------
	// 盾のコライダーON/OFF制御
	if (combat_->GetCurrentState() == CombatState::Guarding) {
		playerShield_->SetEnableCollider(true);
	}
	else {
		playerShield_->SetEnableCollider(false);
	}

	UpdateMotionTime();
	Vector3 sp = playerSword_->GetWowldPosition();

	// ステート更新
	movement_->Update(YoRigine::GameTime::GetDeltaTime());
	combat_->Update(YoRigine::GameTime::GetDeltaTime());

	// オブジェクト更新
	obj_->UpdateAnimation();
	wt_.UpdateMatrix();
	playerSword_->Update();
	playerShield_->Update();
	obbCollider_->Update();
	obbCollider_->SetVelocity(movement_->GetVelocity());
	healthUI_->Update();
}

// ============================================================
// アニメーションモデルの描画
// ============================================================
void Player::DrawAnimation() {
	obj_->Draw(camera_, wt_);
}

// ============================================================
// 描画
// ============================================================
void Player::Draw() {
	playerSword_->Draw();
	playerShield_->Draw();
}

// ============================================================
// コライダーの描画
// ============================================================
void Player::DrawCollision() {
	if (isAlive_) {
		playerSword_->DrawCollision();
		playerShield_->DrawCollision();
		obbCollider_->Draw();
	}
}

// ============================================================
// 骨の描画
// ============================================================
void Player::DrawBone(Line& line) {
	if (isAlive_) {
		obj_->DrawBone(line, wt_.GetMatWorld());
	}
}

// ============================================================
// 影の描画
// ============================================================
void Player::DrawShadow() {
	if (isAlive_) {
		obj_->DrawShadow(wt_);
		playerShield_->DrawShadow();
		playerSword_->DrawShadow();
	}
}

// ============================================================
// ImGui描画
// ============================================================
void Player::DrawImGui() {
	movement_->ShowStateDebug();
	combat_->ShowDebugImGui();
}

// ============================================================
// VFXの描画
// ============================================================
void Player::DrawVfx() {
	playerSword_->DrawVfx();
}

// ============================================================
// モーションの再生時間更新
// ============================================================
void Player::UpdateMotionTime() {
	if (motionSpeed_ != preMotionSpeed_) {
		if (obj_->GetModel()) {
			obj_->GetModel()->GetMotionSystem()->SetMotionSpeed(motionSpeed_);
		}
		preMotionSpeed_ = motionSpeed_;
	}
}

// ============================================================
// ワールド座標の取得
// ============================================================
Vector3 Player::GetWorldPosition() {
	return {
		wt_.matWorld_.m[3][0],
		wt_.matWorld_.m[3][1],
		wt_.matWorld_.m[3][2]
	};
}

// ============================================================
// 現在のカメラの回転を取得
// ============================================================
Vector3 Player::GetCameraRotation() const {
	if (playerCamera_) return playerCamera_->GetRotate();
	return Vector3(0.0f, 0.0f, 0.0f);
}

// ============================================================
//	Jsonの初期化
// ============================================================
void Player::InitJson() {
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("Player", "Resources/Json/Objects/Player");
	jsonManager_->SetCategory("Objects");
	jsonManager_->SetSubCategory("Player");

	//------------------------------------------------------------
	// メイン情報
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("メイン情報");
	jsonManager_->Register("位置", &wt_.translate_);
	jsonManager_->Register("回転", &wt_.rotate_);
	jsonManager_->Register("スケール", &wt_.scale_);
	jsonManager_->Register("色", &obj_->GetColor());

	//------------------------------------------------------------
	// UV関連
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("UV関連");
	jsonManager_->Register("アンカーポイントを使用", &wt_.useAnchorPoint_);
	jsonManager_->Register("アンカーポイント", &anchorPoint_);
	jsonManager_->Register("UVスケール", &obj_->uvScale);
	jsonManager_->Register("UV回転", &obj_->uvRotate);
	jsonManager_->Register("UV移動", &obj_->uvTranslate);

	//------------------------------------------------------------
	// ライティング関連
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("ライティング関連");
	auto* lighting = obj_->GetMaterialLighting()->GetRaw();
	jsonManager_->Register("ライティングを有効化", &lighting->enableLighting);
	jsonManager_->Register("スペキュラを有効化", &lighting->enableSpecular);
	jsonManager_->Register("環境光を有効化", &lighting->enableEnvironment);
	jsonManager_->Register("ハーフベクトルを使用", &lighting->isHalfVector);
	jsonManager_->Register("光沢度", &lighting->shininess);
	jsonManager_->Register("環境光係数", &lighting->environmentCoefficient);

	//------------------------------------------------------------
	// モーション・その他設定
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("その他");
	jsonManager_->Register("モーションの再生速度係数", &motionSpeed_);

	jsonManager_->SetTreePrefix("モーション速度");
	jsonManager_->Register("アイドル状態速度", &motionSpeed[0]);
	jsonManager_->Register("アタック状態速度", &motionSpeed[1]);
	jsonManager_->Register("ガード状態速度", &motionSpeed[2]);
	jsonManager_->Register("死亡状態速度", &motionSpeed[3]);

	//------------------------------------------------------------
	// 下層システム登録
	//------------------------------------------------------------
	movement_->InitJson(jsonManager_.get());
	combat_->GetCombo()->InitJson(jsonManager_.get());
	combat_->GetGuard()->InitJson(jsonManager_.get());

	jsonCollider_ = std::make_unique<YoRigine::JsonManager>("PlayerCollider", "Resources/Json/Colliders");
	obbCollider_->InitJson(jsonCollider_.get());
}

// ============================================================
// 衝突開始時の処理
// ============================================================
void Player::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		Vector3 emitPos = wt_.translate_;
	}
}

// ============================================================
// 衝突継続時の処理
// ============================================================
void Player::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		// obj_->SetMaterialColor({ 0.0f,0.0f,0.0f,0.0f });
	}
}

// ============================================================
// 衝突終了時の処理
// ============================================================
void Player::OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		// obj_->SetMaterialColor({ 1.0f,1.0f,1.0f,1.0f });
	}
}

// ============================================================
// 衝突方向ごとの処理
// ============================================================
void Player::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {

}

// ============================================================
// 衝突開始時の処理（方向ヒット）
// ============================================================
void Player::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, HitDirection dir)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {

		//------------------------------------------------------------
		// SE再生
		//------------------------------------------------------------

		// YoRigine::Audio::GetInstance()->PlayOneShot(
		//	"Resources/Audio/.mp4",
		//	0.2f,
		//	YoRigine::SoundCategory::SE
		//);

		//------------------------------------------------------------
		// カメラシェイク（被弾方向で強度を変える）
		//------------------------------------------------------------
		if (playerCamera_) {
			float intensity = (dir == HitDirection::Back) ? 0.6f : 0.4f;
			float duration  = (dir == HitDirection::Back) ? 0.25f : 0.2f;
			playerCamera_->StartShake(intensity, duration);
		}

		// 方向ヒット状態へ遷移
		combat_->SetHitDirection(dir);
		combat_->ChangeState(CombatState::Hit);
	}
}

// ============================================================
// リセット
// ============================================================
void Player::Reset() {
	hp_ = maxHP_;
	isAlive_ = true;
	if (combat_) combat_->Reset();
	if (movement_) {
		movement_->SetCanMove(true);
		movement_->SetCanRotate(true);
	}

	// Idleモーションに戻す
	if (obj_) {
		obj_->SetMotionSpeed(motionSpeed[0]);
		obj_->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");
	}
}

// ============================================================
// ダメージ処理
// ============================================================
void Player::TakeDamage(int damage) {
	if (!isAlive_ || hp_ <= 0) return;

	hp_ -= damage;

	// HPが0以下になったら死亡
	if (hp_ <= 0) {
		hp_ = 0;
		isAlive_ = false;
	}
}

// ============================================================
// 復活処理
// ============================================================
void Player::Revive(int reviveHP) {
	if (isAlive_) return; // すでに生存中なら処理しない

	hp_ = reviveHP;
	isAlive_ = true;

	// ステートをIdleに戻す
	combat_->ChangeState(CombatState::Idle);
	movement_->ChangeState(MovementState::Idle);
	movement_->SetCanMove(true);
	movement_->SetCanRotate(true);

	// アニメーション再生
	obj_->SetMotionSpeed(GetMotionSpeed(0));
	obj_->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");
}

// ============================================================
// 初期値置
// ============================================================
void Player::SetInitialPosition()
{
	LookAtDirection(Vector3(0.0f, 0.0f, 0.0f));
	SetPosition({ 23.4f, 0.0f, 4.4f });
	wt_.UpdateMatrix();
}

// ============================================================
// 攻撃入力の判定
// ============================================================
bool Player::IsAttackPressedA() const { return input_->IsPadTriggered(0, GamePadButton::A); }
bool Player::IsAttackPressedB() const { return input_->IsPadTriggered(0, GamePadButton::B); }

// ============================================================
// 指定方向を向く
// ============================================================
void Player::LookAtDirection(const Vector3& direction)
{
	Vector3 dir = direction - wt_.translate_;
	dir.y = 0.0f; // 水平方向のみ
	dir = Vector3::Normalize(dir);
	float targetYaw = std::atan2f(dir.x, dir.z); // ラジアンで計算
	wt_.rotate_.y = targetYaw;
	if (playerCamera_) {
		Vector3 rot = playerCamera_->GetRotate();
		playerCamera_->SetRotate({ rot.x, targetYaw, rot.z });
	}
}
