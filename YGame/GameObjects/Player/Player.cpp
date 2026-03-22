#include "Player.h"

// App
#include "Combo/ComboTypes.h"
#include "Guard/PlayerGuard.h"

// Engine
#include "Systems/GameTime/GameTime.h"
#include <Debugger/Logger.h>
#include <Particle/ParticleManager.h>
#include "Collision/AreaCollision/Base/AreaManager.h"

#ifdef USE_IMGUI
#include "imgui.h" 
#endif // _DEBUG
#include <Systems/Audio/Audio.h>

/// <summary>
/// デストラクタ
/// </summary>
Player::~Player() {
	obbCollider_->~OBBCollider();
}

/// <summary>
/// プレイヤー初期化処理
/// </summary>
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

	testEmitter_ = std::make_unique<ParticleEmitter>("GuardParticle", wt_.translate_, 10);


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
}

/// <summary>
/// 移動ステート初期化
/// </summary>
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

/// <summary>
/// 戦闘システム初期化
/// </summary>
void Player::InitCombatSystem() {
	combat_ = std::make_unique<PlayerCombat>(this);

	// アクション変更コールバック（デバッグログ）
	combat_->SetActionCallback([this]([[maybe_unused]] const std::string& action) {
		});
}

/// <summary>
/// 戦闘系入力の処理
/// </summary>
void Player::HandleCombatInput() {

	if (followCamera_->IsInPerformance()) return;
	if (!combat_->IsIdle()) return;

	// A（軽攻撃）
	if (input_->IsPadTriggered(0, GamePadButton::A) || input_->GetInstance()->TriggerKey(DIK_Q)) {
		combat_->TryAttack(AttackType::A_Arte);
	}

	// B（重攻撃）
	if (input_->IsPadTriggered(0, GamePadButton::B) || input_->GetInstance()->TriggerKey(DIK_E)) {
		combat_->TryAttack(AttackType::B_Arte);
	}

	// ガード
	if (input_->IsPadTriggered(0, GamePadButton::X)
		|| input_->GetInstance()->IsPressMouse(3)) {
		combat_->TryGuard();
	}

}

/// <summary>
/// コライダー初期化
/// </summary>
void Player::InitCollision() {
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_,
		static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)
	);
}

/// <summary>
/// 毎フレーム更新処理
/// </summary>
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

	//// 剣のコライダーON/OFF制御
	//if (combat_->GetCurrentState() == CombatState::Attacking) {
	//	playerSword_->SetEnableCollider(true);
	//} else {
	//	playerSword_->SetEnableCollider(false);
	//}

	// 盾のコライダーON/OFF制御
	if (combat_->GetCurrentState() == CombatState::Guarding) {
		playerShield_->SetEnableCollider(true);
	} else {
		playerShield_->SetEnableCollider(false);
	}

	UpdateMotionTime();
	Vector3 sp = playerSword_->GetWowldPosition();
	//testEmitter_->FollowEmit(sp, 10);

	// ステート更新
	movement_->Update(YoRigine::GameTime::GetDeltaTime());
	combat_->Update(YoRigine::GameTime::GetDeltaTime());

	// オブジェクト更新
	obj_->UpdateAnimation();
	wt_.UpdateMatrix();
	playerSword_->Update();
	playerShield_->Update();
	obbCollider_->Update();
	healthUI_->Update();
}

/// <summary>
/// モデル描画（アニメーション付き）
/// </summary>
void Player::DrawAnimation() {
	obj_->Draw(camera_, wt_);
}

/// <summary>
/// 装備品描画
/// </summary>
void Player::Draw() {
	playerSword_->Draw();
	playerShield_->Draw();
}

/// <summary>
/// コライダー描画
/// </summary>
void Player::DrawCollision() {
	if (isAlive_) {
		playerSword_->DrawCollision();
		playerShield_->DrawCollision();
		obbCollider_->Draw();
	}
}

/// <summary>
/// 骨構造描画（デバッグ用）
/// </summary>
void Player::DrawBone(Line& line) {
	if (isAlive_) {
		obj_->DrawBone(line, wt_.GetMatWorld());
	}
}

void Player::DrawShadow() {
	if (isAlive_) {
		obj_->DrawShadow(wt_);
		playerShield_->DrawShadow();
		playerSword_->DrawShadow();
	}
}

void Player::DrawImGui() {
	movement_->ShowStateDebug();
	combat_->ShowDebugImGui();
}

/// <summary>
/// モーション速度変更時の更新
/// </summary>
void Player::UpdateMotionTime() {
	if (motionSpeed_ != preMotionSpeed_) {
		if (obj_->GetModel()) {
			obj_->GetModel()->GetMotionSystem()->SetMotionSpeed(motionSpeed_);
		}
		preMotionSpeed_ = motionSpeed_;
	}
}

/// <summary>
/// ワールド座標を取得
/// </summary>
Vector3 Player::GetWorldPosition() {
	return {
		wt_.matWorld_.m[3][0],
		wt_.matWorld_.m[3][1],
		wt_.matWorld_.m[3][2]
	};
}

/// <summary>
/// 現在のカメラ回転を取得
/// </summary>
Vector3 Player::GetCameraRotation() const {
	if (camera_ && followCamera_) {
		return followCamera_->GetRotate();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}

/// <summary>
/// Jsonデータ登録処理
/// </summary>
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

/// <summary>
/// 衝突開始時の処理
/// </summary>
void Player::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		Vector3 emitPos = wt_.translate_;
		emitPos.y += 1.5f; // 少し上にずら
		YoRigine::ParticleManager::GetInstance()->Emit("GuardParticle", emitPos, 20);
	}
}

/// <summary>
/// 衝突中の処理
/// </summary>
void Player::OnCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		// obj_->SetMaterialColor({ 0.0f,0.0f,0.0f,0.0f });
	}
}

/// <summary>
/// 衝突終了時の処理
/// </summary>
void Player::OnExitCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		// obj_->SetMaterialColor({ 1.0f,1.0f,1.0f,1.0f });
	}
}

/// <summary>
/// 衝突方向ごとの処理
/// </summary>
void Player::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {

}

/// <summary>
/// 衝突判定開始時の方向ごとの処理
/// </summary>
void Player::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other, HitDirection dir)
{
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {

		//------------------------------------------------------------
		// SE再生
		//------------------------------------------------------------

		// YoRigine::Audio::GetInstance()->PlayOneShot(
		//	"Resources/Audio/isamikun.mp4",
		//	0.2f,
		//	YoRigine::SoundCategory::SE
		//);
		// 方向ヒット状態へ遷移
		combat_->SetHitDirection(dir);
		combat_->ChangeState(CombatState::Hit);
	}
}

/// <summary>
/// 全システムのリセット処理
/// </summary>
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

/// <summary>
/// ダメージ処理
/// </summary>
void Player::TakeDamage(int damage) {
	if (!isAlive_ || hp_ <= 0) return;

	hp_ -= damage;

	// HPが0以下になったら死亡
	if (hp_ <= 0) {
		hp_ = 0;
		isAlive_ = false;
	}
}

/// <summary>
/// 復活処理
/// </summary>
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

void Player::SetInitialPosition()
{
	LookAtDirection(Vector3(0.0f, 0.0f, 0.0f));
	SetPosition({ 23.4f, 0.0f, 4.4f });
	wt_.UpdateMatrix();
}

/// <summary>
///	指定方向を向く関数
/// </summary>
/// <param name="direction"></param>
void Player::LookAtDirection(const Vector3& direction)
{
	Vector3 dir = direction - wt_.translate_;
	dir.y = 0.0f; // 水平方向のみ
	dir = Vector3::Normalize(dir);
	float targetYaw = std::atan2f(dir.x, dir.z); // ラジアンで計算
	wt_.rotate_.y = targetYaw;
	followCamera_->SetRotate({ followCamera_->GetRotate().x, targetYaw,followCamera_->GetRotate().z});
}
