#include "BattleEnemy.h"
#include "Player/Player.h"
#include "Systems/GameTime/GameTime.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include <Loaders/Json/JsonManager.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>

#include "States/BattleIdleState.h"
#include "States/Attack/BattleRushAttackState.h"
#include "States/BattleDamageState.h"
#include "States/BattleDownedState.h"
#include "States/BattleDeadState.h"

#include "Debugger/Logger.h"
#include <Collision/AreaCollision/Base/AreaManager.h>

/*==========================================================================
デストラクタ
//========================================================================*/
BattleEnemy::~BattleEnemy() {
	if (obbCollider_) {
		obbCollider_->~OBBCollider();
	}
}

/*==========================================================================
メイン初期化
//========================================================================*/
void BattleEnemy::Initialize(Camera* camera) {
	camera_ = camera;
	obj_ = std::make_unique<Object3d>();
	obj_->Initialize();
	wt_.Initialize();
	wt_.useAnchorPoint_ = true;
	InitCollision();

	healthBarUI_ = std::make_unique<EnemyHealthBarUI>(this, camera);
	healthBarUI_->Initialize();
}

/*==========================================================================
戦闘用データを使用して初期化
//========================================================================*/
void BattleEnemy::InitializeBattleData(const BattleEnemyData& data, Vector3 position)
{
	// データ適用
	enemyData_ = data;
	enemyData_.maxHp_ = data.hp;
	enemyData_.currentHp_ = enemyData_.maxHp_;

	// モデル設定
	if (obj_) {
		obj_->SetModel(data.modelPath);
	}

	// 初期位置設定
	wt_.translate_ = position;
	isAlive_ = true;

	this->InitJson();
	// 初期ステートは Idle に設定
	ChangeState(std::make_unique<BattleIdleState>());
	Logger(("[BattleEnemy] Initialized from JSON: ID=" + data.enemyId + ", HP=" + std::to_string(data.hp) + "\n").c_str());
}

/*==========================================================================
コリジョンの初期化
//========================================================================*/
void BattleEnemy::InitCollision() {
	// OBBコライダー生成
	obbCollider_ = ColliderFactory::Create<OBBCollider>(
		this, &wt_, camera_, static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy));
}

/*==========================================================================
Jsonの初期化
//========================================================================*/
void BattleEnemy::InitJson() {
	std::string identifier = enemyData_.enemyId;
	if (identifier.empty()) identifier = "UnknownEnemy";

	jsonManager_ = std::make_unique<YoRigine::JsonManager>(identifier, "Resources/Json/Objects/BattleEnemies");
	jsonManager_->SetCategory(identifier);
	obbCollider_->InitJson(jsonManager_.get());
}

/*==========================================================================
更新処理
//========================================================================*/
void BattleEnemy::Update() {
	// プレイヤーが死んでいたら更新しないように
	if (player_->GetCombat()->IsDead())
		return;

	float dt = YoRigine::GameTime::GetDeltaTime();
	stateTimer_ += dt;

	// 現在のステートを更新
	if (currentState_) {
		currentState_->Update(*this, dt);
	}

	// ノックバック更新
	UpdateKnockback(dt);

	// 死亡チェック
	if (enemyData_.currentHp_ == 0) {
		ChangeState(std::make_unique<BattleDeadState>());
		PlayDeathEffect();
	}



	// エリア制限補正
	AreaManager::GetInstance()->UpdateSingleObject(&wt_);
	// 行列とコリジョン更新
	wt_.UpdateMatrix();
	healthBarUI_->Update();
	if (obbCollider_) obbCollider_->Update();
}

/*==========================================================================
状態の切り替え
//========================================================================*/
void BattleEnemy::ChangeState(std::unique_ptr<IEnemyState<BattleEnemy>> newState) {
	if (currentState_) currentState_->Exit(*this);
	currentState_ = std::move(newState);
	if (currentState_) currentState_->Enter(*this);
	stateTimer_ = 0.0f;
}

/*==========================================================================
プレイヤーの現在位置の取得
//========================================================================*/
Vector3 BattleEnemy::GetPlayerPosition() const {
	if (player_) {
		return player_->GetWorldPosition();
	}
	return Vector3(0.0f, 0.0f, 0.0f);
}


/*==========================================================================
攻撃の実行
//========================================================================*/
void BattleEnemy::PerformBasicAttack() {
	if (!player_) return;
	player_->TakeDamage(enemyData_.attack);
}

/*==========================================================================
死亡時の演出
//========================================================================*/
void BattleEnemy::PlayDeathEffect() {
	if (obj_) obj_->SetMaterialColor({ 0.0f,0.0f,0.0f,1.0f });
}

/*==========================================================================
ダメージを受ける処理
//========================================================================*/
void BattleEnemy::TakeDamage(int damage) {
	if (isInvincible_ || !IsAlive()) return;
	enemyData_.currentHp_ -= damage;
	if (enemyData_.currentHp_ < 0) enemyData_.currentHp_ = 0;
}

/*==========================================================================
HPの回復
//========================================================================*/
void BattleEnemy::Heal(int amount)
{
	if (!IsAlive()) return;
	enemyData_.currentHp_ += amount;
	if (enemyData_.currentHp_ > enemyData_.maxHp_) enemyData_.currentHp_ = enemyData_.maxHp_;
}

/*==========================================================================
ダメージを受けた瞬間の点滅処理
//========================================================================*/
void BattleEnemy::UpdateBlinking(float dt) {
	// ダメージ時の点滅中でなければ処理しない
	if (!isDamageBlinking_) return;
	blinkTimer_ += dt;

	// サイン波を利用してα値を周期的に変化させる
	float alpha = 0.65f + 0.35f * std::sin(blinkTimer_ * blinkSpeed_);

	if (obj_) {
		obj_->GetColor() = { 1.0f, 0.0f, 0.0f, alpha };
	}
}

/*==========================================================================
ノックバック開始の処理
//========================================================================*/
void BattleEnemy::StartKnockback(const Vector3& direction, float power, float duration)
{
	knockbackData_.isKnockingBack_ = true;
	knockbackData_.knockbackDirection_ = Vector3::Normalize(direction);
	knockbackData_.knockbackPower_ = power;
	knockbackData_.knockbackDuration_ = duration;
	knockbackData_.knockbackTimer_ = 0.0f;
}

/*==========================================================================
ノックバック中の処理
//========================================================================*/
void BattleEnemy::UpdateKnockback(float dt)
{
	// ノックバック中でなければ処理しない
	if (!knockbackData_.isKnockingBack_) return;
	knockbackData_.knockbackTimer_ += dt;

	// 時間経過でパワーを減衰
	float currentPower = knockbackData_.knockbackPower_ * (1.0f - (knockbackData_.knockbackTimer_ / knockbackData_.knockbackDuration_));
	Vector3 delta = knockbackData_.knockbackDirection_ * currentPower * dt;
	AddTranslate(delta);

	if (knockbackData_.knockbackTimer_ >= knockbackData_.knockbackDuration_) {
		knockbackData_.isKnockingBack_ = false;
		knockbackData_.knockbackPower_ = 0.0f;
	}
}

/*==========================================================================
ヒットした瞬間
//========================================================================*/
void BattleEnemy::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {

	// 攻撃を食らった時
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)) {
		if (isAlive_) {
			TakeDamage(static_cast<int>(player_->GetCombat()->GetCombo()->GetCurrentDamage()));
			ChangeState(std::make_unique<BattleDamageState>());

			Vector3 knockbackDir = wt_.translate_ - player_->GetWorldPosition();
			knockbackDir.y = 0.0f;
			knockbackDir = Vector3::Normalize(knockbackDir);

			float power = player_->GetCombat()->GetCombo()->GetCurrentKnockback();
			float duration = player_->GetCombat()->GetCombo()->GetCurrentKnockbackDuration();
			StartKnockback(knockbackDir, power, duration);

			// 攻撃ヒット時の前進ステップを発火
			player_->GetCombat()->GetCombo()->OnHitStep(wt_.translate_);

		}
	}

	// 盾に当たった時
	if (dynamic_cast<BattleRushAttackState*>(GetCurrentState()) != nullptr) {
		if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield)) {
			if (player_->GetCombat()->GetGuard()->GetState() == PlayerGuard::State::Active ||
				player_->GetCombat()->GetGuard()->GetState() == PlayerGuard::State::Recovery) {
				if (isAlive_) {
					ChangeState(std::make_unique<BattleDownedState>());
				}
			}
		}
	}

	// プレイヤー本体に当たった時
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) {
		PerformBasicAttack();
	}
}

/*==========================================================================
ヒット中
//========================================================================*/
void BattleEnemy::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {

}

/*==========================================================================
ヒットから離脱した瞬間
//========================================================================*/
void BattleEnemy::OnExitCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {}

/*==========================================================================
ヒット方向別処理
//========================================================================*/
void BattleEnemy::OnDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir) {}

void BattleEnemy::OnEnterDirectionCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other, [[maybe_unused]] HitDirection dir)
{
}

/*==========================================================================
描画
//========================================================================*/
void BattleEnemy::Draw() {
	// 死亡していたら半透明にする
	if (player_->GetCombat()->IsDead()) {

		// 現在のマテリアルカラー取得
		Vector4 currentColor = obj_->GetColor();

		// 目標のカラー
		Vector4 targetColor = { 1.0f, 1.0f, 1.0f, 0.0f };

		// 線形補間 (Lerp)
		currentColor = Lerp(currentColor, targetColor, fadeSpeed_ * YoRigine::GameTime::GetUnscaledDeltaTime());

		// 設定
		obj_->SetMaterialColor(currentColor);
		if (currentColor.w <= 0.01f) {
			canAct_ = false;
		}
	}
	if (obj_) obj_->Draw(camera_, wt_);
}

/*==========================================================================
UI描画
//========================================================================*/
void BattleEnemy::DrawUI()
{
	if (isAlive_ && player_->IsAlive()) {
		healthBarUI_->Draw();
	}
}

/*==========================================================================
影描画
//========================================================================*/
void BattleEnemy::DrawShadow()
{

	if (obj_) obj_->DrawShadow(wt_);
}

/*==========================================================================
コリジョン可視化
//========================================================================*/
void BattleEnemy::DrawCollision() {
	if (obbCollider_) obbCollider_->Draw();
}