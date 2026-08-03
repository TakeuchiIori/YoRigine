#include "BattleEnemy.h"
#include "Player/Player.h"
#include "Systems/GameTime/GameTime.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include <Loaders/Json/JsonManager.h>
#include <fstream>
#include <filesystem>
#include <json.hpp>

#include "States/BattleIdleState.h"
#include "States/BattleDamageState.h"
#include "States/BattleDownedState.h"
#include "States/BattleDeadState.h"

#include "Debugger/Logger.h"
#include <Collision/AreaCollision/Base/AreaManager.h>
#include "Object3D/BaseObjectManager.h"
#include "States/BattleRecoveryState.h"
#include "../AI/AttackTokenPool.h"
#include "Particle/YEmitterGroupManager.h"

#include <UI/Damage/DamageNumberManager.h>

/*==========================================================================
デストラクタ
//========================================================================*/
BattleEnemy::~BattleEnemy() {
	// どの削除経路（撃破 / 全消去 / シーン終了）でも確実に登録解除する
	BaseObjectManager::GetInstance()->Unregister(this);

	// 攻撃権を握ったまま消えると、その枠が永久に埋まってしまう
	if (auto* pool = AttackTokenPool::GetCurrent()) {
		pool->Forget(this);
	}

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

	// のけぞりの角度・時間は enemy_data.json の damageReaction 側へ移した。
	// 被弾まわりの数値が2箇所に分かれていると、どちらを触ればいいのか分からなくなる。
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
	timeInCurrentState_ += dt;
	lifeTime_ += dt;
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
	// 攻撃状態から抜けるときは必ず攻撃権を返す。
	// 被弾・ダウン・死亡による中断もここを通るので、返し忘れが起きない。
	if (currentState_ && currentState_->IsAttacking()) {
		if (auto* pool = AttackTokenPool::GetCurrent()) {
			pool->Release(this);
		}
	}

	if (currentState_) currentState_->Exit(*this);
	currentState_ = std::move(newState);
	lastDealtContactDamageWindow_ = -1;
	if (currentState_) currentState_->Enter(*this);
	stateTimer_ = 0.0f;

	// 遷移をログに残す。エディタの状態モニタで「今どう動いているか」を見るために使う。
	if (currentState_) {
		transitionLog_.push_back({ currentState_->GetName(), timeInCurrentState_, lifeTime_ });
		if (transitionLog_.size() > maxTransitionLog_) {
			transitionLog_.erase(transitionLog_.begin());
		}
	}
	timeInCurrentState_ = 0.0f;
}

/*==========================================================================
攻撃の実行
//========================================================================*/
void BattleEnemy::PerformBasicAttack() {
	if (!player_) return;
	// 突進（ホーミング）攻撃中など無敵のときはダメージものけぞりも与えない
	if (player_->IsInvincible()) return;

	// ダメージ・ガード判定・プレイヤー側のリアクションはすべて向こうで処理される。
	// こちらは結果を受け取って「攻撃を防がれた側」としての反応だけを決める。
	const auto result = player_->ApplyDamage(enemyData_.attack, wt_.translate_);

	if (result == PlayerGuard::GuardResult::GuardFail) return;

	// 防がれたので突進の勢いを殺す。
	// パリィなら大きく突き放され、通常ガードならその場で止まる程度。
	// この押し合いの差が「弾き返した」と「受け止められた」の違いになる。
	const PlayerGuard* guard = player_->GetCombat()->GetGuard();
	const GuardOutcome* outcome = guard ? guard->GetOutcome(result) : nullptr;
	if (outcome && outcome->enemyPushPower > 0.0f) {
		Vector3 pushDir = wt_.translate_ - player_->GetWorldPosition();
		pushDir.y = 0.0f;
		if (Length(pushDir) > 0.001f) {
			StartKnockback(Normalize(pushDir), outcome->enemyPushPower, outcome->enemyPushDuration);
		}
	} else {
		knockbackData_.isKnockingBack_ = false;
	}

	// パリィが実際に成立し、かつその攻撃が盾で崩せる設定ならダウンさせる。
	//
	// 以前はここではなく盾コライダーの接触側で判定していて、
	// 「盾に触れた + ガードがActiveかRecovery」だけを見ていた。
	// パリィ窓を見ていなかったため、タイミングを外したガードでも
	// 敵がダウンしてしまっていた。
	if (result == PlayerGuard::GuardResult::ParrySuccess) {
		if (currentState_ && currentState_->CanBeParried()) {
			ChangeState(std::make_unique<BattleDownedState>());
		}
	}
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

	if (!isAlive_ || isInvincible_) return;

	// 攻撃を食らった時。
	// ここでは「誰にどう殴られたか」を集めるだけにして、
	// 状態遷移やリアクションの判断は OnDamaged 側へ寄せる。
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon)) {
		auto* combo = player_->GetCombat()->GetCombo();

		DamageInfo info{};
		info.amount = static_cast<int>(combo->GetCurrentDamage());
		info.sourcePosition = player_->GetWorldPosition();
		info.knockbackPower = combo->GetCurrentKnockback();
		info.knockbackDuration = combo->GetCurrentKnockbackDuration();

		ApplyDamage(info);

		// 攻撃ヒット時の前進ステップ（プレイヤー側の手応え演出）は
		// 敵が死んでいても出したいので、リアクションとは別に発火する。
		combo->OnHitStep(wt_.translate_);
	}

	// プレイヤー本体または盾に当たった時。攻撃実行中のStateの時だけ攻撃を成立させる。
	//
	// 盾も本体と同じ扱いにするのは、ガード中は盾が敵を押し返して
	// 体同士が接触しないことがあり、そのままだとガード判定自体が走らないため。
	// どちらに当たっても TryPerformContactAttack() が攻撃判定時間ごとに
	// 一度だけ通すので、二重に成立することはない。
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer) ||
		other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield)) {
		TryPerformContactAttack();
	}
}

/*==========================================================================
被弾リアクション（ダメージが実際に通ったときだけ呼ばれる）
//========================================================================*/
void BattleEnemy::OnDamaged(const DamageInfo& info) {
	// ダウン中などはダメージを受けても、その反撃チャンス状態を維持する。
	const bool keepCurrentState = currentState_ && currentState_->KeepsStateWhenDamaged();

	// --------------------- ダメージ数値UI --------------------- //
	// 頭上Yオフセットは DamageNumberManager 側でJSON調整
	if (player_) {
		const bool isSine = (player_->GetCombat()->GetComboDamageMultiplier() > 1.0f);
		DamageNumberManager::GetInstance()->SpawnDamage(info.amount, wt_.translate_, isSine);
	}

	// --------------------- 連続被弾からのカウンター --------------------- //
	// しきい値・有効フラグは CounterAttackParams (JSON 経由) で調整可能
	if (!keepCurrentState) {
		const auto& counterParams = enemyData_.attackParams.counter;
		consecutiveHitCount_++;
		hitCountResetTimer_ = 0.0f;

		if (counterParams.enabled && consecutiveHitCount_ >= counterParams.triggerHitCount) {
			ChangeState(std::make_unique<BattleRecoveryState>());
			consecutiveHitCount_ = 0;
		} else {
			ChangeState(std::make_unique<BattleDamageState>());
		}
	}

	// --------------------- ノックバックとのけぞり --------------------- //
	Vector3 knockbackDir = wt_.translate_ - info.sourcePosition;
	knockbackDir.y = 0.0f;
	if (Length(knockbackDir) > 0.001f) {
		knockbackDir = Vector3::Normalize(knockbackDir);
		StartKnockback(knockbackDir, info.knockbackPower, info.knockbackDuration);
		StartDirectionalHitReaction(knockbackDir,
			enemyData_.damageReaction.hitReactionAngle,
			enemyData_.damageReaction.hitReactionDuration);
	}
}

/*==========================================================================
ヒット中
//========================================================================*/
void BattleEnemy::OnCollision([[maybe_unused]] BaseCollider* self, [[maybe_unused]] BaseCollider* other) {
	if (!isAlive_) return;

	// 溜め中から接触し続けたまま攻撃判定時間へ入った場合にも、一度だけ命中させる。
	// 盾を本体と同じ扱いにする理由は OnEnterCollision 側のコメントを参照。
	if (other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayer) ||
		other->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield)) {
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
