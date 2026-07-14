#pragma once

#include "MagicActionData.h"

#include <string>
#include <vector>

class MagicActionDatabase {
public:
	static std::vector<MagicActionData>& Get();
	static bool LoadFromFile(const std::string& path);
	static bool SaveToFile(const std::string& path);
	static const MagicActionData* FindBySlot(PlayerMagicSlot slot);

private:
	static std::vector<MagicActionData> CreateDefaultActions();
};
