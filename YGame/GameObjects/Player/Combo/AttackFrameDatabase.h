#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <json.hpp>
#include <Debugger/Logger.h>

#include "AttackFrameData.h"

//=============================================================================
// AttackFrameDatabase
// AttackFrameData の一覧を管理・永続化する静的クラス
// AttackDatabase と同じパターンで実装
//=============================================================================
class AttackFrameDatabase
{
public:
    //=========================================================================
    // データアクセス
    //=========================================================================

    // フレームデータの一覧を取得
    static std::vector<AttackFrameData>& Get()
    {
        static std::vector<AttackFrameData> list;
        return list;
    }

    // attackName からインデックスを検索（なければ -1）
    static int FindIndex(const std::string& attackName)
    {
        auto& list = Get();
        for (int i = 0; i < static_cast<int>(list.size()); ++i)
        {
            if (list[i].attackName == attackName)
                return i;
        }
        return -1;
    }

    // attackName から AttackFrameData を取得（なければ nullptr）
    static AttackFrameData* Find(const std::string& attackName)
    {
        int idx = FindIndex(attackName);
        return (idx >= 0) ? &Get()[idx] : nullptr;
    }

    // attackName に対応するデータがなければデフォルト値で追加して返す
    static AttackFrameData& FindOrCreate(const std::string& attackName)
    {
        int idx = FindIndex(attackName);
        if (idx >= 0)
            return Get()[idx];

        AttackFrameData data;
        data.attackName = attackName;
        Get().push_back(data);

        std::string msg = "[AttackFrameDatabase] Created new entry: " + attackName + "\n";
        Logger(msg.c_str());

        return Get().back();
    }

    //=========================================================================
    // JSON ファイル I/O
    //=========================================================================

    static bool LoadFromFile(const std::string& path)
    {
        std::ifstream ifs(path);

        if (!ifs)
        {
            Logger("[AttackFrameDatabase] File not found. Creating empty JSON...\n");

            Get().clear();

            // ディレクトリがなければ作成
            std::filesystem::path filePath(path);
            std::filesystem::path dir = filePath.parent_path();
            if (!dir.empty() && !std::filesystem::exists(dir))
            {
                std::filesystem::create_directories(dir);
                std::string msg = "[AttackFrameDatabase] Created directory: " + dir.string() + "\n";
                Logger(msg.c_str());
            }

            // 空の配列を書き込む
            std::ofstream ofs(path);
            if (!ofs)
            {
                Logger("[AttackFrameDatabase] ERROR: Cannot create file!\n");
                return false;
            }
            ofs << "[]";
            ofs.close();

            std::string msg = "[AttackFrameDatabase] Created empty file: " + path + "\n";
            Logger(msg.c_str());
            return true;
        }

        try
        {
            nlohmann::json j;
            ifs >> j;
            Get() = j.get<std::vector<AttackFrameData>>();

            std::string msg = "[AttackFrameDatabase] Loaded " +
                std::to_string(Get().size()) + " entries from: " + path + "\n";
            Logger(msg.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            std::string msg = "[AttackFrameDatabase] JSON parse error: " +
                std::string(e.what()) + "\n";
            Logger(msg.c_str());
            return false;
        }
    }

    static bool SaveToFile(const std::string& path)
    {
        try
        {
            nlohmann::json j = Get();

            std::ofstream ofs(path);
            if (!ofs)
            {
                std::string msg = "[AttackFrameDatabase] ERROR: Cannot open for writing: " +
                    path + "\n";
                Logger(msg.c_str());
                return false;
            }

            ofs << j.dump(4);
            ofs.close();

            std::string msg = "[AttackFrameDatabase] Saved " +
                std::to_string(Get().size()) + " entries to: " + path + "\n";
            Logger(msg.c_str());
            return true;
        }
        catch (const std::exception& e)
        {
            std::string msg = "[AttackFrameDatabase] Save error: " +
                std::string(e.what()) + "\n";
            Logger(msg.c_str());
            return false;
        }
    }
};
