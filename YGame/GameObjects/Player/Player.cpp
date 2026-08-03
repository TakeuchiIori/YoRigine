#include "Player.h"

// App
#include "Combo/ComboTypes.h"
#include "Guard/PlayerGuard.h"

// Engine
#include "Systems/GameTime/GameTime.h"
#include "Systems/Cinematic/CinematicManager.h"
#include <Debugger/Logger.h>
#include "Collision/AreaCollision/Base/AreaManager.h"
#include "Particle/EffectHandle.h"
#include "Model/Model.h"
#include "Model/Motion/Core/MotionSystem.h"

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
void Player::Initialize(YoRigine::Camera* camera) {
	camera_ = camera;

	// オブジェクト初期化
	obj_ = std::make_unique<YoRigine::Object3d>();
	obj_->Initialize();
	obj_->SetModel("Player.gltf", true, "Idle4");
	input_ = YoRigine::Input::GetInstance();

	// 操作の読み取りは全て PlayerInput 経由にする。
	// 生のキーコードを知っているのはこのクラスの初期化だけになる。
	playerInput_ = std::make_unique<PlayerInput>();
	playerInput_->Initialize();

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

	boneLine_ = std::make_unique<YoRigine::Line>();
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
	magicController_ = std::make_unique<PlayerMagicController>(this);
	styleController_ = std::make_unique<PlayerStyleController>();

	// アクション変更コールバック（デバッグログ）
	combat_->SetActionCallback([this]([[maybe_unused]] const std::string& action) {
		});
}

// ============================================================
// 戦闘用入力の初期化
// ============================================================
void Player::HandleCombatInput() {

	if (playerCamera_ && playerCamera_->IsInPerformance()) return;

	// 攻撃/ガードは戦闘中のみ。フィールド探索中はブンブン振れない。
	if (!battleMode_) return;

	if (!playerInput_) return;

	// スタイル切替は戦闘スタイルの切り替えだけを担当する。
	// スタイル状態を専用クラスへ逃がすことで、剣/魔法が増えても PlayerCombat にUI都合の分岐を背負わせない。
	if (styleController_ && playerInput_->StyleToggleTriggered()) {
		styleController_->Toggle();
	}

	// 攻撃中の入力は AttackingCombatState が AttackData の inputBufferStart に従ってバッファ管理する
	if (!combat_->IsIdle()) return;

	if (styleController_ && styleController_->IsMagic()) {
		HandleMagicInput();
		return;
	}

	HandleSwordInput();
}

// ============================================================
// 剣スタイル入力
// ============================================================
void Player::HandleSwordInput() {
	// 軽攻撃コンボ開始
	if (playerInput_->AttackLightTriggered()) {
		combat_->TryAttack(AttackType::A_Arte);
	}

	// 重攻撃コンボ開始
	if (playerInput_->AttackHeavyTriggered()) {
		combat_->TryAttack(AttackType::B_Arte);
	}

	// ガード
	if (playerInput_->GuardTriggered()) {
		combat_->TryGuard();
	}
}

// ============================================================
// 魔法スタイル入力
// ============================================================
void Player::HandleMagicInput() {
	if (!magicController_) return;

	// 魔法入力はコントローラーへ渡すだけにする。
	// Player は「どのスロット入力か」だけを渡し、溜め・離し・イベント実行は魔法側へ閉じ込める。
	// 剣スタイルの軽攻撃/重攻撃/ガードと同じアクションを、魔法では3つのスロットとして解釈する。
	magicController_->HandleSlotInput(PlayerMagicSlot::Primary,
		playerInput_->AttackLightTriggered(), playerInput_->AttackLightHeld());
	magicController_->HandleSlotInput(PlayerMagicSlot::Secondary,
		playerInput_->AttackHeavyTriggered(), playerInput_->AttackHeavyHeld());
	magicController_->HandleSlotInput(PlayerMagicSlot::Utility,
		playerInput_->GuardTriggered(), playerInput_->GuardHeld());
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
	// 敵と重なっても押し戻しで浮き上がらないよう水平方向のみ押し戻す
	obbCollider_->SetLockPenetrationY(true);
	// Player は画面外でも常に当たり判定を回す
	obbCollider_->SetCheckOutsideCamera(false);

}

// ============================================================
// 更新処理
// ============================================================
void Player::Update() {
	if (YoRigine::GameTime::IsPause()) {
		return;
	}

	// カットシーン演出中はプレイヤー操作・状態更新を全停止
	if (YoRigine::CinematicManager::GetInstance()->IsActive()) {
		return;
	}

	// 入力処理。演出中も描画・アニメーション更新は継続し、操作だけを止める。
	if (controlEnabled_) {
		HandleCombatInput();
	}

	// スタイルに応じて武器の見た目（剣／杖）を同期する。
	const bool isMagicStyle = styleController_ && styleController_->IsMagic();
	if (playerSword_) {
		playerSword_->SetMagicVisual(isMagicStyle);
	}
	// 魔法スタイルでは防御できないので盾を外す（表示・判定とも）。
	if (playerShield_) {
		playerShield_->SetVisible(!isMagicStyle);
	}

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
	// 盾のコライダーON/OFF制御（魔法スタイルでは防御しないので常に無効）
	if (!isMagicStyle && combat_->GetCurrentState() == CombatState::Guarding) {
		playerShield_->SetEnableCollider(true);
	}
	else {
		playerShield_->SetEnableCollider(false);
	}

	UpdateMotionTime();
	Vector3 sp = playerSword_->GetWowldPosition();

	// 操作ロック中はゲームプレイ用StateMachineを進めず、下のアニメーション・
	// 装備追従更新だけを行う。
	if (controlEnabled_) {
		movement_->Update(YoRigine::GameTime::GetDeltaTime());
		combat_->Update(YoRigine::GameTime::GetDeltaTime());
		if (magicController_) magicController_->Update(YoRigine::GameTime::GetDeltaTime());
	}

	// ガードで押し込まれる移動。
	// 通常移動の後に適用して、押されている間も入力で歩けるようにする。
	UpdateGuardPush(YoRigine::GameTime::GetDeltaTime());

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
		if (magicController_) magicController_->DrawCollision();
		obbCollider_->Draw();
	}
}

// ============================================================
// 骨の描画
// ============================================================
void Player::DrawBone(YoRigine::Line& line) {
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
#ifdef USE_IMGUI
	if (magicController_) magicController_->ShowDebugImGui();

	// ガードはタイムラインの前後関係が要になるので、
	// 数値の羅列ではなくドープシート付きの専用エディタで編集する。
	if (ImGui::CollapsingHeader("ガード設定")) {
		guardEditor_.SetTarget(combat_->GetGuard());
		guardEditor_.Draw();
	}
#endif
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
	return wt_.translate_;
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
	// トゥーンは ToonSettings（全オブジェクト共通／エディタの「トゥーン」パネル）で統一管理

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
	// 被弾フィードバック
	//------------------------------------------------------------
	jsonManager_->SetTreePrefix("被弾演出");
	jsonManager_->Register("ヒットストップ秒数", &hitStopDuration_);
	jsonManager_->Register("正面シェイク強度", &frontHitShakeIntensity_);
	jsonManager_->Register("正面シェイク時間", &frontHitShakeDuration_);
	jsonManager_->Register("背面シェイク強度", &backHitShakeIntensity_);
	jsonManager_->Register("背面シェイク時間", &backHitShakeDuration_);
	jsonManager_->Register("振動時間", &hitVibrationDuration_);
	jsonManager_->Register("振動強度", &hitVibrationPower_);

	//------------------------------------------------------------
	// 下層システム登録
	//------------------------------------------------------------
	movement_->InitJson(jsonManager_.get());
	combat_->GetCombo()->InitJson(jsonManager_.get());
	// ガード設定は項目数が多くタイムライン編集も要るため、
	// Player.json ではなく専用ファイル＋専用エディタで扱う。
	combat_->GetGuard()->LoadConfig();

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
		// ガード中は盾コライダー側で処理するため、プレイヤー本体ではスキップ
		if (combat_->IsGuarding()) return;

		//------------------------------------------------------------
		// SE再生
		//------------------------------------------------------------

		// YoRigine::Audio::GetInstance()->PlayOneShot(
		//	"Resources/Audio/.mp4",
		//	0.2f,
		//	YoRigine::SoundCategory::SE
		//);

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
	ResetForBattleStart();
	if (styleController_) styleController_->Reset();
	if (playerSword_) playerSword_->SetMagicVisual(false);
	if (playerShield_) playerShield_->SetVisible(true);
}

void Player::ResetForBattleStart() {
	isAlive_ = hp_ > 0;
	isInvincible_ = false;
	// バトル開始装備は剣＋盾に統一する。状態変更後に表示も明示的に同期する。
	if (styleController_) styleController_->Reset();
	if (combat_) combat_->Reset();
	if (magicController_) magicController_->Reset();
	if (movement_) {
		movement_->ForceStop();
		movement_->ChangeState(MovementState::Idle);
		movement_->SetCanMove(true);
		movement_->SetCanRotate(true);
		movement_->SetIsRotating(false);
	}
	if (playerSword_) playerSword_->ResetRuntimeState();
	if (playerShield_) playerShield_->SetEnableCollider(false);
	if (playerCamera_) playerCamera_->StopAttackCameraWork();
	if (playerSword_) playerSword_->SetMagicVisual(false);
	if (playerShield_) playerShield_->SetVisible(true);

	// Idleモーションに戻す
	if (obj_) {
		obj_->SetMotionSpeed(motionSpeed[0]);
		obj_->SetChangeMotion("Player.gltf", MotionPlayMode::Loop, "Idle4");
		if (obj_->GetModel() && obj_->GetModel()->GetMotionSystem()) {
			auto* motion = obj_->GetModel()->GetMotionSystem();
			motion->ResetPlaybackState();
		}
		obj_->UpdateAnimation();
	}
	wt_.UpdateMatrix();
	if (playerSword_) playerSword_->Update();
	if (playerShield_) playerShield_->Update();
}

void Player::FacePosition(const Vector3& worldPosition) {
	LookAtDirection(worldPosition);
	wt_.UpdateMatrix();
}

void Player::SetControlEnabled(bool enabled) {
	controlEnabled_ = enabled;
	if (!movement_) return;
	if (!enabled) {
		movement_->ForceStop();
		movement_->ChangeState(MovementState::Idle);
		movement_->SetCanMove(false);
		movement_->SetCanRotate(false);
		movement_->SetIsRotating(false);
	}
	else {
		movement_->SetCanMove(true);
		movement_->SetCanRotate(true);
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
// 被弾時のヒット（のけぞり）反応
// 敵の攻撃が実際にヒットしたタイミングで呼ばれ、被弾方向に応じた
// ヒットモーションへ遷移させる。方向ヒットのコールバックは衝突系から
// 発火されないため、ダメージ処理側から明示的に呼ぶ設計にしている。
// ============================================================
void Player::ApplyHitReaction(const Vector3& attackerPos) {
	// 死亡・ガード中・無敵中はのけぞらせない（ガードは盾側で処理）
	if (!isAlive_ || hp_ <= 0) return;
	if (isInvincible_) return;
	if (combat_->IsDead() || combat_->IsGuarding()) return;

	// 攻撃者との水平relative位置から、自分の向き基準で前/後を判定
	Vector3 toAttacker = attackerPos - wt_.translate_;
	toAttacker.y = 0.0f;

	const float facing = wt_.rotate_.y;
	const float forwardX = std::sin(facing);
	const float forwardZ = std::cos(facing);
	const float dot = toAttacker.x * forwardX + toAttacker.z * forwardZ;

	// 前方から食らったら Front、背後から食らったら Back
	HitDirection dir = (dot < 0.0f) ? HitDirection::Back : HitDirection::Front;

	PlayHitFeedback(dir);

	combat_->SetHitDirection(dir);
	combat_->ChangeState(CombatState::Hit);
}

// ============================================================
// 被弾時フィードバック
// ============================================================
void Player::PlayHitFeedback(HitDirection direction) {
	if (hitStopDuration_ > 0.0f) {
		YoRigine::GameTime::SetHitStop(hitStopDuration_);
	}

	const bool fromBack = direction == HitDirection::Back;
	const float shakeIntensity = fromBack ? backHitShakeIntensity_ : frontHitShakeIntensity_;
	const float shakeDuration = fromBack ? backHitShakeDuration_ : frontHitShakeDuration_;
	if (playerCamera_ && shakeIntensity > 0.0f && shakeDuration > 0.0f) {
		playerCamera_->StartShake(shakeIntensity, shakeDuration);
	}

	if (input_ && hitVibrationDuration_ > 0.0f && hitVibrationPower_ > 0) {
		input_->StartVibration(0, hitVibrationDuration_, hitVibrationPower_, hitVibrationPower_);
	}
}

// ============================================================
// 敵の攻撃を受けたときの入口
//
// ガード判定はここで一度だけ行う。判定結果によって
//   ・失敗   → 素のダメージ + のけぞり
//   ・ガード → 軽減ダメージ + CC消費 + 自分が押し込まれる
//   ・パリィ → 無効化 + CC回復 + 自分は動かない（相手を突き放すのは攻撃側の仕事）
// と処理を分ける。
// ============================================================
PlayerGuard::GuardResult Player::ApplyDamage(int damage, const Vector3& attackerPos) {
	using GuardResult = PlayerGuard::GuardResult;

	// 無敵中・死亡中は判定そのものを行わない
	if (!isAlive_ || hp_ <= 0 || isInvincible_) {
		return GuardResult::GuardFail;
	}

	PlayerGuard* guard = combat_ ? combat_->GetGuard() : nullptr;
	const GuardResult result = guard ? guard->ResolveHit(attackerPos) : GuardResult::GuardFail;

	// ------------------------------------------------------------
	// 防御が成立しなかった場合は素通し
	// ------------------------------------------------------------
	if (result == GuardResult::GuardFail) {
		TakeDamage(damage);
		ApplyHitReaction(attackerPos);
		return result;
	}

	// ------------------------------------------------------------
	// 防御成立。結果パラメータに従って軽減とCCの増減を行う
	// ------------------------------------------------------------
	const GuardOutcome* outcome = guard->GetOutcome(result);
	if (!outcome) return result;

	const int reducedDamage = static_cast<int>(damage * outcome->damageRate);
	if (reducedDamage > 0) {
		TakeDamage(reducedDamage);
	}

	if (auto* combo = combat_->GetCombo()) {
		if (outcome->ccCost > 0)    combo->ConsumeCC(outcome->ccCost);
		if (outcome->ccRecover > 0) combo->RecoverCC(outcome->ccRecover);
	}

	// ------------------------------------------------------------
	// 押し込み。
	// 通常ガードは selfPushDistance > 0 で自分が下がり、
	// パリィは 0 なので踏みとどまる。この差が両者の性格になる。
	// ------------------------------------------------------------
	if (outcome->selfPushDistance > 0.0f && outcome->selfPushDuration > 0.0f) {
		Vector3 pushDir = wt_.translate_ - attackerPos;
		pushDir.y = 0.0f;
		if (Length(pushDir) > 0.001f) {
			StartGuardPush(Normalize(pushDir), outcome->selfPushDistance, outcome->selfPushDuration);
		}
	}

	PlayGuardFeedback(*outcome, attackerPos);
	return result;
}

// ============================================================
// ガード／パリィ成立時の手応え
// ============================================================
void Player::PlayGuardFeedback(const GuardOutcome& outcome, const Vector3& attackerPos) {
	// 時間を一瞬止めて「硬いものに当たった」ことを伝える。
	// パリィの方を長く取ると、同じ演出でも重みの差が出る。
	if (outcome.hitStop > 0.0f) {
		YoRigine::GameTime::SetHitStop(outcome.hitStop, 0.0f, outcome.hitStopEase);
	}

	if (playerCamera_ && outcome.shakeIntensity > 0.0f && outcome.shakeDuration > 0.0f) {
		playerCamera_->StartShake(outcome.shakeIntensity, outcome.shakeDuration);
	}

	// 盾のスケールを跳ね返り付きで変形させる。受け止めたことが一番分かりやすい部分。
	if (playerShield_ && outcome.shieldSquash > 0.0f) {
		playerShield_->PlayGuardImpact(outcome.shieldSquash, outcome.shieldSquashTime,
			outcome.shieldSquashAxis, outcome.shieldSquashBounce);
	}

	// 接触点のエフェクト。盾と相手のちょうど中間に出す。
	if (!outcome.vfxName.empty()) {
		Vector3 contactPos = (wt_.translate_ + attackerPos) * 0.5f;
		contactPos.y += 1.0f;
		EffectHandle::PlayOneShot(outcome.vfxName, contactPos);
	}
}

// ============================================================
// ガードで押し込まれる移動の開始
// ============================================================
void Player::StartGuardPush(const Vector3& direction, float distance, float duration) {
	if (duration <= 0.0f) return;

	guardPushDirection_ = direction;
	guardPushDuration_ = duration;
	guardPushTimer_ = duration;
	// 線形に減速して距離ちょうどで止まるよう、初速を 2*d/t にする
	guardPushSpeed_ = (distance * 2.0f) / duration;
}

// ============================================================
// ガードで押し込まれる移動の更新
// ============================================================
void Player::UpdateGuardPush(float deltaTime) {
	if (guardPushTimer_ <= 0.0f) return;

	guardPushTimer_ -= deltaTime;
	if (guardPushTimer_ <= 0.0f) {
		guardPushTimer_ = 0.0f;
		return;
	}

	// 残り時間に比例して減速させる（開始が最速、終了で0）
	const float ratio = guardPushTimer_ / guardPushDuration_;
	wt_.translate_ += guardPushDirection_ * guardPushSpeed_ * ratio * deltaTime;
}

// ============================================================
// 復活処理
// ============================================================
void Player::Revive(int reviveHP) {
	if (isAlive_) return; // すでに生存中なら処理しない

	hp_ = reviveHP;
	isAlive_ = true;
	isInvincible_ = false;

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
bool Player::IsAttackPressedA() const {
	return playerInput_ && playerInput_->AttackLightTriggered();
}

bool Player::IsAttackPressedB() const {
	return playerInput_ && playerInput_->AttackHeavyTriggered();
}

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
