#include "MagicActionEditor.h"

#include "MagicActionDatabase.h"
#include "Vfx/VfxMesh/Runtime/VfxMeshSpawner.h"
#include "Particle/YEmitterGroupManager.h"
#include "Particle/YParticleManager.h"
#include "GPUParticle/GpuEmitManager.h"
#include "Composite/CompositeEffectManager.h"

#ifdef USE_IMGUI
#include <Debugger/Logger.h>
#include "imgui.h"
#include "Debugger/CurveEditor/CurveDelegate.h"
#include <ImCurveEdit.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
	constexpr const char* kMagicActionPath = "Resources/Json/Magic/MagicActions.json";
	std::vector<std::string> g_PlayerAnimationNames;
	bool g_PlayerAnimationListLoaded = false;

	std::string ToAnimationNameFromBinaryFile(const std::string& filename)
	{
		std::filesystem::path path(filename);
		std::string stem = path.stem().string();

		const std::string prefix = "Player_";
		if (stem.rfind(prefix, 0) == 0) {
			stem.erase(0, prefix.size());
		}

		return stem;
	}

	void RefreshPlayerAnimationList()
	{
		g_PlayerAnimationNames.clear();
		g_PlayerAnimationListLoaded = true;

		const std::filesystem::path binaryDir = "Resources/Binary";
		if (!std::filesystem::exists(binaryDir)) {
			Logger("[MagicActionEditor] Player animation binary directory not found: Resources/Binary\n");
			return;
		}

		try {
			for (const auto& entry : std::filesystem::directory_iterator(binaryDir)) {
				if (!entry.is_regular_file()) continue;

				const std::filesystem::path path = entry.path();
				if (path.extension() != ".anim") continue;

				const std::string filename = path.filename().string();
				if (filename.rfind("Player_", 0) != 0) continue;

				std::string animationName = ToAnimationNameFromBinaryFile(filename);
				if (!animationName.empty()) {
					g_PlayerAnimationNames.push_back(animationName);
				}
			}
		}
		catch (const std::exception& e) {
			Logger(("[MagicActionEditor] Failed to scan player animations: " + std::string(e.what()) + "\n").c_str());
			return;
		}

		std::sort(g_PlayerAnimationNames.begin(), g_PlayerAnimationNames.end());
		g_PlayerAnimationNames.erase(
			std::unique(g_PlayerAnimationNames.begin(), g_PlayerAnimationNames.end()),
			g_PlayerAnimationNames.end());

		Logger(("[MagicActionEditor] Player animations: " + std::to_string(g_PlayerAnimationNames.size()) + "\n").c_str());
	}

	void EnsurePlayerAnimationList()
	{
		if (!g_PlayerAnimationListLoaded) {
			RefreshPlayerAnimationList();
		}
	}

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

	void DrawAnimationSelector(MagicActionData& action)
	{
		EnsurePlayerAnimationList();

		const auto it = std::find(g_PlayerAnimationNames.begin(), g_PlayerAnimationNames.end(), action.animationName);
		const bool found = it != g_PlayerAnimationNames.end();
		const char* preview = action.animationName.empty()
			? "(未設定)"
			: action.animationName.c_str();

		// 攻撃エディタと同じく、実在する Player_*.anim から選ばせる。
		// 魔法側で手入力を許すと、JSON名の揺れが実行時の無音失敗になるためここで入口を絞る。
		if (ImGui::BeginCombo("アニメーション", preview)) {
			if (ImGui::Selectable("(未設定)", action.animationName.empty())) {
				action.animationName.clear();
			}

			for (const std::string& name : g_PlayerAnimationNames) {
				const bool selected = (action.animationName == name);
				if (ImGui::Selectable(name.c_str(), selected)) {
					action.animationName = name;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::SmallButton("更新##MagicAnimList")) {
			RefreshPlayerAnimationList();
		}

		if (!action.animationName.empty() && !found) {
			ImGui::TextColored(
				ImVec4(1.0f, 0.55f, 0.25f, 1.0f),
				"未検出: Resources/Binary/Player_%s.anim",
				action.animationName.c_str());
		}
	}

	const char* SlotButtonLabel(PlayerMagicSlot slot);

	void DrawActionFields(MagicActionData& action)
	{
		char nameBuffer[128] = {};
		strncpy_s(nameBuffer, action.name.c_str(), sizeof(nameBuffer) - 1);
		if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
			action.name = nameBuffer;
		}

		const char* slots[] = { "Primary", "Secondary", "Utility" };
		DrawEnumCombo("スロット", action.slot, slots, PlayerMagicSlotFromString);
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(0.35f, 0.8f, 1.0f, 1.0f), "→ %s", SlotButtonLabel(action.slot));

		const char* inputModes[] = { "Tap", "ChargeRelease" };
		DrawEnumCombo("入力方式", action.inputMode, inputModes, MagicInputModeFromString);

		DrawAnimationSelector(action);

		const char* trajectories[] = { "Forward", "LockOnOrForward", "StrikeTarget" };
		DrawEnumCombo("軌道", action.trajectoryType, trajectories, MagicTrajectoryTypeFromString);

		const char* hitShapes[] = { "Sphere", "AABB" };
		DrawEnumCombo("判定形状", action.hitShape, hitShapes, MagicHitShapeFromString);

		ImGui::InputFloat("再生時間", &action.duration, 0.05f, 0.2f);
		ImGui::InputFloat("射程", &action.range, 0.5f, 2.0f);
		ImGui::InputFloat("威力", &action.damage, 1.0f, 5.0f);

		ImGui::SeparatorText("当たり判定");
		ImGui::DragFloat3("中心オフセット", &action.hitOffset.x, 0.05f, -50.0f, 50.0f, "%.2f");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("魔法本体の現在位置から判定中心をずらします。");
		}
		if (action.hitShape == MagicHitShape::Sphere) {
			ImGui::DragFloat("球の半径", &action.hitRadius, 0.05f, 0.01f, 50.0f, "%.2f");
			action.hitRadius = std::max(0.01f, action.hitRadius);
		} else {
			ImGui::DragFloat3("AABB 半サイズ XYZ", &action.hitHalfExtents.x,
				0.05f, 0.01f, 50.0f, "%.2f");
			action.hitHalfExtents.x = std::max(0.01f, action.hitHalfExtents.x);
			action.hitHalfExtents.y = std::max(0.01f, action.hitHalfExtents.y);
			action.hitHalfExtents.z = std::max(0.01f, action.hitHalfExtents.z);
			ImGui::TextDisabled("表示値は全幅ではなく、中心から各面までの距離です。");
			if (ImGui::Button("細いビーム")) {
				action.hitHalfExtents = { 0.75f, 0.75f, 6.0f };
			}
			ImGui::SameLine();
			if (ImGui::Button("太いビーム")) {
				action.hitHalfExtents = { 1.8f, 1.8f, 8.0f };
			}
			ImGui::SameLine();
			if (ImGui::Button("広範囲")) {
				action.hitHalfExtents = { 5.0f, 2.0f, 5.0f };
			}
		}
		ImGui::InputFloat("判定開始遅延", &action.hitDelay, 0.01f, 0.05f);
		ImGui::InputFloat("再ヒット間隔", &action.hitInterval, 0.05f, 0.2f);
		if (action.hitInterval <= 0.0f) {
			ImGui::SameLine();
			ImGui::TextDisabled("(0=単発)");
		}
		ImGui::InputFloat("最小溜め時間", &action.minChargeTime, 0.05f, 0.2f);
		ImGui::InputFloat("最大溜め時間", &action.maxChargeTime, 0.05f, 0.2f);
		ImGui::InputFloat("クールダウン", &action.cooldown, 0.05f, 0.2f);
		ImGui::InputFloat("コンボ維持時間", &action.chainResetTime, 0.05f, 0.2f);
		ImGui::SeparatorText("ヒット時の手応え");
		ImGui::InputFloat("ヒットストップ", &action.hitStopDuration, 0.01f, 0.05f);
		ImGui::InputFloat("シェイク強度", &action.shakeIntensity, 0.05f, 0.2f);
		ImGui::InputFloat("シェイク時間", &action.shakeDuration, 0.05f, 0.2f);

		ImGui::SeparatorText("飛道弾の見た目 (判定本体に追従)");
		char travelBuffer[128] = {};
		strncpy_s(travelBuffer, action.travelVfx.c_str(), sizeof(travelBuffer) - 1);
		if (ImGui::InputText("飛翔VFX", travelBuffer, sizeof(travelBuffer))) {
			action.travelVfx = travelBuffer;
		}
		char impactBuffer[128] = {};
		strncpy_s(impactBuffer, action.impactVfx.c_str(), sizeof(impactBuffer) - 1);
		if (ImGui::InputText("着弾VFX", impactBuffer, sizeof(impactBuffer))) {
			action.impactVfx = impactBuffer;
		}
		ImGui::Checkbox("命中で消滅 (貫通しない)", &action.destroyOnHit);
	}

	void DrawScaleCurve(MagicActionData& action)
	{
		if (!ImGui::CollapsingHeader("スケールカーブ", ImGuiTreeNodeFlags_DefaultOpen)) return;

		action.scaleCurve.SetName("Scale");
		action.scaleCurve.SetViewRange(0.0f, 3.0f);

		CurveDelegate delegate;
		delegate.AddChannel(&action.scaleCurve, 0xFF55AAFF, "Scale");
		delegate.SetViewRange(ImVec2(0.0f, 0.0f), ImVec2(1.0f, 3.0f));
		delegate.SyncAll();
		const float width = std::max(260.0f, ImGui::GetContentRegionAvail().x);
		ImCurveEdit::Edit(delegate, ImVec2(width, 180.0f), ImGui::GetID("##MagicScaleCurve"));

		if (ImGui::SmallButton("一定 1.0")) {
			action.scaleCurve.Reset(1.0f);
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("膨張")) {
			action.scaleCurve.Clear();
			action.scaleCurve.AddKey(0.0f, 0.2f);
			action.scaleCurve.AddKey(0.45f, 1.6f);
			action.scaleCurve.AddKey(1.0f, 0.4f);
		}

		if (ImGui::TreeNode("キー一覧")) {
			auto& keys = action.scaleCurve.GetKeys();
			for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
				ImGui::PushID(i);
				ImGui::SetNextItemWidth(80.0f);
				ImGui::DragFloat("time", &keys[i].time, 0.01f, 0.0f, 1.0f, "%.2f");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(80.0f);
				ImGui::DragFloat("scale", &keys[i].value, 0.05f, 0.0f, 5.0f, "%.2f");
				ImGui::SameLine();
				int mode = static_cast<int>(keys[i].interpMode);
				const char* modes[] = { "Step", "Linear", "CatmullRom", "Bezier" };
				ImGui::SetNextItemWidth(110.0f);
				if (ImGui::Combo("補間", &mode, modes, IM_ARRAYSIZE(modes))) {
					keys[i].interpMode = static_cast<InterpolationMode>(mode);
				}
				ImGui::PopID();
			}
			if (ImGui::Button("キー追加")) {
				action.scaleCurve.AddKey(0.5f, 1.0f);
			}
			action.scaleCurve.SortByTime();
			ImGui::TreePop();
		}
	}

	// 入力の対応は Player::HandleMagicInput と一致させる:
	//   A → Primary / B → Secondary / X → Utility
	const char* SlotButtonLabel(PlayerMagicSlot slot)
	{
		switch (slot) {
		case PlayerMagicSlot::Primary: return "Aボタン";
		case PlayerMagicSlot::Secondary: return "Bボタン";
		case PlayerMagicSlot::Utility: return "Xボタン";
		default: return "?";
		}
	}

	const char* SlotLabel(PlayerMagicSlot slot)
	{
		switch (slot) {
		case PlayerMagicSlot::Primary: return "Aボタン (Primary)";
		case PlayerMagicSlot::Secondary: return "Bボタン (Secondary)";
		case PlayerMagicSlot::Utility: return "Xボタン (Utility)";
		default: return "Primary";
		}
	}

	void DrawEventFields(MagicTimelineEvent& event);

	void DrawToolbar(std::vector<MagicActionData>& actions, int& selectedAction, int& selectedEvent)
	{
		if (ImGui::Button("保存")) {
			MagicActionDatabase::SaveToFile(kMagicActionPath);
		}
		ImGui::SameLine();
		if (ImGui::Button("読み込み")) {
			MagicActionDatabase::LoadFromFile(kMagicActionPath);
			selectedAction = 0;
			selectedEvent = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("アニメ一覧更新")) {
			RefreshPlayerAnimationList();
		}
		ImGui::SameLine();
		if (ImGui::Button("新規")) {
			MagicActionData action;
			action.name = "NewMagic_" + std::to_string(actions.size());
			actions.push_back(action);
			selectedAction = static_cast<int>(actions.size()) - 1;
			selectedEvent = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("複製") && !actions.empty()) {
			MagicActionData copy = actions[std::clamp(selectedAction, 0, static_cast<int>(actions.size()) - 1)];
			copy.name += "_copy";
			actions.push_back(copy);
			selectedAction = static_cast<int>(actions.size()) - 1;
			selectedEvent = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("削除") && !actions.empty()) {
			actions.erase(actions.begin() + std::clamp(selectedAction, 0, static_cast<int>(actions.size()) - 1));
			selectedAction = std::clamp(selectedAction, 0, std::max(0, static_cast<int>(actions.size()) - 1));
			selectedEvent = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("↑") && selectedAction > 0) {
			std::swap(actions[selectedAction], actions[selectedAction - 1]);
			--selectedAction;
		}
		ImGui::SameLine();
		if (ImGui::Button("↓") && selectedAction + 1 < static_cast<int>(actions.size())) {
			std::swap(actions[selectedAction], actions[selectedAction + 1]);
			++selectedAction;
		}
		ImGui::SameLine();
		ImGui::Text("ファイル: %s | 魔法数: %d", kMagicActionPath, static_cast<int>(actions.size()));
	}

	void DrawActionList(std::vector<MagicActionData>& actions, int& selectedAction, int& selectedEvent)
	{
		ImGui::Text("魔法リスト");
		ImGui::Separator();

		const PlayerMagicSlot slots[] = {
			PlayerMagicSlot::Primary,
			PlayerMagicSlot::Secondary,
			PlayerMagicSlot::Utility,
		};

		for (PlayerMagicSlot slot : slots) {
			if (!ImGui::CollapsingHeader(SlotLabel(slot), ImGuiTreeNodeFlags_DefaultOpen)) continue;

			int comboStep = 1;
			for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
				if (actions[i].slot != slot) continue;

				const bool selected = (i == selectedAction);
				const std::string label = std::to_string(comboStep) + "段目: " + actions[i].name + "##magic" + std::to_string(i);
				if (selected) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.8f, 1.0f, 1.0f));
				}
				if (ImGui::Selectable(label.c_str(), selected)) {
					selectedAction = i;
					selectedEvent = 0;
				}
				if (selected) {
					ImGui::PopStyleColor();
				}
				++comboStep;
			}
		}
	}

	void DrawActionDetail(MagicActionData& action)
	{
		ImGui::Text("詳細");
		ImGui::Separator();
		if (ImGui::CollapsingHeader("基本情報", ImGuiTreeNodeFlags_DefaultOpen)) {
			DrawActionFields(action);
		}
		if (ImGui::CollapsingHeader("魔法コンボ", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("同じスロット内の並び順がコンボ段数になります。");
			ImGui::Text("入力後 %.2f 秒以内に同じスロットを押すと次段へ進みます。", action.chainResetTime);
		}
	}

	void DrawEventList(MagicActionData& action, int& selectedEvent)
	{
		ImGui::Text("イベント");
		ImGui::Separator();
		if (ImGui::Button("イベント追加")) {
			action.events.push_back(MagicTimelineEvent{});
			selectedEvent = static_cast<int>(action.events.size()) - 1;
		}
		ImGui::SameLine();
		if (ImGui::Button("イベント削除") && !action.events.empty()) {
			action.events.erase(action.events.begin() + std::clamp(selectedEvent, 0, static_cast<int>(action.events.size()) - 1));
			selectedEvent = std::clamp(selectedEvent, 0, std::max(0, static_cast<int>(action.events.size()) - 1));
		}

		auto appendLayer = [&](const char* memo, MagicEventTrigger trigger,
			MagicEventType type, MagicEffectBackend backend, const char* asset,
			float time, int count) {
			MagicTimelineEvent e;
			e.label = memo;
			e.trigger = trigger;
			e.type = type;
			e.effectBackend = backend;
			e.vfxAsset = asset;
			e.time = time;
			e.emitCount = count;
			// 新規魔法素材の寿命は用途別の3規格だけを使う。
			// エディタ上にも同じ値を出し、VfxMesh/Compositeの尺合わせにも利用する。
			const std::string assetName = asset;
			if (assetName.find("Charge") != std::string::npos ||
				assetName.find("Cloud") != std::string::npos) {
				e.duration = 1.2f;
			} else if (assetName.find("Trail") != std::string::npos ||
				assetName.find("Arc") != std::string::npos ||
				assetName.find("Chain") != std::string::npos ||
				assetName.find("Bolt") != std::string::npos) {
				e.duration = 0.9f;
			} else {
				e.duration = 0.6f;
			}
			action.events.push_back(std::move(e));
		};
		if (ImGui::Button("炎発射3層プリセット")) {
			appendLayer("Launch Mesh", MagicEventTrigger::OnRelease,
				MagicEventType::PlayVfx, MagicEffectBackend::VfxMesh, "NewEffect3", 0.0f, 1);
			appendLayer("Launch CPU", MagicEventTrigger::OnRelease,
				MagicEventType::PlayVfx, MagicEffectBackend::CpuParticle, "FireCastSparks", 0.0f, 26);
			appendLayer("Launch Embers", MagicEventTrigger::OnRelease,
				MagicEventType::PlayVfx, MagicEffectBackend::CpuParticle, "FireTrailEmbers", 0.0f, 14);
			selectedEvent = static_cast<int>(action.events.size()) - 3;
		}
		ImGui::SameLine();
		if (ImGui::Button("炎着弾3層プリセット")) {
			const float hitTime = std::max(0.0f, action.duration * 0.85f);
			appendLayer("Impact Mesh", MagicEventTrigger::OnTimeline,
				MagicEventType::SpawnArea, MagicEffectBackend::VfxMesh, "Explosion", hitTime, 1);
			appendLayer("Impact Sparks", MagicEventTrigger::OnTimeline,
				MagicEventType::SpawnArea, MagicEffectBackend::CpuParticle, "FireImpactSparks", hitTime, 42);
			appendLayer("Impact Cloud", MagicEventTrigger::OnTimeline,
				MagicEventType::SpawnArea, MagicEffectBackend::CpuParticle, "FireImpactCloud", hitTime, 16);
			selectedEvent = static_cast<int>(action.events.size()) - 3;
		}

		if (ImGui::Button("炎魔法セット")) {
			appendLayer("Fire Charge", MagicEventTrigger::OnStart,
				MagicEventType::PlayVfx, MagicEffectBackend::Composite, "FireCharge", 0.0f, 1);
			appendLayer("Fire Cast", MagicEventTrigger::OnRelease,
				MagicEventType::PlayVfx, MagicEffectBackend::Composite, "FireCastFlash", 0.0f, 1);
			appendLayer("Fire Impact", MagicEventTrigger::OnTimeline,
				MagicEventType::SpawnArea, MagicEffectBackend::Composite, "FireImpact",
				std::max(0.0f, action.duration * 0.85f), 1);
			selectedEvent = static_cast<int>(action.events.size()) - 3;
		}
		ImGui::SameLine();
		if (ImGui::Button("雷魔法セット")) {
			appendLayer("Thunder Charge", MagicEventTrigger::OnStart,
				MagicEventType::PlayVfx, MagicEffectBackend::Composite, "ThunderCharge", 0.0f, 1);
			appendLayer("Thunder Cast", MagicEventTrigger::OnRelease,
				MagicEventType::PlayVfx, MagicEffectBackend::Composite, "ThunderCastFlash", 0.0f, 1);
			appendLayer("Thunder Impact", MagicEventTrigger::OnTimeline,
				MagicEventType::StrikeTarget, MagicEffectBackend::Composite, "ThunderImpact",
				std::max(0.0f, action.duration * 0.85f), 1);
			selectedEvent = static_cast<int>(action.events.size()) - 3;
		}

		if (action.events.empty()) {
			ImGui::TextDisabled("イベントがありません。");
			return;
		}

		selectedEvent = std::clamp(selectedEvent, 0, static_cast<int>(action.events.size()) - 1);
		if (ImGui::BeginListBox("イベント一覧", ImVec2(260.0f, 120.0f))) {
			for (int i = 0; i < static_cast<int>(action.events.size()); ++i) {
				const std::string label = std::to_string(i) + ": " + ToString(action.events[i].trigger) + " / " + ToString(action.events[i].type);
				if (ImGui::Selectable(label.c_str(), selectedEvent == i)) {
					selectedEvent = i;
				}
			}
			ImGui::EndListBox();
		}

		ImGui::Separator();
		DrawEventFields(action.events[selectedEvent]);
	}

	void DrawEventFields(MagicTimelineEvent& event)
	{
		ImGui::InputFloat("発火時間", &event.time, 0.05f, 0.2f);

		const char* triggers[] = { "OnStart", "OnTimeline", "OnRelease" };
		DrawEnumCombo("発火条件", event.trigger, triggers, MagicEventTriggerFromString);

		const char* types[] = { "Debug", "PlayVfx", "SpawnBeam", "SpawnProjectile", "SpawnArea", "StrikeTarget" };
		DrawEnumCombo("動作の種類", event.type, types, MagicEventTypeFromString);
		const char* typeHelp = "ログのみ";
		switch (event.type) {
		case MagicEventType::PlayVfx: typeHelp = "発射位置にVFXを1回再生"; break;
		case MagicEventType::SpawnBeam: typeHelp = "発射位置から目標までビームVFX"; break;
		case MagicEventType::SpawnProjectile: typeHelp = "目標への軌跡＋着弾VFX"; break;
		case MagicEventType::SpawnArea: typeHelp = "目標位置に範囲VFX"; break;
		case MagicEventType::StrikeTarget: typeHelp = "目標の上から落雷＋着弾VFX"; break;
		default: break;
		}

		ImGui::SameLine();
		ImGui::TextDisabled("%s", typeHelp);

		const char* elements[] = { "None", "Fire", "Thunder", "Ice", "Light" };
		DrawEnumCombo("属性", event.element, elements, MagicElementFromString);

		char labelBuffer[128] = {};
		strncpy_s(labelBuffer, event.label.c_str(), sizeof(labelBuffer) - 1);
		if (ImGui::InputText("メモ／イベント名", labelBuffer, sizeof(labelBuffer))) {
			event.label = labelBuffer;
		}

		const char* backends[] = { "Composite", "VfxMesh", "CpuParticle", "GpuParticle" };
		DrawEnumCombo("エフェクト方式", event.effectBackend, backends,
			MagicEffectBackendFromString);

		std::vector<std::string> assets;
		bool assetExists = false;
		const char* assetLabel = "VfxMesh アセット";
		if (event.effectBackend == MagicEffectBackend::Composite) {
			assetLabel = "複合エフェクト";
			assets = CompositeEffectManager::GetInstance()->GetAssetNames();
			assetExists = event.vfxAsset.empty() ||
				CompositeEffectManager::GetInstance()->Has(event.vfxAsset);
		} else if (event.effectBackend == MagicEffectBackend::VfxMesh) {
			auto* spawner = VfxMeshSpawner::GetInstance();
			assets = spawner->GetAssetNames();
			assetExists = event.vfxAsset.empty() || spawner->GetAsset(event.vfxAsset);
		} else if (event.effectBackend == MagicEffectBackend::CpuParticle) {
			assetLabel = "CPU Particle グループ／システム";
			assets = YEmitterGroupManager::GetInstance().GetAllGroupNames();
			const auto systems = YParticleManager::GetInstance().GetAllSystemNames();
			assets.insert(assets.end(), systems.begin(), systems.end());
			assetExists = event.vfxAsset.empty() ||
				YEmitterGroupManager::GetInstance().GetGroup(event.vfxAsset) ||
				YParticleManager::GetInstance().GetSystem(event.vfxAsset);
		} else {
			assetLabel = "GPU Particle グループ";
			assets = YoRigine::GpuEmitManager::GetInstance()->GetGroupNames();
			assetExists = event.vfxAsset.empty() ||
				YoRigine::GpuEmitManager::GetInstance()->HasGroup(event.vfxAsset);
		}
		std::sort(assets.begin(), assets.end());
		assets.erase(std::unique(assets.begin(), assets.end()), assets.end());

		const char* vfxPreview = event.vfxAsset.empty() ? "(未設定)" : event.vfxAsset.c_str();
		if (ImGui::BeginCombo(assetLabel, vfxPreview)) {
			if (ImGui::Selectable("(未設定)", event.vfxAsset.empty())) {
				event.vfxAsset.clear();
			}
			for (const std::string& asset : assets) {
				const bool selected = event.vfxAsset == asset;
				if (ImGui::Selectable(asset.c_str(), selected)) event.vfxAsset = asset;
				if (selected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		if (!assetExists) {
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f),
				"未検出のエフェクト: %s", event.vfxAsset.c_str());
		}
		ImGui::DragFloat3("発生位置オフセット", &event.effectOffset.x,
			0.05f, -50.0f, 50.0f, "%.2f");
		if (event.effectBackend != MagicEffectBackend::VfxMesh) {
			ImGui::DragInt("放出数", &event.emitCount, 1.0f, 1, 10000);
			event.emitCount = std::clamp(event.emitCount, 1, 10000);
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

	DrawToolbar(actions, selectedAction_, selectedEvent_);
	ImGui::Separator();

	if (actions.empty()) {
		ImGui::TextDisabled("魔法データがありません。");
		return;
	}

	selectedAction_ = std::clamp(selectedAction_, 0, static_cast<int>(actions.size()) - 1);

	// 左は魔法選択に固定し、右は用途別タブに分ける。
	// 縦長の単一フォームをスクロールし続ける必要をなくす。
	ImGui::Columns(2, nullptr, true);
	DrawActionList(actions, selectedAction_, selectedEvent_);
	ImGui::NextColumn();
	MagicActionData& action = actions[selectedAction_];
	ImGui::TextColored(ImVec4(0.35f, 0.8f, 1.0f, 1.0f), "%s  /  %s  /  %s",
		action.name.c_str(), ToString(action.hitShape), ToString(action.trajectoryType));
	if (ImGui::BeginTabBar("MagicActionEditTabs")) {
		if (ImGui::BeginTabItem("基本・判定")) {
			DrawActionDetail(action);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("スケールカーブ")) {
			DrawScaleCurve(action);
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("イベント")) {
			DrawEventList(action, selectedEvent_);
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::Columns(1);
#endif
}
