#include "FileBrowser.h"

// C++
#include <filesystem>
#include <algorithm>
#include <iostream>

#ifdef USE_IMGUI
namespace YoRigine {

    // -----------------------------------------------------------------------
    // コンストラクタ
    // -----------------------------------------------------------------------
    FileBrowser::FileBrowser(
        const std::string& rootDirectory,
        const std::vector<std::string>& extensions,
        DisplayMode displayMode
    )
        : currentDir_(rootDirectory)
        , displayMode_(displayMode)
    {
        SetExtensions(extensions);
        Scan(rootDirectory);
    }

    // -----------------------------------------------------------------------
    // SetExtensions
    // -----------------------------------------------------------------------
    void FileBrowser::SetExtensions(const std::vector<std::string>& extensions)
    {
        extensions_.clear();
        for (const auto& ext : extensions) {
            std::string lower = ext;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            extensions_.insert(lower);
        }
    }

    // -----------------------------------------------------------------------
    // Scan
    // -----------------------------------------------------------------------
    bool FileBrowser::Scan(const std::string& directory)
    {
        const std::string& target = directory.empty() ? currentDir_ : directory;

        if (!std::filesystem::exists(target) || !std::filesystem::is_directory(target)) {
            std::cerr << "[FileBrowser] ディレクトリが存在しません: " << target << "\n";
            files_.clear();
            folders_.clear();
            return false;
        }

        currentDir_ = target;
        // 末尾スラッシュを統一
        if (!currentDir_.empty() &&
            currentDir_.back() != '/' && currentDir_.back() != '\\') {
            currentDir_ += '/';
        }

        files_.clear();
        folders_.clear();

        for (const auto& entry : std::filesystem::directory_iterator(target))
        {
            if (entry.is_directory()) {
                folders_.push_back(entry.path().filename().string());
                continue;
            }

            if (!entry.is_regular_file()) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // 拡張子フィルタ (空の場合は全ファイルを許可)
            if (!extensions_.empty() && extensions_.find(ext) == extensions_.end()) continue;

            files_.push_back(entry.path().string());
        }

        // ソート（見やすさのため）
        std::sort(folders_.begin(), folders_.end());
        std::sort(files_.begin(), files_.end());

        return true;
    }

    // -----------------------------------------------------------------------
    // GoUp
    // -----------------------------------------------------------------------
    void FileBrowser::GoUp()
    {
        std::filesystem::path parent =
            std::filesystem::path(currentDir_).parent_path().parent_path(); // 末尾 '/' 対策で2段
        if (!parent.empty()) {
            Scan(parent.string());
        }
    }

    // -----------------------------------------------------------------------
    // Draw
    // -----------------------------------------------------------------------
    void FileBrowser::Draw(const char* id, ImVec2 size)
    {
#ifdef USE_IMGUI
        if (ImGui::BeginChild(id, size, true, ImGuiWindowFlags_HorizontalScrollbar))
        {
            // ヘッダー：現在のディレクトリと再スキャンボタン
            ImGui::Text("\uf07b %s", currentDir_.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("\uf2f9 再スキャン")) {
                Scan();
            }

            // 表示モード切り替えボタン
            ImGui::SameLine();
            if (displayMode_ == DisplayMode::List) {
                if (ImGui::SmallButton("\uf00a グリッド")) {
                    displayMode_ = DisplayMode::Grid;
                }
            }
            else {
                if (ImGui::SmallButton("\uf03a リスト")) {
                    displayMode_ = DisplayMode::List;
                }
            }

            ImGui::Separator();

            DrawFileList();
        }
        ImGui::EndChild();
#else
        (void)id; (void)size;
#endif
    }

    // -----------------------------------------------------------------------
    // DrawFileList (外部向けインタフェース)
    // -----------------------------------------------------------------------
    bool FileBrowser::DrawFileList()
    {
#ifdef USE_IMGUI
        bool selected = false;

        // ----- 親フォルダへ戻る -----
        if (ImGui::Selectable("\uf148")) {
            GoUp();
            return false;
        }

        ImGui::Spacing();

        // ----- サブフォルダ -----
        for (const auto& folder : folders_)
        {
            std::string label = "\uf07b " + folder;
            if (ImGui::Selectable(label.c_str()))
            {
                std::string fullPath = currentDir_ + folder;
                Scan(fullPath);
                return false; // フォルダ移動は選択扱いしない
            }
        }

        if (!folders_.empty() && !files_.empty())
            ImGui::Separator();

        // ----- ファイル -----
        if (displayMode_ == DisplayMode::Grid) {
            selected = DrawGrid();
        }
        else {
            selected = DrawList();
        }

        return selected;
#else
        return false;
#endif
    }

    // -----------------------------------------------------------------------
    // DrawGrid (private)
    // -----------------------------------------------------------------------
    bool FileBrowser::DrawGrid()
    {
#ifdef USE_IMGUI
        bool selected = false;

        int columns = static_cast<int>(ImGui::GetContentRegionAvail().x / (thumbnailSize_ + 16.0f));
        columns = std::max(1, columns);

        if (ImGui::BeginTable("##FileBrowserGrid", columns))
        {
            for (const auto& fullPath : files_)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(fullPath.c_str());

                std::string filename = std::filesystem::path(fullPath).filename().string();

                // サムネイル取得
                ImTextureID texID = 0;
                if (thumbnailProvider_) {
                    texID = thumbnailProvider_(fullPath);
                }

                bool clicked = false;
                if (texID != 0) {
                    // サムネイルボタン
                    clicked = ImGui::ImageButton("##img", texID,
                        ImVec2(thumbnailSize_, thumbnailSize_));
                }
                else {
                    // アイコン代替ボタン
                    clicked = ImGui::Button(
                        ("\uf15b\n" + filename).c_str(),
                        ImVec2(thumbnailSize_, thumbnailSize_));
                }

                if (clicked) {
                    selectedPath_ = fullPath;
                    selected = true;
                    if (onFileSelected_) {
                        onFileSelected_(fullPath);
                    }
                }

                // ホバー時ツールチップ
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", filename.c_str());
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        return selected;
#else
        return false;
#endif
    }

    // -----------------------------------------------------------------------
    // DrawList (private)
    // -----------------------------------------------------------------------
    bool FileBrowser::DrawList()
    {
#ifdef USE_IMGUI
        bool selected = false;

        for (const auto& fullPath : files_)
        {
            std::string filename = std::filesystem::path(fullPath).filename().string();
            bool isSelected = (selectedPath_ == fullPath);

            std::string label = "\uf15b " + filename;
            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                selectedPath_ = fullPath;
                selected = true;
                if (onFileSelected_) {
                    onFileSelected_(fullPath);
                }
            }

            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }

            // ホバー時にフルパスをツールチップ表示
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", fullPath.c_str());
                ImGui::EndTooltip();
            }
        }

        return selected;
#else
        return false;
#endif
    }

    // -----------------------------------------------------------------------
    // MatchesFilter (private)
    // -----------------------------------------------------------------------
    bool FileBrowser::MatchesFilter(const std::string& extension) const
    {
        if (extensions_.empty()) return true;
        std::string lower = extension;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return extensions_.count(lower) > 0;
    }

} // namespace YoRigine
#endif // USE_IMGUI
