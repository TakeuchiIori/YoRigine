#include "MagicActionRunner.h"

#include <algorithm>

void MagicActionRunner::Start(const MagicActionData& action, Player* owner)
{
	if (cooldown_ > 0.0f) return;

	currentAction_ = action;
	for (auto& event : currentAction_.events) {
		event.fired = false;
	}

	owner_ = owner;
	running_ = true;
	released_ = false;
	elapsedTime_ = 0.0f;
	chargeTime_ = 0.0f;

	FireEvents(MagicEventTrigger::OnStart);

	if (currentAction_.inputMode == MagicInputMode::Tap) {
		Release();
	}
}

void MagicActionRunner::Update(float deltaTime)
{
	if (cooldown_ > 0.0f) {
		cooldown_ = std::max(0.0f, cooldown_ - deltaTime);
	}

	if (!running_) return;

	elapsedTime_ += deltaTime;
	if (!released_) {
		chargeTime_ = std::min(currentAction_.maxChargeTime, chargeTime_ + deltaTime);
	}

	FireTimelineEvents();
}

bool MagicActionRunner::Release()
{
	if (!running_ || released_) return false;
	if (chargeTime_ < currentAction_.minChargeTime) return false;

	released_ = true;
	FireEvents(MagicEventTrigger::OnRelease);
	cooldown_ = currentAction_.cooldown;
	running_ = false;
	return true;
}

void MagicActionRunner::Reset()
{
	running_ = false;
	released_ = false;
	elapsedTime_ = 0.0f;
	chargeTime_ = 0.0f;
	cooldown_ = 0.0f;
}

void MagicActionRunner::FireEvents(MagicEventTrigger trigger)
{
	MagicEventContext context{ owner_, elapsedTime_, chargeTime_ };
	for (auto& event : currentAction_.events) {
		if (event.trigger != trigger || event.fired) continue;
		executor_.Execute(event, context);
		event.fired = true;
	}
}

void MagicActionRunner::FireTimelineEvents()
{
	MagicEventContext context{ owner_, elapsedTime_, chargeTime_ };
	for (auto& event : currentAction_.events) {
		if (event.trigger != MagicEventTrigger::OnTimeline || event.fired) continue;
		if (elapsedTime_ < event.time) continue;
		executor_.Execute(event, context);
		event.fired = true;
	}
}
