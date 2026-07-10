#pragma once

#include "../TriggerAction.h"
#include "Vector3.h"

#include <memory>
#include <functional>
#include <string>
#include <utility>
#include <vector>

class OpenGateAction;

// ============================================================
// RuleTriggerAction
//   Condition + Action 型の汎用イベント。
//   conditions が全て満たされたら actions を一度だけ実行する。
//
//   初期対応:
//     Condition: EnemyDefeatCount
//     Action   : OpenGate / ActivateWaypoint / ClearWaypoint
// ============================================================
class RuleTriggerAction : public TriggerAction {
public:
	struct Condition {
		std::string type = "EnemyDefeatCount";
		std::string group;
		std::string name;
		int requiredCount = 1;
		int currentCount = 0;

		bool IsSatisfied() const;
		nlohmann::json SerializeToJson() const;
	};

	struct Command {
		std::string type = "OpenGate";
		std::string targetName;

		Vector3 openOffsetPosition = { 0.0f, 0.0f, 0.0f };
		Vector3 openOffsetRotationDeg = { 0.0f, 0.0f, 0.0f };
		Vector3 openOffsetScale = { 0.0f, 0.0f, 0.0f };
		float openDuration = 1.0f;

		nlohmann::json SerializeToJson() const;
	};

	void Update(float deltaTime) override;
	void NotifyEnemyDefeated(const std::string& group) override;

	nlohmann::json SerializeToJson() const override;
	std::string GetTypeName() const override { return "Rule"; }
	bool NeedsSpatialPlacement() const override { return false; }

	void AddCondition(const Condition& condition) { conditions_.push_back(condition); }
	void AddCommand(const Command& command) { commands_.push_back(command); }
	void SetOnGateOpened(std::function<void()> callback) { onGateOpened_ = std::move(callback); }

	std::vector<Condition>& GetConditions() { return conditions_; }
	const std::vector<Condition>& GetConditions() const { return conditions_; }
	std::vector<Command>& GetCommands() { return commands_; }
	const std::vector<Command>& GetCommands() const { return commands_; }

	bool HasFired() const { return fired_; }
	void ResetRuntime();

private:
	void TryFire();
	void ExecuteCommand(const Command& command);

	std::vector<Condition> conditions_;
	std::vector<Command> commands_;
	std::vector<std::unique_ptr<OpenGateAction>> runningGates_;
	std::function<void()> onGateOpened_;
	bool fired_ = false;
};
