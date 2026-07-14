#include "MagicActionEditor.h"

#include "MagicActionDatabase.h"

#ifdef USE_IMGUI
#include "imgui.h"

#include <algorithm>
#include <cstring>

namespace {
	constexpr const char* kMagicActionPath = "Resources/Json/Magic/MagicActions.json";

	template <typename EnumType, size_t Count>
	bool DrawEnumCombo(const char* label, EnumType& value, const char* const (&names)[Count], EnumType(*fromString)(const std::string&))
	{
		int current = 0;
		for (int i = 0; i < static_cast<int>(Count); ++i) {
			if (std::string(ToString(value)) == names[i]) {
				current = i;
				break;
			}
		}

		if (!ImGui::Combo(label, &current, names, static_cast<int>(Count))) {
			return false;
		}

		value = fromString(names[current]);
		return true;
	}

	void DrawActionFields(MagicActionData& action)
	{
		char nameBuffer[128] = {};
		strncpy_s(nameBuffer, action.name.c_str(), sizeof(nameBuffer) - 1);
		if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
			action.name = nameBuffer;
		}

		const char* slots[] = { "Primary", "Secondary", "Utility" };
		DrawEnumCombo("スロット", action.slot, slots, PlayerMagicSlotFromString);

		const char* inputModes[] = { "Tap", "ChargeRelease" };
		DrawEnumCombo("入力方式", action.inputMode, inputModes, MagicInputModeFromString);

		ImGui::InputFloat("最小溜め時間", &action.minChargeTime, 0.05f, 0.2f);
		ImGui::InputFloat("最大溜め時間", &action.maxChargeTime, 0.05f, 0.2f);
		ImGui::InputFloat("クールダウン", &action.cooldown, 0.05f, 0.2f);
	}

	void DrawEventFields(MagicTimelineEvent& event)
	{
		ImGui::InputFloat("発火時間", &event.time, 0.05f, 0.2f);

		const char* triggers[] = { "OnStart", "OnTimeline", "OnRelease" };
		DrawEnumCombo("発火条件", event.trigger, triggers, MagicEventTriggerFromString);

		const char* types[] = { "Debug", "PlayVfx", "SpawnBeam", "SpawnProjectile", "SpawnArea", "StrikeTarget" };
		DrawEnumCombo("イベント種類", event.type, types, MagicEventTypeFromString);

		const char* elements[] = { "None", "Fire", "Thunder", "Ice", "Light" };
		DrawEnumCombo("属性", event.element, elements, MagicElementFromString);

		char labelBuffer[128] = {};
		strncpy_s(labelBuffer, event.label.c_str(), sizeof(labelBuffer) - 1);
		if (ImGui::InputText("ラベル", labelBuffer, sizeof(labelBuffer))) {
			event.label = labelBuffer;
		}

		ImGui::InputFloat("威力", &event.power, 0.1f, 1.0f);
		ImGui::InputFloat("持続", &event.duration, 0.05f, 0.2f);
		ImGui::InputFloat("範囲", &event.radius, 0.1f, 1.0f);
		ImGui::InputFloat("速度", &event.speed, 0.5f, 2.0f);
	}
}
#endif

void MagicActionEditor::Draw()
{
#ifdef USE_IMGUI
	auto& actions = MagicActionDatabase::Get();

	if (ImGui::Button("Reload")) {
		MagicActionDatabase::LoadFromFile(kMagicActionPath);
		selectedAction_ = 0;
		selectedEvent_ = 0;
	}
	ImGui::SameLine();
	if (ImGui::Button("Save")) {
		MagicActionDatabase::SaveToFile(kMagicActionPath);
	}
	ImGui::SameLine();
	if (ImGui::Button("Add Action")) {
		actions.push_back(MagicActionData{});
		selectedAction_ = static_cast<int>(actions.size()) - 1;
		selectedEvent_ = 0;
	}

	if (actions.empty()) return;

	selectedAction_ = std::clamp(selectedAction_, 0, static_cast<int>(actions.size()) - 1);

	if (ImGui::BeginListBox("魔法一覧", ImVec2(240.0f, 120.0f))) {
		for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
			const bool selected = selectedAction_ == i;
			if (ImGui::Selectable(actions[i].name.c_str(), selected)) {
				selectedAction_ = i;
				selectedEvent_ = 0;
			}
		}
		ImGui::EndListBox();
	}

	MagicActionData& action = actions[selectedAction_];
	DrawActionFields(action);

	ImGui::Separator();
	ImGui::Text("イベント");
	if (ImGui::Button("Add Event")) {
		action.events.push_back(MagicTimelineEvent{});
		selectedEvent_ = static_cast<int>(action.events.size()) - 1;
	}

	if (action.events.empty()) return;

	selectedEvent_ = std::clamp(selectedEvent_, 0, static_cast<int>(action.events.size()) - 1);
	if (ImGui::BeginListBox("イベント一覧", ImVec2(240.0f, 120.0f))) {
		for (int i = 0; i < static_cast<int>(action.events.size()); ++i) {
			const std::string label = std::to_string(i) + ": " + ToString(action.events[i].trigger) + " / " + ToString(action.events[i].type);
			const bool selected = selectedEvent_ == i;
			if (ImGui::Selectable(label.c_str(), selected)) {
				selectedEvent_ = i;
			}
		}
		ImGui::EndListBox();
	}

	DrawEventFields(action.events[selectedEvent_]);
#endif
}
