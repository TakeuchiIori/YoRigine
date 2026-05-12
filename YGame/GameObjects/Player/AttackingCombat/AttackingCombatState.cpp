#include "AttackingCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"

// ============================================================
// コンストラクタ
// コンボシステムの各種コールバックを設定する
// ============================================================
AttackingCombatState::AttackingCombatState(PlayerCombat* combat) : combat_(combat) {
	auto* combo = combat_->GetCombo();
	auto* player = combat_->GetOwner();

	// ------------------------------------------------------------
	// 攻撃開始時のコールバック設定
	// ------------------------------------------------------------
	combo->SetAttackStartCallback([combat, player, this](const AttackData& attack) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		// 攻撃開始時にステート側のタイマーをリセット
		stateTimer_ = 0.0f;

		auto* movement = player->GetMovement();
		movement->SetCanMove(false);
		movement->SetCanRotate(false);
		movement->ForceStop();

		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(attack.motionSpeed);
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);

		// 攻撃タイプに応じたエフェクトの読み込み
		if (attack.type == AttackType::A_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Vfx/NewEffect.json");
		}
		else if (attack.type == AttackType::B_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Vfx/NewEffect2.json");
		}

		player->GetSword()->PlayTrail();
		combat->NotifyAction("コンボ開始: " + attack.animationName);
		});

	// ------------------------------------------------------------
	// コンボ継続時のコールバック設定
	// ------------------------------------------------------------
	combo->SetAttackContinueCallback([combat, player, this](const AttackData& attack) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		// 連続攻撃時にステート側のタイマーをリセットし、アニメーションを即座に上書きする
		stateTimer_ = 0.0f;

		auto* movement = player->GetMovement();
		movement->SetCanMove(false);
		movement->SetCanRotate(false);

		auto* obj = player->GetObject3d();
		obj->SetMotionSpeed(attack.motionSpeed);
		obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);

		// 攻撃タイプに応じたエフェクトの読み込み
		if (attack.type == AttackType::A_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Vfx/NewEffect.json");
		}
		else if (attack.type == AttackType::B_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Vfx/NewEffect2.json");
		}
		player->GetSword()->PlayTrail();
		combat->NotifyAction("コンボ継続: " + attack.animationName);
		});

	// ------------------------------------------------------------
	// コンボ終了時（リセット時含む）のコールバック設定
	// ------------------------------------------------------------
	combo->SetComboEndCallback([combat, player]([[maybe_unused]] int finalCount) {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->NotifyAction("コンボ終了");
		});

	combo->SetComboResetCallback([combat]() {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->NotifyAction("コンボリセット");
		});

	// ------------------------------------------------------------
	// CC変化時・剣コライダー制御のコールバック
	// ------------------------------------------------------------
	combo->SetCCChangeCallback([]([[maybe_unused]] int oldCC, [[maybe_unused]] int newCC) {});

	combo->SetSwordColliderCallback([player](bool isActive) {
		player->GetSword()->SetEnableCollider(isActive);
		});
}

// ============================================================
// ステート開始処理
// ============================================================
void AttackingCombatState::OnEnter() {
	auto* player = combat_->GetOwner();
	auto* movement = player->GetMovement();

	movement->SetCanMove(false);
	movement->SetCanRotate(false);
	movement->ForceStop();

	stateTimer_ = 0.0f;
}

// ============================================================
// ステート終了処理
// （攻撃が終わってIdleに戻る、または被弾して中断される時など）
// ============================================================
void AttackingCombatState::OnExit() {
	auto* player = combat_->GetOwner();
	auto* movement = player->GetMovement();

	// 動きの制限を解除
	movement->SetCanMove(true);
	movement->SetCanRotate(true);

	// エフェクトと当たり判定を確実にオフにする
	player->GetSword()->StopTrail();
	player->GetSword()->SetEnableCollider(false);
	player->GetObject3d()->SetMotionSpeed(player->GetMotionSpeed(0));

	// コンボ側へ「攻撃アクションが終了した」ことを通知する
	combat_->GetCombo()->OnAttackFinished();
}

// ============================================================
// 更新処理
// ============================================================
void AttackingCombatState::Update([[maybe_unused]] float deltaTime) {
	auto* combo = combat_->GetCombo();
	auto* player = combat_->GetOwner();
	const AttackData* currentAttack = combo->GetCurrentAttack();

	// 攻撃データが取得できない場合は安全のため即座にIdleへ戻る
	if (!currentAttack) {
		combat_->ChangeState(CombatState::Idle);
		return;
	}

	stateTimer_ += deltaTime;

	// ------------------------------------------------------------
	// フレームの計算
	// ------------------------------------------------------------
	const float frameDuration = (currentAttack->fps > 0) ? 1.0f / static_cast<float>(currentAttack->fps) : 1.0f / 60.0f;
	const int currentFrame = static_cast<int>(stateTimer_ / frameDuration);

	// ------------------------------------------------------------
	// 当たり判定(Hitbox)のON/OFF
	// ------------------------------------------------------------
	const bool inHitWindow = (currentAttack->hitEnd > currentAttack->hitStart) &&
		(currentFrame >= currentAttack->hitStart) &&
		(currentFrame < currentAttack->hitEnd);
	player->GetSword()->SetEnableCollider(inHitWindow);

	// ------------------------------------------------------------
	// 次の攻撃の先行入力（キャンセル）受付
	// ------------------------------------------------------------
	if (currentFrame >= currentAttack->comboWindowStart && currentFrame <= currentAttack->comboWindowEnd) {
		if (player->IsAttackPressedA()) {
			if (combat_->TryAttack(AttackType::A_Arte)) return;
		}
		else if (player->IsAttackPressedB()) {
			if (combat_->TryAttack(AttackType::B_Arte)) return;
		}
	}

	// ------------------------------------------------------------
	// アニメーション終了判定（硬直なしで即座に動けるようにIdleへ）
	// ------------------------------------------------------------
	if (stateTimer_ >= currentAttack->duration) {
		combat_->ChangeState(CombatState::Idle);
	}
}