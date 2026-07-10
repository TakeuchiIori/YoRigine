#ifdef USE_IMGUI
#include "EventTriggerEditor.h"

#include "Actions/OpenGateAction.h"
#include "Actions/RuleTriggerAction.h"
#include "Actions/WaypointAction.h"
#include "EventTrigger.h"
#include "WaypointManager.h"
#include "Object3D/ObjectManager.h"

#include <algorithm>
#include <cstdio>
#include "imgui.h"
#include <memory>
#include <string>
#include <utility>

namespace {

	void DrawOpenGateEditor(OpenGateAction& gate, const std::function<void()>& onGateOpened)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Action: OpenGate");

		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s", gate.GetTargetName().c_str());
			if (ImGui::InputText("ターゲット nameTag", buf, sizeof(buf))) {
				gate.SetTargetName(buf);
			}
		}
		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s", gate.GetRequiredGroup().c_str());
			if (ImGui::InputText("必要敵グループ (enemyId)", buf, sizeof(buf))) {
				gate.SetRequiredGroup(buf);
			}
			if (gate.GetRequiredGroup().empty()) {
				ImGui::SameLine();
				ImGui::TextDisabled("(空欄 = どの敵でもカウント)");
			}
		}
		{
			int n = gate.GetRequiredCount();
			if (ImGui::DragInt("必要撃破数", &n, 1.0f, 1, 999)) {
				gate.SetRequiredCount(n);
			}
		}

		ImGui::Separator();
		ImGui::TextDisabled("開放モーション (元 transform に対する差分)");
		{
			Vector3 v = gate.GetOpenOffsetPosition();
			if (ImGui::DragFloat3("オフセット位置", &v.x, 0.1f)) gate.SetOpenOffsetPosition(v);
		}
		{
			Vector3 v = gate.GetOpenOffsetRotationDeg();
			if (ImGui::DragFloat3("オフセット回転(deg)", &v.x, 1.0f)) gate.SetOpenOffsetRotationDeg(v);
		}
		{
			Vector3 v = gate.GetOpenOffsetScale();
			if (ImGui::DragFloat3("オフセットスケール", &v.x, 0.05f)) gate.SetOpenOffsetScale(v);
		}
		{
			float d = gate.GetOpenDuration();
			if (ImGui::DragFloat("補間時間 (秒)", &d, 0.05f, 0.05f, 60.0f)) gate.SetOpenDuration(d);
		}

		ImGui::Separator();
		ImGui::TextDisabled("ヒンジ式の扉なら、ターゲット PlacedObject 側でアンカーを設定してください (シーンエディタ → アンカー使用)");

		ImGui::Separator();
		ImGui::TextDisabled("閉 (初期) 位置: %s",
			gate.HasClosedPose() ? "記録済み" : "未記録 (初回起動時に target 現在位置を捕捉)");
		if (gate.HasClosedPose()) {
			ImGui::TextDisabled("  pos (%.2f, %.2f, %.2f)",
				gate.GetClosedPosition().x, gate.GetClosedPosition().y, gate.GetClosedPosition().z);
		}
		if (ImGui::Button("現在の target 位置を「閉」として記録")) {
			if (auto* om = ObjectManager::GetInstance()) {
				if (auto* tgt = om->GetObjectByName(gate.GetTargetName())) {
					gate.SetClosedPose(tgt->position, tgt->rotation, tgt->scale);
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("進捗: %d / %d (Phase: %d)",
			gate.GetCurrentCount(), gate.GetRequiredCount(),
			static_cast<int>(gate.GetPhase()));

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
		if (ImGui::Button(" プレビュー (即発火)")) {
			gate.SetOnGateOpened(onGateOpened);
			gate.TriggerPreview();
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Button("状態リセット")) {
			gate.Reset();
		}
	}

	void DrawWaypointEditor(WaypointAction& waypoint, const std::string& triggerName)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Action: Waypoint");

		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s", waypoint.GetBeaconEffect().c_str());
			if (ImGui::InputText("ビーコンVfxMesh", buf, sizeof(buf))) {
				waypoint.SetBeaconEffect(buf);
			}
			if (waypoint.GetBeaconEffect().empty()) {
				ImGui::SameLine();
				ImGui::TextDisabled("(空欄 = ビーコンなし)");
			}
		}
		{
			float s = waypoint.GetBeaconScale();
			if (ImGui::DragFloat("ビーコンスケール", &s, 0.05f, 0.01f, 100.0f, "%.2f")) {
				waypoint.SetBeaconScale(s);
			}
		}
		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s", waypoint.GetRequiredGroup().c_str());
			if (ImGui::InputText("必要敵グループ (enemyId)", buf, sizeof(buf))) {
				waypoint.SetRequiredGroup(buf);
			}
			if (waypoint.GetRequiredGroup().empty()) {
				ImGui::SameLine();
				ImGui::TextDisabled("(空欄 = どの敵でもカウント)");
			}
		}
		{
			int n = waypoint.GetRequiredCount();
			if (ImGui::DragInt("必要撃破数", &n, 1.0f, 1, 999)) {
				waypoint.SetRequiredCount(n);
			}
		}
		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "%s", waypoint.GetNextWaypoint().c_str());
			if (ImGui::InputText("次のWaypoint名", buf, sizeof(buf))) {
				waypoint.SetNextWaypoint(buf);
			}
			if (waypoint.GetNextWaypoint().empty()) {
				ImGui::SameLine();
				ImGui::TextDisabled("(空欄 = 最終目的地)");
			}
		}
		{
			bool start = waypoint.IsStartActive();
			if (ImGui::Checkbox("最初の目的地", &start)) {
				waypoint.SetStartActive(start);
			}
		}

		ImGui::Text("状態: %s", waypoint.IsActive() ? "現在の目的地" : "待機");
		ImGui::SameLine();
		if (ImGui::Button("このWaypointを有効化")) {
			WaypointManager::GetInstance()->Activate(triggerName);
		}
		ImGui::SameLine();
		if (ImGui::Button("目的地クリア")) {
			WaypointManager::GetInstance()->Activate("");
		}
	}

	void DrawRuleEditor(RuleTriggerAction& rule)
	{
		ImGui::Separator();
		ImGui::TextDisabled("Action: Rule (Condition + Action)");

		ImGui::TextDisabled("Conditions");
		auto& conditions = rule.GetConditions();
		for (int ci = 0; ci < static_cast<int>(conditions.size()); ++ci) {
			ImGui::PushID(1000 + ci);
			auto& condition = conditions[ci];
			ImGui::Text("Condition %d: %s", ci, condition.type.c_str());
			{
				char buf[128];
				std::snprintf(buf, sizeof(buf), "%s", condition.group.c_str());
				if (ImGui::InputText("敵グループ", buf, sizeof(buf))) {
					condition.group = buf;
				}
				if (condition.group.empty()) {
					ImGui::SameLine();
					ImGui::TextDisabled("(空欄 = どの敵でもカウント)");
				}
			}
			ImGui::DragInt("必要数", &condition.requiredCount, 1.0f, 1, 999);
			ImGui::TextDisabled("進捗: %d / %d", condition.currentCount, condition.requiredCount);
			ImGui::Separator();
			ImGui::PopID();
		}
		if (ImGui::Button("Condition 追加")) {
			RuleTriggerAction::Condition condition;
			condition.type = "EnemyDefeatCount";
			condition.requiredCount = 1;
			rule.AddCondition(condition);
		}

		ImGui::Separator();
		ImGui::TextDisabled("Actions");
		auto& commands = rule.GetCommands();
		const char* actionTypes[] = { "OpenGate", "ActivateWaypoint", "ClearWaypoint" };
		for (int ai = 0; ai < static_cast<int>(commands.size()); ++ai) {
			ImGui::PushID(2000 + ai);
			auto& command = commands[ai];
			int typeIndex = 0;
			if (command.type == "ActivateWaypoint") typeIndex = 1;
			else if (command.type == "ClearWaypoint") typeIndex = 2;
			if (ImGui::Combo("実行Action", &typeIndex, actionTypes, IM_ARRAYSIZE(actionTypes))) {
				command.type = actionTypes[typeIndex];
			}

			if (command.type != "ClearWaypoint") {
				char buf[128];
				std::snprintf(buf, sizeof(buf), "%s", command.targetName.c_str());
				if (ImGui::InputText("対象名", buf, sizeof(buf))) {
					command.targetName = buf;
				}
			}

			if (command.type == "OpenGate") {
				ImGui::DragFloat3("オフセット位置", &command.openOffsetPosition.x, 0.1f);
				ImGui::DragFloat3("オフセット回転(deg)", &command.openOffsetRotationDeg.x, 1.0f);
				ImGui::DragFloat3("オフセットスケール", &command.openOffsetScale.x, 0.05f);
				ImGui::DragFloat("補間時間 (秒)", &command.openDuration, 0.05f, 0.05f, 60.0f);
			}
			ImGui::Separator();
			ImGui::PopID();
		}
		if (ImGui::Button("Action 追加")) {
			RuleTriggerAction::Command command;
			command.type = "OpenGate";
			command.openOffsetPosition = { 0.0f, 5.0f, 0.0f };
			rule.AddCommand(command);
		}

		ImGui::Separator();
		ImGui::Text("状態: %s", rule.HasFired() ? "実行済み" : "待機");
		ImGui::SameLine();
		if (ImGui::Button("状態リセット")) {
			rule.ResetRuntime();
		}
	}

} // namespace

void EventTriggerEditor::Draw(Context& context)
{
	if (!context.system) return;

	auto& eventTriggers = context.system->GetTriggers();
	auto& openGateActions = context.system->GetOpenGateActions();

	ImGui::TextDisabled("%s", context.filePath.c_str());
	ImGui::Separator();

	if (ImGui::Button("＋ OpenGate トリガー追加")) {
		auto trigger = std::make_unique<EventTrigger>();
		trigger->Initialize(context.camera);
		trigger->SetName("NewTrigger");

		auto action = std::make_unique<OpenGateAction>(std::string(""), std::string(""), 1);
		action->SetOnGateOpened(context.onGateOpened);

		openGateActions.push_back(action.get());
		trigger->SetAction(std::move(action));
		eventTriggers.push_back(std::move(trigger));
	}
	ImGui::SameLine();
	if (ImGui::Button("＋ Waypoint 追加")) {
		bool hasWaypoint = false;
		for (const auto& existing : eventTriggers) {
			if (existing && dynamic_cast<WaypointAction*>(existing->GetAction())) {
				hasWaypoint = true;
				break;
			}
		}

		auto trigger = std::make_unique<EventTrigger>();
		trigger->Initialize(context.camera);
		trigger->SetName("Waypoint" + std::to_string(eventTriggers.size() + 1));
		if (context.getPlacementPosition) {
			auto& wt = trigger->GetWT();
			wt.translate_ = context.getPlacementPosition();
			wt.scale_ = { 1.0f, 1.0f, 1.0f };
			wt.UpdateMatrix();
		}

		auto action = std::make_unique<WaypointAction>(
			std::string("WaypointBeacon"), std::string(""), 1, std::string(""), !hasWaypoint);

		trigger->SetAction(std::move(action));
		eventTriggers.push_back(std::move(trigger));
	}
	ImGui::SameLine();
	if (ImGui::Button("＋ Rule 追加")) {
		auto trigger = std::make_unique<EventTrigger>();
		trigger->Initialize(context.camera);
		trigger->SetName("Rule" + std::to_string(eventTriggers.size() + 1));

		auto action = std::make_unique<RuleTriggerAction>();
		action->SetOnGateOpened(context.onGateOpened);
		RuleTriggerAction::Condition condition;
		condition.type = "EnemyDefeatCount";
		condition.requiredCount = 1;
		action->AddCondition(condition);

		RuleTriggerAction::Command command;
		command.type = "OpenGate";
		command.openOffsetPosition = { 0.0f, 5.0f, 0.0f };
		action->AddCommand(command);

		trigger->SetAction(std::move(action));
		eventTriggers.push_back(std::move(trigger));
	}
	ImGui::SameLine();
	if (ImGui::Button("保存")) {
		context.system->Save(context.filePath);
	}
	ImGui::SameLine();
	if (ImGui::Button("再ロード")) {
		WaypointManager::GetInstance()->Reset();
		context.system->Load(context.filePath, context.camera, context.onGateOpened);
	}

	ImGui::Separator();
	ImGui::Text("登録数: %d", static_cast<int>(eventTriggers.size()));
	ImGui::Separator();

	int deleteIndex = -1;
	for (size_t i = 0; i < eventTriggers.size(); ++i) {
		auto& trig = eventTriggers[i];
		if (!trig) continue;

		ImGui::PushID(static_cast<int>(i));
		const std::string header = "[" + std::to_string(i) + "] " +
			(trig->GetName().empty() ? "(無名)" : trig->GetName());

		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			{
				char buf[128];
				std::snprintf(buf, sizeof(buf), "%s", trig->GetName().c_str());
				if (ImGui::InputText("名前", buf, sizeof(buf))) {
					trig->SetName(buf);
				}
			}

			TriggerAction* curAction = trig->GetAction();
			const bool needsSpatial = curAction ? curAction->NeedsSpatialPlacement() : true;
			if (needsSpatial) {
				auto& wt = trig->GetWT();
				bool xformChanged = false;
				if (ImGui::DragFloat3("位置", &wt.translate_.x, 0.1f)) xformChanged = true;
				if (ImGui::DragFloat3("回転(rad)", &wt.rotate_.x, 0.05f)) xformChanged = true;
				if (ImGui::DragFloat3("スケール", &wt.scale_.x, 0.1f, 0.1f, 100.0f)) xformChanged = true;
				if (xformChanged) wt.UpdateMatrix();
			}
			else {
				ImGui::TextDisabled("(このアクションは位置情報を使いません)");
			}

			if (auto* gate = dynamic_cast<OpenGateAction*>(trig->GetAction())) {
				DrawOpenGateEditor(*gate, context.onGateOpened);
			}
			else if (auto* waypoint = dynamic_cast<WaypointAction*>(trig->GetAction())) {
				DrawWaypointEditor(*waypoint, trig->GetName());
			}
			else if (auto* rule = dynamic_cast<RuleTriggerAction*>(trig->GetAction())) {
				DrawRuleEditor(*rule);
			}
			else if (trig->GetAction()) {
				ImGui::TextDisabled("Action: %s (未対応のエディタ)",
					trig->GetAction()->GetTypeName().c_str());
			}

			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("このトリガーを削除")) {
				deleteIndex = static_cast<int>(i);
			}
			ImGui::PopStyleColor();
		}

		ImGui::PopID();
	}

	if (deleteIndex >= 0) {
		auto* doomedAction = eventTriggers[deleteIndex]->GetAction();
		openGateActions.erase(
			std::remove(openGateActions.begin(), openGateActions.end(), doomedAction),
			openGateActions.end());
		eventTriggers.erase(eventTriggers.begin() + deleteIndex);
	}
}

#endif
