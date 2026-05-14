#pragma once
#include <json.hpp>
#include <vector>
#include <string>
#include <memory>
#include "../VariableJson.h"

///=============================================================================
/// 変数を登録するだけで Save / Load / ImGui 表示が自動になるクラス。
/// 継承不要・どのクラスにもメンバとして置くだけで使える。
///
/// 【使い方】
///   AutoJson aj_;
///   aj_.Add("speed", &speed_)
///         .Add("hp",    &hp_);
///
///   aj_.Save(j);     // 保存
///   aj_.Load(j);     // 読み込み
///   aj_.ShowImGui(); // ImGui表示
///=============================================================================
class AutoJson
{
public:
    ///-----------------------------------------------------
    /// フィールド登録
    ///-----------------------------------------------------
    template <typename T>
    AutoJson& Add(const std::string& name, T* ptr)
    {
        entries_.emplace_back(name, std::make_unique<VariableJson<T>>(ptr));
        return *this;
    }

    ///-----------------------------------------------------
    /// ネストした AutoJson をグループとして登録
    /// 例: aj_.AddGroup("rush", rushaj_)
    ///-----------------------------------------------------
    AutoJson& AddGroup(const std::string& groupName, AutoJson& child);

    ///-----------------------------------------------------
    /// ファイルへ保存
    ///-----------------------------------------------------
    void SaveToFile(const std::string& filepath) const;

    ///-----------------------------------------------------
    /// ファイルから読み込み
    ///-----------------------------------------------------
    void LoadFromFile(const std::string& filepath);

    ///-----------------------------------------------------
    /// ImGui 表示
    ///-----------------------------------------------------
    void ShowImGui(const std::string& uniqueId = "");

    ///-----------------------------------------------------
    /// 全フィールドを初期値にリセット
    ///-----------------------------------------------------
    void Reset();

private:
    ///-----------------------------------------------------
    /// JSON へ保存
    ///-----------------------------------------------------
    void Save(nlohmann::json& j) const;

    ///-----------------------------------------------------
    /// JSON から読み込み（存在しないキーは無視）
    ///-----------------------------------------------------
    void Load(const nlohmann::json& j);

private:
    // フィールドエントリ: (名前, VariableJson)
    std::vector<std::pair<std::string, std::unique_ptr<IVariableJson>>> entries_;
    // 子グループ: (名前, 子AutoJson*)
    std::vector<std::pair<std::string, AutoJson*>> children_;
};