#include "HitCombatState.h"
#include "../Player.h"

// ============================================================
// コンストラクタ
// ============================================================
HitCombatState::HitCombatState(PlayerCombat* combat)
    : combat_(combat)
{

}

// ============================================================
// 被弾ステート開始処理
// ============================================================
void HitCombatState::OnEnter()
{
    // ------------------------------------------------------------
    // 攻撃状態突入時の初期処理
    // ------------------------------------------------------------
    auto* player = combat_->GetOwner();
    auto* movement = player->GetMovement();
    auto* obj = player->GetObject3d();

    movement->SetCanMove(false);
    movement->SetCanRotate(false);
    movement->ForceStop();

    std::string motionName = "Impact2";

    // ------------------------------------------------------------
    // 衝突方向に応じたモーション選択
    // ------------------------------------------------------------
    switch (combat_->GetHitDirection())
    {
    case HitDirection::Front:
        break;
    case HitDirection::Back:
        motionName = "Impact3";
        break;
    case HitDirection::Left:
        break;
    case HitDirection::Right:
        break;
    }

    // ------------------------------------------------------------
    // モーション変更
    // ------------------------------------------------------------
    obj->SetMotionSpeed(player->GetMotionSpeed(0));
    obj->SetChangeMotion("Player.gltf", MotionPlayMode::Once, motionName);
}

// ============================================================
// 被弾ステート終了処理
// ============================================================
void HitCombatState::OnExit()
{
    auto* player = combat_->GetOwner();
    auto* movement = player->GetMovement();

    movement->SetCanMove(true);
    movement->SetCanRotate(true);

    // Movementステートに現在の状態に応じたアニメーション再生を委譲
    movement->SyncAnimationToCurrentState();
}

// ============================================================
// 更新処理
// ============================================================
void HitCombatState::Update([[maybe_unused]] float deltaTime)
{
    auto* player = combat_->GetOwner();
    auto* obj = player->GetObject3d();

    // ------------------------------------------------------------
    // アニメーションが終了しているかチェック
    // ------------------------------------------------------------
    if (obj->GetModel()->GetMotionSystem()->IsFinished()) {
        combat_->ChangeState(CombatState::Idle);
    }
}