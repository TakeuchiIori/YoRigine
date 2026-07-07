#pragma once

#include "EventTrigger.h"
#include "Actions/OpenGateAction.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;

// ============================================================
// EventTriggerSystem
//   EventTrigger 群の所有・ロード/保存・更新・通知配送を担当する。
//   Scene 側は「いつ通知するか」だけを知り、各ギミックの条件判定は
//   TriggerAction / RuleTriggerAction に閉じ込める。
// ============================================================
class EventTriggerSystem {
public:
	void Clear();

	bool Load(const std::string& filePath,
	          Camera* camera,
	          std::function<void()> onAnyGateOpened);
	bool Save(const std::string& filePath) const;

	void Update();
	void DrawCollision();
	void NotifyEnemyDefeated(const std::string& group);

	std::vector<std::unique_ptr<EventTrigger>>& GetTriggers() { return triggers_; }
	const std::vector<std::unique_ptr<EventTrigger>>& GetTriggers() const { return triggers_; }
	std::vector<OpenGateAction*>& GetOpenGateActions() { return openGateActions_; }

private:
	std::vector<std::unique_ptr<EventTrigger>> triggers_;
	std::vector<OpenGateAction*> openGateActions_;
};
