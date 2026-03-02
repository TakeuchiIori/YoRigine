#pragma once
#include "../StateMachine.h"
#include "../Combat/PlayerCombat.h"
#include "Collision/Core/CollisionDirection.h"

class HitCombatState : public IState<CombatState>
{
public:
    // 衝突方向を受け取るコンストラクタ
    HitCombatState(PlayerCombat* combat);
	~HitCombatState() = default;

    void OnEnter() override;
    void OnExit() override;
    void Update(float deltaTime) override;
    CombatState GetStateType() const override { return CombatState::Hit; }
private:
    PlayerCombat* combat_;
};

