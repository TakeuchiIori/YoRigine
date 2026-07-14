#pragma once

#include "MagicActionData.h"
#include "MagicEventExecutor.h"

class MagicActionRunner {
public:
	void Start(const MagicActionData& action, Player* owner);
	void Update(float deltaTime);
	void Release();
	void Reset();

	bool IsRunning() const { return running_; }
	float GetCooldown() const { return cooldown_; }
	const std::string& GetCurrentActionName() const { return currentAction_.name; }

private:
	void FireEvents(MagicEventTrigger trigger);
	void FireTimelineEvents();

private:
	MagicActionData currentAction_;
	MagicEventExecutor executor_;
	Player* owner_ = nullptr;
	bool running_ = false;
	bool released_ = false;
	float elapsedTime_ = 0.0f;
	float chargeTime_ = 0.0f;
	float cooldown_ = 0.0f;
};
