#include "MagicActionDatabase.h"

#include <Debugger/Logger.h>

#include <filesystem>
#include <fstream>

std::vector<MagicActionData>& MagicActionDatabase::Get()
{
	static std::vector<MagicActionData> actions;
	return actions;
}

bool MagicActionDatabase::LoadFromFile(const std::string& path)
{
	std::ifstream ifs(path);
	if (!ifs) {
		Get() = CreateDefaultActions();
		return SaveToFile(path);
	}

	try {
		nlohmann::json j;
		ifs >> j;
		Get() = j.get<std::vector<MagicActionData>>();
		Logger(("[MagicActionDatabase] Loaded: " + path + "\n").c_str());
		return true;
	}
	catch (const std::exception& e) {
		Logger(("[MagicActionDatabase] JSON parse error: " + std::string(e.what()) + "\n").c_str());
		Get() = CreateDefaultActions();
		return false;
	}
}

bool MagicActionDatabase::SaveToFile(const std::string& path)
{
	try {
		const std::filesystem::path filePath(path);
		const std::filesystem::path dir = filePath.parent_path();
		if (!dir.empty() && !std::filesystem::exists(dir)) {
			std::filesystem::create_directories(dir);
		}

		std::ofstream ofs(path);
		if (!ofs) return false;

		nlohmann::json j = Get();
		ofs << j.dump(4);
		return true;
	}
	catch (const std::exception& e) {
		Logger(("[MagicActionDatabase] Save error: " + std::string(e.what()) + "\n").c_str());
		return false;
	}
}

const MagicActionData* MagicActionDatabase::FindBySlot(PlayerMagicSlot slot)
{
	return FindBySlotAt(slot, 0);
}

const MagicActionData* MagicActionDatabase::FindBySlotAt(PlayerMagicSlot slot, int comboIndex)
{
	const int count = CountBySlot(slot);
	if (count <= 0) return nullptr;

	const int target = ((comboIndex % count) + count) % count;
	int current = 0;
	for (const auto& action : Get()) {
		if (action.slot != slot) continue;
		if (current == target) return &action;
		++current;
	}
	return nullptr;
}

int MagicActionDatabase::CountBySlot(PlayerMagicSlot slot)
{
	int count = 0;
	for (const auto& action : Get()) {
		if (action.slot == slot) ++count;
	}
	return count;
}

std::vector<MagicActionData> MagicActionDatabase::CreateDefaultActions()
{
	MagicActionData primary;
	primary.name = "ChargeBeam";
	primary.slot = PlayerMagicSlot::Primary;
	primary.inputMode = MagicInputMode::ChargeRelease;
	primary.animationName = "Attack1";
	primary.trajectoryType = MagicTrajectoryType::LockOnOrForward;
	primary.duration = 0.45f;
	primary.range = 24.0f;
	primary.hitRadius = 1.8f;
	primary.minChargeTime = 0.4f;
	primary.maxChargeTime = 2.0f;
	primary.cooldown = 0.6f;
	primary.chainResetTime = 1.2f;
	primary.scaleCurve.Clear();
	primary.scaleCurve.AddKey(0.0f, 0.4f);
	primary.scaleCurve.AddKey(0.35f, 1.4f);
	primary.scaleCurve.AddKey(1.0f, 0.2f);
	primary.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::PlayVfx, MagicElement::Light, "ChargeStart", 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, MagicEventTrigger::OnRelease, MagicEventType::SpawnBeam, MagicElement::Light, "ReleaseBeam", 12.0f, 1.0f, 0.0f, 0.0f },
	};

	MagicActionData primaryFinisher;
	primaryFinisher.name = "LightBurst";
	primaryFinisher.slot = PlayerMagicSlot::Primary;
	primaryFinisher.inputMode = MagicInputMode::Tap;
	primaryFinisher.animationName = "Attack2";
	primaryFinisher.trajectoryType = MagicTrajectoryType::StrikeTarget;
	primaryFinisher.duration = 0.28f;
	primaryFinisher.range = 20.0f;
	primaryFinisher.hitRadius = 2.4f;
	primaryFinisher.cooldown = 0.35f;
	primaryFinisher.chainResetTime = 1.0f;
	primaryFinisher.scaleCurve.Clear();
	primaryFinisher.scaleCurve.AddKey(0.0f, 0.3f);
	primaryFinisher.scaleCurve.AddKey(0.45f, 1.8f);
	primaryFinisher.scaleCurve.AddKey(1.0f, 0.5f);
	primaryFinisher.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::StrikeTarget, MagicElement::Light, "LightBurst", 10.0f, 0.0f, 2.4f, 0.0f },
	};

	MagicActionData secondary;
	secondary.name = "FireBolt";
	secondary.slot = PlayerMagicSlot::Secondary;
	secondary.inputMode = MagicInputMode::Tap;
	secondary.animationName = "Attack2";
	secondary.trajectoryType = MagicTrajectoryType::LockOnOrForward;
	secondary.duration = 0.35f;
	secondary.range = 18.0f;
	secondary.hitRadius = 1.6f;
	secondary.cooldown = 0.25f;
	secondary.chainResetTime = 1.0f;
	secondary.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::SpawnProjectile, MagicElement::Fire, "FireBolt", 6.0f, 0.0f, 0.0f, 32.0f },
	};

	MagicActionData utility;
	utility.name = "ThunderStrike";
	utility.slot = PlayerMagicSlot::Utility;
	utility.inputMode = MagicInputMode::Tap;
	utility.animationName = "Attack3";
	utility.trajectoryType = MagicTrajectoryType::StrikeTarget;
	utility.duration = 0.3f;
	utility.range = 20.0f;
	utility.hitRadius = 2.5f;
	utility.cooldown = 0.4f;
	utility.chainResetTime = 1.0f;
	utility.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::StrikeTarget, MagicElement::Thunder, "ThunderStrike", 8.0f, 0.0f, 3.0f, 0.0f },
	};

	return { primary, primaryFinisher, secondary, utility };
}
