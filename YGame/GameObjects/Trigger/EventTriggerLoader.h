#pragma once

#include "EventTrigger.h"
#include "Actions/OpenGateAction.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;

// ============================================================
// EventTriggerLoader
//   JSON ファイルから EventTrigger 群を読み込み、EventTrigger と
//   その内部 TriggerAction をまとめて生成する。
//
//   出力:
//     - outTriggers          : 所有権付きの EventTrigger 群
//     - outOpenGateActions   : OpenGateAction だけの弱参照リスト
//                              (FieldEnemyManager の撃破コールバックで dispatch する用)
//
//   onAnyGateOpened は各 OpenGateAction に attach される (RebakeNavGrid を呼ぶ想定)。
// ============================================================
class EventTriggerLoader {
public:
	static bool Load(
		const std::string& filePath,
		Camera* camera,
		std::function<void()> onAnyGateOpened,
		std::vector<std::unique_ptr<EventTrigger>>& outTriggers,
		std::vector<OpenGateAction*>& outOpenGateActions);

	// EventTrigger 群を JSON ファイルへ書き戻す。
	// triggers の所有権は奪わない (const 参照)。失敗時 false。
	static bool Save(
		const std::string& filePath,
		const std::vector<std::unique_ptr<EventTrigger>>& triggers);
};

namespace EventTriggerPaths {
	const std::string Field = "Resources/Json/EventTriggers/field_triggers.json";
}
