#ifdef USE_IMGUI
#include "VfxMeshEditor.h"

#include "Debugger/Logger.h"

#include <algorithm>
#include <filesystem>
#include <utility>

namespace fs = std::filesystem;

namespace YoRigine {

    void VfxMeshEditor::SaveCurrent()
    {
        auto* sel = Selected();
        if (!sel) return;

        fs::path p(sel->filePath);
        if (!p.parent_path().empty()) {
            std::error_code ec;
            fs::create_directories(p.parent_path(), ec);
        }
        sel->asset.SaveToJson(sel->filePath);
        sel->isDirty = false;
        Logger("VfxMeshEditor: 保存 -> " + sel->filePath);
    }

    void VfxMeshEditor::SaveAs(const std::string& newPath)
    {
        auto* sel = Selected();
        if (!sel) return;

        sel->filePath = newPath;
        SaveCurrent();
    }

    void VfxMeshEditor::RenameCurrentFile(const std::string& newName)
    {
        auto* sel = Selected();
        if (!sel) return;

        if (newName.empty()) return;
        if (newName.find_first_of("\\/:*?\"<>|") != std::string::npos) {
            Logger("VfxMeshEditor: ファイル名に使えない文字が含まれるためリネームしません -> " + newName);
            return;
        }

        fs::path oldPath(sel->filePath);
        fs::path dir = oldPath.parent_path();
        if (dir.empty()) dir = fs::path(scanRoot_);
        if (oldPath.stem().string() == newName) return;

        auto conflicts = [&](const fs::path& p) {
            std::error_code e;
            if (fs::exists(p, e)) return true;
            for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
                if (i == selectedIndex_) continue;
                if (fs::path(entries_[i].filePath) == p) return true;
            }
            return false;
        };

        fs::path newPath = dir / (newName + ".json");
        for (int n = 1; conflicts(newPath); ++n) {
            newPath = dir / (newName + std::to_string(n) + ".json");
        }

        std::error_code ec;
        if (fs::exists(oldPath, ec)) {
            fs::rename(oldPath, newPath, ec);
            if (ec) {
                Logger("VfxMeshEditor: リネーム失敗 " + oldPath.string() + " -> " + newPath.string());
                return;
            }
            Logger("VfxMeshEditor: リネーム " + oldPath.string() + " -> " + newPath.string());
        }

        sel->filePath = newPath.string();
    }

    void VfxMeshEditor::DeleteCurrent()
    {
        auto* sel = Selected();
        if (!sel) return;

        std::error_code ec;
        fs::remove(sel->filePath, ec);
        if (ec) Logger("VfxMeshEditor: 削除失敗 -> " + sel->filePath);
        else    Logger("VfxMeshEditor: 削除 -> " + sel->filePath);

        entries_.erase(entries_.begin() + selectedIndex_);

        if (entries_.empty()) {
            selectedIndex_ = -1;
        } else {
            selectedIndex_ = std::min(selectedIndex_, static_cast<int>(entries_.size()) - 1);
            SelectEffect(selectedIndex_);
        }
    }

    std::string VfxMeshEditor::MakeUniqueEffectName(const std::string& base) const
    {
        auto used = [&](const std::string& n) {
            for (const auto& e : entries_) {
                if (e.asset.name == n) return true;
            }
            return false;
        };
        if (!used(base)) return base;
        for (int i = 1; i < 100000; ++i) {
            std::string cand = base + std::to_string(i);
            if (!used(cand)) return cand;
        }
        return base;
    }

    void VfxMeshEditor::CreateNew(const std::string& name,
                                  const std::string& filePath,
                                  VfxPreset          preset)
    {
        std::string uniqueName = MakeUniqueEffectName(name);
        std::string finalPath = filePath;
        if (uniqueName != name) finalPath = scanRoot_ + uniqueName + ".json";

        VfxEffectEntry e;
        e.asset = MakePreset(preset);
        e.asset.name = uniqueName;
        e.filePath = finalPath;
        e.isDirty = true;
        entries_.push_back(std::move(e));

        SelectEffect(static_cast<int>(entries_.size()) - 1);
        SaveCurrent();

        Logger("VfxMeshEditor: 新規作成 -> " + finalPath);
    }

    void VfxMeshEditor::CommitChange(const VfxEffectAsset& before, const char* label)
    {
        auto* sel = Selected();
        if (!sel) return;

        VfxEffectAsset after = sel->asset;
        const int idx = selectedIndex_;

        history_.Execute(MakeLambdaCommand(
            label,
            [this, idx, after]() {
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = after;
                    entries_[idx].isDirty = true;
                    if (previewTrailEmitter_) previewTrailEmitter_->SetAsset(after);
                }
            },
            [this, idx, before]() {
                if (idx < static_cast<int>(entries_.size())) {
                    entries_[idx].asset = before;
                    entries_[idx].isDirty = true;
                    if (previewTrailEmitter_) previewTrailEmitter_->SetAsset(before);
                }
            }
        ));

        sel->isDirty = true;
        if (previewTrailEmitter_) {
            previewTrailEmitter_->SetAsset(sel->asset);
        }
    }

} // namespace YoRigine
#endif
