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
#include "Object3D/BaseObjectManager.h"
#include "States/BattleRecoveryState.h"
#include "Particle/YEmitterGroupManager.h"

#include <UI/Damage/DamageNumberManager.h>

/*==========================================================================
デストラクタ
//========================================================================*/
BattleEnemy::~BattleEnemy() {
	// どの削除経路（撃破 / 全消去 / シーン終了）でも確実に登録解除する
	BaseObjectManager::GetInstance()->Unregister(this);

	// 燃焼などの付着VFXを取り残さない
	StopStatusVfx();

	if (obbCollider_) {
		obbCollider_->~OBBCollider();
	}
}

/*==========================================================================
メイン初期化
//========================================================================*/
void BattleEnemy::Initialize(YoRigine::Camera* camera) {
	camera_ = camera;
	obj_ = std::make_unique<YoRigine::Object3d>();
	obj_->Initialize();
	wt_.Initialize();
	visualWt_.Initialize();
	wt_.useAnchorPoint_ = true;
	visualWt_.useAnchorPoint_ = true;
	InitCollision();

	healthBarUI_ = std::make_unique<EnemyHealthBarUI>(this, camera);
	healthBarUI_->Initialize();

	animation_ = std::make_unique<ObjectAnimation>();
	animation_->SetBaseColor({ 1.0f,1.0f,1.0f,1.0f });
	animation_->SetBaseScale({ 1.0f,1.0f,1.0f });

}

/*==========================================================================
戦闘用データを使用して初期化
//========================================================================*/
void BattleEnemy::InitializeBattleData(const BattleEnemyData& data, Vector3 position, const Vector3& scale)
{
	// データ適用
	enemyData_ = data;
	SetupHP(data.hp);

	// モデル設定
	if (obj_) {
		obj_->SetModel(data.modelPath);
	}

	// 初期位置・スケール設定（フィールド敵の見た目を引き継ぐ）
	wt_.translate_ = position;
	wt_.scale_ = scale;

	// バトル開始演出中は敵のUpdateが停止するため、生成時点で描画用Transformも
	// 同期しておく。これが無いと複数体が原点に重なり、1体だけに見える。
	wt_.UpdateMatrix();
	SyncVisualTransform();

	// 攻撃時の punch/bounce アニメーションは baseScale_ を起点に補間するので、
	// ここで揃えないと攻撃の瞬間にスケールが (1,1,1) へスナップしてしまう。
	if (animation_) {
		animation_->SetBaseScale(scale);
	}
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
	obbCollider_->SetIsStatic(false);
	obbCollider_->SetMass(1.0f);
	// ジャンプ攻撃等で自前でYを制御するため、押し戻しは水平方向のみに限定する。
	// これを許すと押し戻しのY成分がジャンプ中のY設定と競合してガタつく。
	obbCollider_->SetLockPenetrationY(true);
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

	// 死亡演出（ディゾルブ）パラメータをエディタへ公開
	jsonManager_->SetTreePrefix("ディゾルブ");
	jsonManager_->Register("継続時間(秒)", &dissolveDuration_);
	jsonManager_->Register("エッジ幅", &dissolveEdgeWidth_);
	jsonManager_->Register("エッジ発光色", &dissolveEdgeColor_);
	jsonManager_->Register("ノイズスケール", &dissolveNoiseScale_);
	jsonManager_->ClearTreePrefix();

	jsonManager_->SetTreePrefix("ヒットリアクション");
	jsonManager_->Register("のけぞり角度(rad)", &hitReactionAngle_);
	jsonManager_->Register("のけぞり時間(秒)", &hitReactionDuration_);
	jsonManager_->ClearTreePrefix();
}

/*==========================================================================
更新処理
//========================================================================*/
void BattleEnemy::Update() {
	// プレイヤーが死んでいたら更新しないように
	if (player_->GetCombat()->IsDead())
		return;

	// 死亡チェック（1度だけ遷移させる。毎フレーム走ると PlayDeathEffect で
	// 有効化したディゾルブが直後の Exit() で無効化されてしまい、threshold も
	// deathTimer もリセットされ続けるため、見た目には何も起きなくなる）
	if (currentHp_ == 0 && logicalState_ != BattleEnemyState::Dead) {
		logicalState_ = BattleEnemyState::Dead;
		StopStatusVfx(); // 死体が燃え続けないように付着VFXを止める
		PlayDeathEffect();
		ChangeState(std::make_unique<BattleDeadState>());
	}

	float dt = YoRigine::GameTime::GetDeltaTime();
	stateTimer_ += dt;
	MarkPreviousPosition();

	// アニメーター更新
	UpdateAnimation(dt);

	// ヒットカウントのリセット処理（しきい値は CounterAttackParams 経由）
	if (consecutiveHitCount_ > 0 && !isInvincible_) {
		hitCountResetTimer_ += dt;
		if (hitCountResetTimer_ > enemyData_.attackParams.counter.hitCountResetTime) {
			consecutiveHitCount_ = 0;
			hitCountResetTimer_ = 0.0f;
		}
	}

	// 現在のステートを更新
	if (currentState_) {
		currentState_->Update(*this, dt);
	}

	// ノックバック・のけぞり・状態VFX（燃焼など）の更新
	UpdateReactions(dt);

	// エリア制限補正
	AreaManager::GetInstance()->UpdateSingleObject(&wt_);

	// 行列と更新
	UpdateVelocity();
	wt_.UpdateMatrix();
	// 描画専用Transformにだけのけぞり回転を加える。コライダーはwt_のまま傾けない。
	SyncVisualTransform();
	// コリジョン更新
	if (obbCollider_) {
		obbCollider_->Update();
		obbCollider_->SetVelocity(currentVelocity_);
	}


	// UI更新
	healthBarUI_->Update();
}

/*==========================================================================
状態の切り替え
//========================================================================*/
void BattleEnemy::ChangeState(std::unique_ptr<IEnemyState<BattleEnemy>> newState) {
	if (currentState_) currentState_->Exit(*this);
	currentState_ = std::move(newState);
	lastDealtContactDamageWindow_ = -1;
	if (currentState_) currentState_->Enter(*this);
	stateTimer_ = 0.0f;
}

/*==========================================================================
攻撃の実行
//========================================================================*/
void BattleEnemy::PerformBasicAttack() {
	if (!player_) return;
	// 突進（ホーミング）攻撃中など無敵のときはダメージものけぞりも与えない
	if (player_->IsInvincible()) return;
	player_->TakeDamage(enemyData_.attack);
	// 被弾したプレイヤーをのけぞらせる（ヒットモーションへ遷移）
	player_->ApplyHitReaction(wt_.translate_);
}

/*==========================================================================
死亡時の演出 — ディゾルブ開始（threshold は BattleDeadState が時間で進める）
//========================================================================*/
void BattleEnemy::PlayDeathEffect() {
	if (!obj_) return;
	obj_->SetDissolveEnabled(true);
	obj_->SetDissolveThreshold(0.0f);
	obj_->SetDissolveEdgeWidth(dissolveEdgeWidth_);
	obj_->SetDissolveEdgeColor(dissolveEdgeColor_);
	obj_->SetDissolveNoiseScale(dissolveNoiseScale_);
}

void BattleEnemy::TryPerformContactAttack() {
	if (!currentState_) return;
	const int damageWindow = currentState_->GetContactDamageWindow();
	if (damageWindow < 0 || damageWindow == lastDealtContactDamageWindow_) return;

	PerformBasicAttack();
	// 無敵中などで実ダメージが通らなかった場合も、同じ判定時間で毎フレーム再試行しない。
	lastDealtContactDamageWindow_ = damageWindow;
}

/*==========================================================================
ヒットした瞬間
//========================================================================*/
void BattleEnemy::OnEnterCollision([[maybe_unused]] BaseCollider* self, BaseCollider* other) {

	if (isAlive_ && !isInvincible_) {
		// 攻撃を食らった時
		if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)) {
			// ダウン中などはダメージを受けても、その反撃チャンス状態を維持する。
			const bool keepCurrentState = currentState_ && currentState_->KeepsStateWhenDamaged();

			// --------------------- ダメージの処理 --------------------- //
			int damage = static_cast<int>(player_->GetCombat()->GetCombo()->GetCurrentDamage());
			TakeDamage(damage);

			// ダメージ数値UIをスポーン（頭上Yオフセットは DamageNumberManager 側でJSON調整）
			bool isSine = (player_->GetCombat()->GetComboDamageMultiplier() > 1.0f);
			DamageNumberManager::GetInstance()->SpawnDamage(damage, wt_.translate_, isSine);
			// --------------------- ヒットエフェクトの処理 --------------------- //
			//auto* enemyHitEmitterGroup_ = YEmitterGroupManager::GetInstance().GetGroup("EnemyHit");
			//if (enemyHitEmitterGroup_) {
			//	enemyHitEmitterGroup_->SetPosition(wt_.translate_);
			//	enemyHitEmitterGroup_->SetActive(true);
			//	enemyHitEmitterGroup_->SetAutoEmitAll(false);  // 自動射出OFF
			//	enemyHitEmitterGroup_->EmitAll(10);
			//}

			// --------------------- ヒットカウントの処理 --------------------- //
			// しきい値・有効フラグは CounterAttackParams (JSON 経由) で調整可能
			if (!keepCurrentState) {
				const auto& counterParams = enemyData_.attackParams.counter;
				consecutiveHitCount_++;
				hitCountResetTimer_ = 0.0f;
				// 連続ヒット数が限界を超え、かつカウンター挙動が有効なら Recovery 状態へ
				if (counterParams.enabled && consecutiveHitCount_ >= counterParams.triggerHitCount) {
					ChangeState(std::make_unique<BattleRecoveryState>());
					consecutiveHitCount_ = 0;  // リセット
				}
				else {
					// 通常のダメージ状態
					ChangeState(std::make_unique<BattleDamageState>());
				}
			}

			// --------------------- ノックバックの処理 --------------------- //
			Vector3 knockbackDir = wt_.translate_ - player_->GetWorldPosition();
			knockbackDir.y = 0.0f;
			knockbackDir = Vector3::Normalize(knockbackDir);

			float power = player_->GetCombat()->GetCombo()->GetCurrentKnockback();
			float duration = player_->GetCombat()->GetCombo()->GetCurrentKnockbackDuration();
			StartKnockback(knockbackDir, power, duration);
			StartDirectionalHitReaction(knockbackDir);

			// --------------------- 攻撃ヒット時の前進ステップを発火 --------------------- //
			player_->GetCombat()->GetCombo()->OnHitStep(wt_.translate_);

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

		// プレイヤー本体に当たった時。攻撃実行中のStateの時だけダメージを与える
		if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) {
			TryPerformContactAttack();
		}
	}
}

/*==========================================================================
ヒット中
//========================================================================*/
void BattleEnemy::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {
	if (!isAlive_) return;

	// 溜め中から接触し続けたまま攻撃判定時間へ入った場合にも、一度だけ命中させる。
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer)) {
		TryPerformContactAttack();
	}

	// 敵同士の押し出し処理（重なり防止）
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)) {
		Vector3 otherPos = other->GetWorldTransform().translate_;
		Vector3 myPos = wt_.translate_;

		Vector3 pushDir = myPos - otherPos;
		pushDir.y = 0.0f; // 水平方向のみ

		float dist = Length(pushDir);
		if (dist < 0.001f) {
			// 完全に重なっている場合はランダムな方向にずらす
			pushDir = { ((rand() % 100) - 50) * 0.01f, 0.0f, ((rand() % 100) - 50) * 0.01f };
			if (Length(pushDir) < 0.001f) pushDir = {1.0f, 0.0f, 0.0f};
		}
		
		pushDir = Normalize(pushDir);
		
		// 押し出し係数
		float pushForce = 2.5f * YoRigine::GameTime::GetDeltaTime(); 
		wt_.translate_ += pushDir * pushForce;
	}
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
// プレイヤー敗北時の半透明フェード。
// インスタンシング描画では Draw() が呼ばれず Submit が色を読むだけなので、
// フェードの色更新をここに分離し、マネージャの描画ループ直前で呼ぶ（発火条件は旧 Draw と同じ）。
void BattleEnemy::ApplyDeathFade() {
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
}

void BattleEnemy::Draw() {
	ApplyDeathFade();
	if (obj_) obj_->Draw(camera_, visualWt_);
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

	if (obj_) obj_->DrawShadow(visualWt_);
}

/*==========================================================================
コリジョン可視化
//========================================================================*/
void BattleEnemy::DrawCollision() {
	if (obbCollider_) obbCollider_->Draw();
}
