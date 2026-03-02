#pragma once
#include "json.hpp"
#include <string>
#include <map>
#include <memory>

class CameraState;

/// <summary>
/// カメラステートのプリセット管理
/// </summary>
class CameraStatePresetManager {
public:
    static CameraStatePresetManager* GetInstance();
    
    // プリセットの保存・読込
    void SavePreset(const std::string& presetName, const std::string& stateType, const CameraState* state);
    std::unique_ptr<CameraState> LoadPreset(const std::string& presetName);
    
    // ファイルへの保存・読込
    void SaveToFile(const std::string& filepath);
    void LoadFromFile(const std::string& filepath);
    
    // プリセット一覧の取得
    std::vector<std::string> GetPresetNames() const;
    
    // プリセットの削除
    void DeletePreset(const std::string& presetName);
    
    // ImGuiでの編集UI
    void DrawPresetManagerGui();
    
private:
    CameraStatePresetManager() = default;
    ~CameraStatePresetManager() = default;
    CameraStatePresetManager(const CameraStatePresetManager&) = delete;
    CameraStatePresetManager& operator=(const CameraStatePresetManager&) = delete;
    
    // ステートタイプからインスタンスを生成
    std::unique_ptr<CameraState> CreateStateFromType(const std::string& stateType);
    
    struct PresetData {
        std::string stateType;
        nlohmann::json data;
    };
    
    std::map<std::string, PresetData> presets_;
    std::string selectedPreset_;
};
