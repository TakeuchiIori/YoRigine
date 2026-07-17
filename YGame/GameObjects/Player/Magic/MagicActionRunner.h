#pragma once

#include "MagicActionData.h"
#include "MagicEventExecutor.h"
#include "Particle/EffectHandle.h"

class MagicActionRunner {
public:
	void Start(const MagicActionData& action, Player* owner);
	void Update(float deltaTime);
	bool Release();
	void Reset();

	bool IsRunning() const { return running_; }
	float GetCooldown() const { return cooldown_; }
	float GetChargeTime() const { return chargeTime_; }
	const std::string& GetCurrentActionName() const { return currentAction_.name; }

private:
	void FireEvents(MagicEventTrigger trigger);
	void FireTimelineEvents();
	void StopChargeVfx();

private:
	MagicActionData currentAction_;
	MagicEventExecutor executor_;
	Player* owner_ = nullptr;
	bool running_ = false;
	bool released_ = false;
	float elapsedTime_ = 0.0f;
	float chargeTime_ = 0.0f;
	float cooldown_ = 0.0f;

	// チャージ中の手元ループVFX（ChargeRelease 専用）。
	// 長押し中は詠唱原点に追従し、maxChargeTime 到達で chargeMaxVfx を一度だけ出す。
	EffectHandle chargeVfx_;
	bool chargeMaxFired_ = false;
};
