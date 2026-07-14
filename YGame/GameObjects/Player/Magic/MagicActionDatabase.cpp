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
	for (const auto& action : Get()) {
		if (action.slot == slot) return &action;
	}
	return nullptr;
}

std::vector<MagicActionData> MagicActionDatabase::CreateDefaultActions()
{
	MagicActionData primary;
	primary.name = "ChargeBeam";
	primary.slot = PlayerMagicSlot::Primary;
	primary.inputMode = MagicInputMode::ChargeRelease;
	primary.minChargeTime = 0.4f;
	primary.maxChargeTime = 2.0f;
	primary.cooldown = 0.6f;
	primary.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::PlayVfx, MagicElement::Light, "ChargeStart", 0.0f, 0.0f, 0.0f, 0.0f },
		{ 0.0f, MagicEventTrigger::OnRelease, MagicEventType::SpawnBeam, MagicElement::Light, "ReleaseBeam", 12.0f, 1.0f, 0.0f, 0.0f },
	};

	MagicActionData secondary;
	secondary.name = "FireBolt";
	secondary.slot = PlayerMagicSlot::Secondary;
	secondary.inputMode = MagicInputMode::Tap;
	secondary.cooldown = 0.25f;
	secondary.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::SpawnProjectile, MagicElement::Fire, "FireBolt", 6.0f, 0.0f, 0.0f, 32.0f },
	};

	MagicActionData utility;
	utility.name = "ThunderStrike";
	utility.slot = PlayerMagicSlot::Utility;
	utility.inputMode = MagicInputMode::Tap;
	utility.cooldown = 0.4f;
	utility.events = {
		{ 0.0f, MagicEventTrigger::OnStart, MagicEventType::StrikeTarget, MagicElement::Thunder, "ThunderStrike", 8.0f, 0.0f, 3.0f, 0.0f },
	};

	return { primary, secondary, utility };
}
