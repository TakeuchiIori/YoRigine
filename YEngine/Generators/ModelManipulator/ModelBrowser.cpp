#ifdef USE_IMGUI

#include "ModelBrowser.h"

// C++
#include <filesystem>
#include <algorithm>
#include <iostream>

// ImGui
#include "imgui.h"

namespace YoRigine {

void ModelBrowser::SetModelFolderPath(const std::string& path)
{
    modelFolderPath_ = path;
}

//=============================================================================
// フォルダスキャン
//=============================================================================
void ModelBrowser::ScanModelFolder()
{
    modelFiles_.clear();
    modelNames_.clear();
    filteredIndices_.clear();

    if (!std::filesystem::exists(modelFolderPath_)) {
        std::cout << "[ModelBrowser] Folder not found: " << modelFolderPath_ << "\n";
        return;
    }

    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(modelFolderPath_))
    {
        if (!entry.is_regular_file()) continue;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb") {
            modelFiles_.push_back(entry.path().string());
            modelNames_.push_back(
                std::filesystem::relative(entry.path(), modelFolderPath_).string());
        }
    }

    for (int i = 0; i < static_cast<int>(modelNames_.size()); ++i)
        filteredIndices_.push_back(i);

    if (selectedIndex_ >= static_cast<int>(modelFiles_.size()))
        selectedIndex_ = -1;
}

//=============================================================================
// ImGui 描画
//=============================================================================
void ModelBrowser::Draw()
{
    // ── フォルダ操作 ──────────────────────────────────────────
    if (ImGui::Button("フォルダ更新")) ScanModelFolder();
    ImGui::SameLine();
    if (ImGui::Button("フォルダ変更...")) { /* TODO */ }
    ImGui::Text("現在のフォルダ: %s", modelFolderPath_.c_str());
    ImGui::Separator();

    // ── 検索バー（Ctrl+F トグル） ─────────────────────────────
    if (ImGui::IsKeyPressed(ImGuiKey_F) && ImGui::GetIO().KeyCtrl)
        showSearchBar_ = !showSearchBar_;

    if (showSearchBar_) {
        if (ImGui::CollapsingHeader("検索バー##ModelSearch",
                                    ImGuiTreeNodeFlags_FramePadding)) {
            ImGui::SetNextItemAllowOverlap();
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##SearchBar", searchBuf_, IM_ARRAYSIZE(searchBuf_)))
                FilterModels();
            ImGui::PopItemWidth();
            ImGui::Separator();
        }
    }

    // ── モデル一覧 ────────────────────────────────────────────
    if (ImGui::BeginListBox("##ModelList", ImVec2(-1, 200))) {
        for (int idx : filteredIndices_) {
            if (idx < 0 || idx >= static_cast<int>(modelNames_.size())) continue;

            bool sel = (selectedIndex_ == idx);
            if (ImGui::Selectable(modelNames_[idx].c_str(), sel))
                selectedIndex_ = idx;

            // ダブルクリックで即配置
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (IsValidIndex(idx) && placeCallback_)
                    placeCallback_(modelFiles_[idx]);
            }
        }
        ImGui::EndListBox();
    }

    // ── 配置ボタン ────────────────────────────────────────────
    if (IsValidIndex(selectedIndex_)) {
        if (ImGui::Button("シーンに配置する") && placeCallback_)
            placeCallback_(modelFiles_[selectedIndex_]);
        ImGui::Text("選択中: %s", modelNames_[selectedIndex_].c_str());
    }
}

//=============================================================================
// フィルタリング
//=============================================================================
void ModelBrowser::FilterModels()
{
    filteredIndices_.clear();

    std::string term = searchBuf_;
    std::transform(term.begin(), term.end(), term.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    for (int i = 0; i < static_cast<int>(modelNames_.size()); ++i) {
        std::string name = modelNames_[i];
        std::transform(name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (term.empty() || name.find(term) != std::string::npos)
            filteredIndices_.push_back(i);
    }
}

} // namespace YoRigine

#endif // USE_IMGUI
