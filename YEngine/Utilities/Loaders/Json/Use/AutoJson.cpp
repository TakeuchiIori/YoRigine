#include "AutoJson.h"
#include <fstream>

// ==============================================================================
// フィールド登録
// ==============================================================================
AutoJson& AutoJson::AddGroup(const std::string& groupName, AutoJson& child)
{
	children_.emplace_back(groupName, &child);
	return *this;
}

// =============================================================================
// JSON へ保存
// =============================================================================
void AutoJson::Save(nlohmann::json& j) const
{
    for (const auto& [name, var] : entries_)
    {
        var->SaveToJson(j[name]);
    }
    for (const auto& [name, child] : children_)
    {
        child->Save(j[name]);
    }
}

// ==============================================================================
// JSON から読み込み（存在しないキーは無視）
// =============================================================================
void AutoJson::Load(const nlohmann::json& j)
{
    for (auto& [name, var] : entries_)
    {
        if (j.contains(name)) var->LoadFromJson(j[name]);
    }
    for (auto& [name, child] : children_)
    {
        if (j.contains(name)) child->Load(j[name]);
    }
}

// ==============================================================================
// ファイルへ保存
// ==============================================================================
void AutoJson::SaveToFile(const std::string& filepath) const
{
    nlohmann::json j;
    Save(j);
    std::ofstream ofs(filepath);
    ofs << j.dump(4);
}

// ==============================================================================
// ファイルから読み込み
// ==============================================================================
void AutoJson::LoadFromFile(const std::string& filepath)
{
    std::ifstream ifs(filepath);
    if (!ifs.is_open()) return;
    nlohmann::json j;
    ifs >> j;
    Load(j);
}

// ==============================================================================
// ImGui 表示
// ==============================================================================
void AutoJson::ShowImGui(const std::string& uniqueId)
{
#ifdef USE_IMGUI
    for (auto& [name, var] : entries_)
    {
        var->ShowImGui(name, uniqueId + name);
    }
    for (auto& [name, child] : children_)
    {
        if (ImGui::TreeNode(name.c_str()))
        {
            child->ShowImGui(uniqueId + name);
            ImGui::TreePop();
        }
    }
#endif
}

// ==============================================================================
// 全フィールドを初期値にリセット
// ==============================================================================
void AutoJson::Reset()
{
    for (auto& [name, var] : entries_)
        var->ResetValue();
    for (auto& [name, child] : children_)
        child->Reset();
}
