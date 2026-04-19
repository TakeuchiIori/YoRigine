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

// ============================================================
// 攻撃データベースクラス
// 攻撃用データ（AttackData）をメモリ上に保持し、JSONファイルとの入出力を担う
// ============================================================
class AttackDatabase
{
public:
	// ============================================================
	// 攻撃データの一覧を取得（シングルトン的なアクセス）
	// ============================================================
	static std::vector<AttackData>& Get()
	{
		static std::vector<AttackData> attacks;
		return attacks;
	}

	// ============================================================
	// 名称から攻撃データのインデックスを検索
	// 見つからない場合は -1 を返す
	// ============================================================
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

	// ============================================================
	// JSONファイルからの読み込み
	// ファイルが存在しない場合は、対象ディレクトリと空のJSONファイルを新規作成する
	// ============================================================
	static bool LoadFromFile(const std::string& path)
	{
		std::ifstream ifs(path);

		// ------------------------------------------------------------
		// ファイルが存在しない場合の初期化処理
		// ------------------------------------------------------------
		if (!ifs)
		{
			Logger("[AttackDatabase] File not found. Creating empty JSON...\n");

			Get().clear();

			std::filesystem::path filePath(path);
			std::filesystem::path dir = filePath.parent_path();

			if (!dir.empty() && !std::filesystem::exists(dir))
			{
				std::filesystem::create_directories(dir);
				std::string msg = "[AttackDatabase] Created directory: " + dir.string() + "\n";
				Logger(msg.c_str());
			}

			std::ofstream ofs(path);
			if (!ofs)
			{
				Logger("[AttackDatabase] ERROR: Cannot create file!\n");
				return false;
			}

			// 空のJSON配列を書き込んで初期化
			ofs << "[]";
			ofs.close();

			std::string msg = "[AttackDatabase] Created empty file: " + path + "\n";
			Logger(msg.c_str());

			return true;
		}

		// ------------------------------------------------------------
		// ファイルが存在する場合の通常読み込み処理
		// ------------------------------------------------------------
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

	// ============================================================
	// JSONファイルへの保存
	// 現在メモリ上にある攻撃データをJSON形式で整形出力する
	// ============================================================
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

			// インデント4で人間が読みやすい形式に整形して保存
			ofs << j.dump(4);
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