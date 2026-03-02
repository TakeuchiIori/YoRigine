#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <json.hpp>
#include <fstream>
#include <filesystem>

#include "ComboTypes.h"
#include "Loaders/Json/ConversionJson.h"
#include "Loaders/Json/VariableJson.h"
#include <Debugger/Logger.h>

class AttackDatabase
{
public:
	// 攻撃データの一覧
	static std::vector<AttackData>& Get()
	{
		static std::vector<AttackData> attacks;
		return attacks;
	}

	// 名称 → インデックス
	static int FindIndex(const std::string& name)
	{
		auto& list = Get();
		for (int i = 0; i < static_cast<int>(list.size()); i++)
		{
			if (list[i].name == name)
				return i;
		}
		return -1;
	}

	// JSONファイル読込（ファイルがなければ空のJSONを生成）
	static bool LoadFromFile(const std::string& path)
	{
		std::ifstream ifs(path);

		// ファイルが存在しない場合
		if (!ifs)
		{
			Logger("[AttackDatabase] File not found. Creating empty JSON...\n");

			// 空の配列を設定
			Get().clear();

			// ディレクトリを作成（存在しない場合）
			std::filesystem::path filePath(path);
			std::filesystem::path dir = filePath.parent_path();

			if (!dir.empty() && !std::filesystem::exists(dir))
			{
				std::filesystem::create_directories(dir);

				std::string msg = "[AttackDatabase] Created directory: " + dir.string() + "\n";
				Logger(msg.c_str());
			}

			// 空のJSON配列をファイルに書き込み
			std::ofstream ofs(path);
			if (!ofs)
			{
				Logger("[AttackDatabase] ERROR: Cannot create file!\n");
				return false;
			}

			ofs << "[]";  // 空の配列
			ofs.close();

			std::string msg = "[AttackDatabase] Created empty file: " + path + "\n";
			Logger(msg.c_str());

			return true;
		}

		// ファイルが存在する場合は通常通り読み込み
		try
		{
			nlohmann::json j;
			ifs >> j;

			Get() = j.get<std::vector<AttackData>>();

			std::string msg = "[AttackDatabase] Loaded " + std::to_string(Get().size()) + " attacks from: " + path + "\n";
			Logger(msg.c_str());

			return true;
		}
		catch (const std::exception& e)
		{
			std::string msg = "[AttackDatabase] JSON parse error: " + std::string(e.what()) + "\n";
			Logger(msg.c_str());
			return false;
		}
	}

	// JSONファイル保存
	static bool SaveToFile(const std::string& path)
	{
		try
		{
			nlohmann::json j = Get();

			std::ofstream ofs(path);
			if (!ofs)
			{
				std::string msg = "[AttackDatabase] ERROR: Cannot open file for writing: " + path + "\n";
				Logger(msg.c_str());
				return false;
			}

			ofs << j.dump(4);  // インデント4で整形
			ofs.close();

			std::string msg = "[AttackDatabase] Saved " + std::to_string(Get().size()) + " attacks to: " + path + "\n";
			Logger(msg.c_str());

			return true;
		}
		catch (const std::exception& e)
		{
			std::string msg = "[AttackDatabase] Save error: " + std::string(e.what()) + "\n";
			Logger(msg.c_str());
			return false;
		}
	}
};