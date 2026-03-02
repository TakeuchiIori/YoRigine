#pragma once
#include <string>
#include <memory>

class CameraEditor {
public:
    void Initialize();
    void LoadFileOrDefault(const std::string& filePath, const std::string& sceneType);

    void Update(); // 毎フレーム呼ぶ ImGui描画

    // シーン全体のカメラ構成を保存・読込
    void SaveFile(const std::string& filePath);
    void LoadFile(const std::string& filePath);


    // シーンごとにパスを設定できるようにする
    void SetFilePath(const std::string& path) { filePath_ = path; }
    const std::string& GetFilePath() const { return filePath_; }

private:
    void InitializeDefaults(const std::string& sceneType);

private:
    std::string selectedCameraName_; // 現在選択中のカメラ
    char newCameraName_[64] = "NewCamera";
    std::string filePath_ = "Resources/Json/VirtualCameraData/VirtualCameras.json"; // デフォルト
};

