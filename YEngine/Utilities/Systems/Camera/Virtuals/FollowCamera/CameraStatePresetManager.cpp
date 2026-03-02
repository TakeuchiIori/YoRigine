#include "CameraStatePresetManager.h"
#include "CameraState.h"
#include "CinematicCameraState.h"
#include "ParryCameraState.h"
#include "BattleStartCameraState.h"
#include <fstream>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

CameraStatePresetManager* CameraStatePresetManager::GetInstance() {
    static CameraStatePresetManager instance;
    return &instance;
}

void CameraStatePresetManager::SavePreset(const std::string& presetName, const std::string& stateType, const CameraState* state) {
    if (!state) return;
    
    PresetData preset;
    preset.stateType = stateType;
    state->Save(preset.data);
    
    presets_[presetName] = preset;
}

std::unique_ptr<CameraState> CameraStatePresetManager::LoadPreset(const std::string& presetName) {
    auto it = presets_.find(presetName);
    if (it == presets_.end()) {
        return nullptr;
    }
    
    auto state = CreateStateFromType(it->second.stateType);
    if (state) {
        state->Load(it->second.data);
    }
    
    return state;
}

void CameraStatePresetManager::SaveToFile(const std::string& filepath) {
    nlohmann::json j;
    
    for (const auto& [name, preset] : presets_) {
        nlohmann::json presetJson;
        presetJson["stateType"] = preset.stateType;
        presetJson["data"] = preset.data;
        j[name] = presetJson;
    }
    
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
    }
}

void CameraStatePresetManager::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }
    
    nlohmann::json j;
    file >> j;
    file.close();
    
    presets_.clear();
    
    for (auto& [name, presetJson] : j.items()) {
        PresetData preset;
        preset.stateType = presetJson.value("stateType", "");
        preset.data = presetJson.value("data", nlohmann::json{});
        presets_[name] = preset;
    }
}

std::vector<std::string> CameraStatePresetManager::GetPresetNames() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : presets_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

void CameraStatePresetManager::DeletePreset(const std::string& presetName) {
    presets_.erase(presetName);
}

std::unique_ptr<CameraState> CameraStatePresetManager::CreateStateFromType(const std::string& stateType) {
    if (stateType == "Parry") {
        return std::make_unique<ParryCameraState>();
    }
    else if (stateType == "BattleStart") {
        return std::make_unique<BattleStartCameraState>();
    }
    else if (stateType == "Cinematic") {
        return std::make_unique<CinematicCameraState>();
    }
    return nullptr;
}

void CameraStatePresetManager::DrawPresetManagerGui() {
#ifdef USE_IMGUI
    ImGui::Begin("Camera State Preset Manager");
    
    ImGui::Text("プリセット管理");
    ImGui::Separator();
    
    // ファイル保存・読込
    static char filepath[256] = "camera_presets.json";
    ImGui::InputText("ファイルパス", filepath, sizeof(filepath));
    
    if (ImGui::Button("ファイルに保存")) {
        SaveToFile(filepath);
    }
    ImGui::SameLine();
    if (ImGui::Button("ファイルから読込")) {
        LoadFromFile(filepath);
    }
    
    ImGui::Separator();
    
    // プリセット一覧
    ImGui::Text("プリセット一覧:");
    
    auto presetNames = GetPresetNames();
    for (const auto& name : presetNames) {
        ImGui::PushID(name.c_str());
        
        bool isSelected = (selectedPreset_ == name);
        if (ImGui::Selectable(name.c_str(), isSelected)) {
            selectedPreset_ = name;
        }
        
        // 右クリックメニュー
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("削除")) {
                DeletePreset(name);
                if (selectedPreset_ == name) {
                    selectedPreset_.clear();
                }
            }
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    }
    
    ImGui::Separator();
    
    // 選択されたプリセットの情報表示
    if (!selectedPreset_.empty() && presets_.count(selectedPreset_) > 0) {
        ImGui::Text("選択中: %s", selectedPreset_.c_str());
        ImGui::Text("タイプ: %s", presets_[selectedPreset_].stateType.c_str());
    }
    
    ImGui::End();
#endif
}
