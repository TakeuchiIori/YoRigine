#include "AttackingCombatState.h"
#include "../Player.h"
#include "../Movement/PlayerMovement.h"
#include "Model.h"
#include "Systems/GameTime/GameTime.h"

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

		stateTimer_ = 0.0f;

		// ★移動制限(SetCanMove(false)など)は行わず、PlayerMovementの下半身の動きを活かす

		auto* obj = player->GetObject3d();


		// 追加した関数で上半身だけ攻撃アニメーションを再生
		obj->PlayUpperMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);
		obj->SetUpperMotionSpeed(attack.motionSpeed);

		// 攻撃タイプに応じたエフェクトの読み込み
		if (attack.type == AttackType::A_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Json/VfxMesh/NewEffect2.json");
		}
		else if (attack.type == AttackType::B_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Json/VfxMesh/NewEffect3.json");
		}

		player->GetSword()->PlayTrail();
		combat->NotifyAction("コンボ開始: " + attack.animationName);
		});

	// ------------------------------------------------------------
	// 攻撃ヒット時のコールバック設定
	// AttackData 駆動でカメラワーク・ヒットストップ・シェイクを発火する。
	// PlayerSword はヒット検出と通知だけを担当し、演出はすべてここで集約する。
	// ------------------------------------------------------------
	combo->SetAttackHitCallback([combat, player](const AttackData& attack, const AttackHitContext& ctx) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		// カメラワークは振りごとに最初のヒットでだけ発火する。
		// （複数敵を貫いた場合や同一振り内の再ヒットで暴発させない）
		if (ctx.hitIndex == 1 && !attack.playCameraWorkName.empty()) {
			player->GetPlayerCamera()->PlayAttackCameraWork(attack.playCameraWorkName);
		}

		// ヒットストップとシェイクは AttackData の値が 0 のときはスキップ。
		if (attack.hitStopDuration > 0.0f) {
			YoRigine::GameTime::SetHitStop(attack.hitStopDuration);
		}
		if (attack.shakeIntensity > 0.0f && attack.shakeDuration > 0.0f) {
			if (player->GetFollowCamera()) {
				player->GetFollowCamera()->StartShake(attack.shakeIntensity, attack.shakeDuration);
			}
		}
		});

	// ------------------------------------------------------------
	// コンボ継続時のコールバック設定
	// ------------------------------------------------------------
	combo->SetAttackContinueCallback([combat, player, this](const AttackData& attack) {
		if (combat->GetCurrentState() == CombatState::Dead) return;

		stateTimer_ = 0.0f;

		auto* obj = player->GetObject3d();

		// 上半身アニメーションの再生
		obj->PlayUpperMotion("Player.gltf", MotionPlayMode::Once, attack.animationName);
		obj->SetUpperMotionSpeed(attack.motionSpeed);

		if (attack.type == AttackType::A_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Json/VfxMesh/NewEffect2.json");
		}
		else if (attack.type == AttackType::B_Arte) {
			player->GetSword()->LoadVfxAssets("Resources/Json/VfxMesh/NewEffect3.json");
		}
		player->GetSword()->PlayTrail();
		combat->NotifyAction("コンボ継続: " + attack.animationName);
		});

	// ------------------------------------------------------------
	// コンボ終了時のコールバック設定
	// ------------------------------------------------------------
	combo->SetComboEndCallback([combat, player]([[maybe_unused]] int finalCount) {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->NotifyAction("コンボ終了");
		});

	combo->SetComboResetCallback([combat]() {
		if (combat->GetCurrentState() == CombatState::Dead) return;
		combat->NotifyAction("コンボリセット");
		});

	combo->SetCCChangeCallback([]([[maybe_unused]] int oldCC, [[maybe_unused]] int newCC) {});

	combo->SetSwordColliderCallback([player](bool isActive) {
		player->GetSword()->SetEnableCollider(isActive);
		});
}

// ============================================================
// ステート開始処理
// ============================================================
void AttackingCombatState::OnEnter() {
	stateTimer_ = 0.0f;
}

// ============================================================
// ステート終了処理
// ============================================================
void AttackingCombatState::OnExit() {
	auto* player = combat_->GetOwner();

	// 状態終了時に上半身の攻撃アニメーションを確実に停止する
	auto* model = player->GetObject3d()->GetModel();
	if (model && model->GetMotionSystem()) {
		model->GetMotionSystem()->StopUpperAnimation(0.15f);
	}

	player->GetSword()->StopTrail();
	player->GetSword()->SetEnableCollider(false);
	player->GetObject3d()->SetMotionSpeed(player->GetMotionSpeed(0));

	// 被弾・スタンなどで攻撃が中断された場合に古い入力で次が暴発しないよう破棄
	combat_->ClearBufferedAttack();

	combat_->GetCombo()->OnAttackFinished();

	// Movementステートに現在の状態に応じたアニメーション再生を委譲
	player->GetMovement()->SyncAnimationToCurrentState();
}

// ============================================================
// 更新処理
// ============================================================
void AttackingCombatState::Update([[maybe_unused]] float deltaTime) {
	auto* combo = combat_->GetCombo();
	auto* player = combat_->GetOwner();
	const AttackData* currentAttack = combo->GetCurrentAttack();

	if (!currentAttack) {
		combat_->ChangeState(CombatState::Idle);
		return;
	}

	stateTimer_ += deltaTime;

	// 1. フレームの計算
	const float frameDuration = (currentAttack->fps > 0) ? 1.0f / static_cast<float>(currentAttack->fps) : 1.0f / 60.0f;
	const int currentFrame = static_cast<int>(stateTimer_ / frameDuration);

	// 2. 当たり判定(Hitbox)のON/OFF
	const bool inHitWindow = (currentAttack->hitEnd > currentAttack->hitStart) &&
		(currentFrame >= currentAttack->hitStart) &&
		(currentFrame < currentAttack->hitEnd);
	player->GetSword()->SetEnableCollider(inHitWindow);

	// 3. 先行入力受付：inputBufferStart 〜 comboWindowEnd の間に押された入力をバッファに積む
	//    （ここでは発火しない。現在の攻撃モーションは最後まで再生する）
	const bool inInputWindow =
		currentFrame >= currentAttack->inputBufferStart &&
		currentFrame <= currentAttack->comboWindowEnd;

	if (inInputWindow) {
		if (player->IsAttackPressedA()) {
			combat_->BufferAttack(AttackType::A_Arte);
		}
		else if (player->IsAttackPressedB()) {
			combat_->BufferAttack(AttackType::B_Arte);
		}
	}

	// 4. アニメーション終了：バッファされた先行入力があれば即時に次の攻撃へ、なければ Idle に戻る
	if (stateTimer_ >= currentAttack->duration) {
		if (combat_->HasBufferedAttack()) {
			AttackType next = combat_->PopBufferedAttack();
			if (combat_->TryAttack(next)) return;
		}
		combat_->ChangeState(CombatState::Idle);
	}
}