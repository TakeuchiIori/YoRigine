#include "UIBase.h"
#include "UIBase.h"
#include <fstream>
#include <chrono>
#include <thread>
#include "Sprite/SpriteCommon.h"

#ifdef _DEBUG
#include <imgui.h>
#endif // _DEBUG


UIBase::UIBase(const std::string& name) :
    sprite_(nullptr),
    hotReloadEnabled_(false),
    name_(name) {
}

UIBase::~UIBase() {
    // 設定パスがある場合は現在の状態を保存
    if (!configPath_.empty()) {
       // SaveToJSON();
    }
}

void UIBase::Initialize(const std::string& jsonConfigPath) {
    configPath_ = jsonConfigPath;

    // スプライトを作成
    sprite_ = std::make_unique<Sprite>();

    // JSONファイルが存在するか確認
    bool jsonExists = std::filesystem::exists(jsonConfigPath);

    if (jsonExists) {
        // JSONファイルが存在する場合は読み込み
        LoadFromJSON(jsonConfigPath);
    } else {
        // JSONファイルが存在しない場合はデフォルト初期化
        sprite_->Initialize("./Resources/images/white.png");
        texturePath_ = "./Resources/images/white.png";

        // デフォルト設定でJSONファイルを作成
        SaveToJSON();
    }

    // ホットリロード用に初期ファイル更新時間を保存
    if (std::filesystem::exists(configPath_)) {
        lastModTime_ = std::filesystem::last_write_time(configPath_);
    }
}


void UIBase::Update() {
    // ホットリロードが有効な場合はJSONファイルの変更を確認
    if (hotReloadEnabled_) {
        CheckForChanges();
    }

    // スプライトを更新
    if (sprite_) {
        sprite_->Update();
    }

#ifdef _DEBUG
    ImGUi();
#endif // _DEBUG

}

void UIBase::Draw() {
    if (sprite_) {
        sprite_->Draw();
    }
}

void UIBase::ImGUi() {
    if (!sprite_) return;
    ImGui::Begin(name_.c_str());

    bool modified = false;  // 変更があったかどうかを追跡するフラグ

    // UI名前設定
    char nameBuffer[256];
    strncpy_s(nameBuffer, name_.c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';
    if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
        name_ = nameBuffer;
        modified = true;
    }

    // トランスフォーム設定
    if (ImGui::CollapsingHeader("変形", ImGuiTreeNodeFlags_DefaultOpen)) {
       
        // スケール設定
        Vector2 scale = GetScale();
        if (ImGui::DragFloat2("拡大縮小", &scale.x, 0.5f)) {
            SetScale(scale);
            modified = true;
        }
        
        // 回転設定
        Vector3 rotation = GetRotation();
        if (ImGui::DragFloat3("回転", &rotation.x, 0.1f)) {
            SetRotation(rotation);
            modified = true;
        }

        // 位置設定
        Vector3 position = GetPosition();
        if (ImGui::DragFloat3("位置", &position.x, 1.0f)) {
            SetPosition(position);
            modified = true;
        }
    }

    // 外観設定
    if (ImGui::CollapsingHeader("外観", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 色設定（RGBA）
        Vector4 color = GetColor();
        if (ImGui::ColorEdit4("色", &color.x)) {
            SetColor(color);
            modified = true;
        }

        // X軸反転設定
        bool flipX = GetFlipX();
        if (ImGui::Checkbox("X軸反転", &flipX)) {
            SetFlipX(flipX);
            modified = true;
        }

        ImGui::SameLine();

        // Y軸反転設定
        bool flipY = GetFlipY();
        if (ImGui::Checkbox("Y軸反転", &flipY)) {
            SetFlipY(flipY);
            modified = true;
        }
    }

    // テクスチャ設定
    if (ImGui::CollapsingHeader("テクスチャ", ImGuiTreeNodeFlags_DefaultOpen)) {
        // 現在のテクスチャパスを表示
        ImGui::Text("現在のテクスチャ: %s", texturePath_.c_str());
        // テクスチャフォルダ選択ダイアログ
        static char dialogCurrentPath[512] = "";
        static bool showHiddenFiles = false;
        static std::vector<std::string> dialogFiles;
        static std::vector<std::string> dialogDirs;
        static char navigationPath[512] = "";
        static bool needsRefresh = true;

        // テクスチャファイル選択ボタン
        if (ImGui::Button("テクスチャフォルダ選択")) {
            ImGui::OpenPopup("TextureFolderDialog");
            // 初回表示時または再表示時にダイアログの状態を初期化
            static bool initialized = false;
            if (!initialized) {
                // 現在のパスがあればそれを初期値に
                if (!texturePath_.empty()) {
                    strncpy_s(dialogCurrentPath, std::filesystem::path(texturePath_).parent_path().string().c_str(), sizeof(dialogCurrentPath) - 1);
                }
                initialized = true;
            }
        }



        // ダイアログのモーダルウィンドウ
        if (ImGui::BeginPopupModal("TextureFolderDialog", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            // パス表示・編集バー
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60);
            if (ImGui::InputText("##Path", dialogCurrentPath, sizeof(dialogCurrentPath), ImGuiInputTextFlags_EnterReturnsTrue)) {
                needsRefresh = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("移動")) {
                needsRefresh = true;
            }

            // 親ディレクトリへの移動ボタン
            if (ImGui::Button("上へ")) {
                std::string currentPath = dialogCurrentPath;
                std::filesystem::path path(currentPath);
                if (path.has_parent_path()) {
                    path = path.parent_path();
                    strncpy_s(dialogCurrentPath, path.string().c_str(), sizeof(dialogCurrentPath) - 1);
                    needsRefresh = true;
                }
            }
            ImGui::SameLine();
            ImGui::Checkbox("隠しファイルを表示", &showHiddenFiles);

            // ファイルリストを更新
            if (needsRefresh) {
                dialogFiles.clear();
                dialogDirs.clear();

                try {
                    // 現在のパスからファイルとディレクトリを取得
                    std::filesystem::path currentPath(dialogCurrentPath);
                    if (std::filesystem::exists(currentPath) && std::filesystem::is_directory(currentPath)) {
                        for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
                            std::string filename = entry.path().filename().string();

                            // 隠しファイルのフィルタリング（先頭が.のファイル）
                            if (!showHiddenFiles && filename[0] == '.') {
                                continue;
                            }

                            if (entry.is_directory()) {
                                dialogDirs.push_back(filename);
                            } else if (entry.is_regular_file()) {
                                std::string ext = entry.path().extension().string();
                                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                                    dialogFiles.push_back(filename);
                                }
                            }
                        }

                        // アルファベット順にソート
                        std::sort(dialogDirs.begin(), dialogDirs.end());
                        std::sort(dialogFiles.begin(), dialogFiles.end());
                    }
                }
                catch (const std::exception& e) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "エラー: %s", e.what());
                }

                needsRefresh = false;
            }

            // ファイルブラウザ表示部分
            float listHeight = 300.0f;
            ImGui::BeginChild("FileBrowser", ImVec2(500, listHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

            // ディレクトリの表示
            for (const auto& dir : dialogDirs) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f)); // フォルダは黄色で表示
                if (ImGui::Selectable(("[DIR] " + dir).c_str(), false)) {
                    // ディレクトリをクリックしたら、そのディレクトリに移動
                    std::string newPath = std::string(dialogCurrentPath) + "/" + dir;
                    strncpy_s(dialogCurrentPath, newPath.c_str(), sizeof(dialogCurrentPath) - 1);
                    needsRefresh = true;
                }
                ImGui::PopStyleColor();
            }

            // テクスチャファイルの表示
            static std::string selectedFile = "";
            for (const auto& file : dialogFiles) {
                bool isSelected = (selectedFile == file);
                if (ImGui::Selectable(file.c_str(), isSelected)) {
                    selectedFile = file;
                    // 選択されたファイルのフルパスを作成
                    std::string fullPath = std::string(dialogCurrentPath) + "/" + file;
                    strncpy_s(navigationPath, fullPath.c_str(), sizeof(navigationPath) - 1);
                }

                // ダブルクリックで選択確定
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    std::string fullPath = std::string(dialogCurrentPath) + "/" + file;
                    strncpy_s(navigationPath, fullPath.c_str(), sizeof(navigationPath) - 1);
                    // 選択して閉じる
                    SetTexture(navigationPath);
                    modified = true;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndChild();

            // 現在選択されているファイルの表示
            ImGui::Text("選択: %s", navigationPath);

            // ボタン：選択・キャンセル
            if (ImGui::Button("選択", ImVec2(120, 0))) {
                if (strlen(navigationPath) > 0) {
                    SetTexture(navigationPath);
                    modified = true;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("キャンセル", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    

        // テクスチャ座標設定
        Vector2 leftTop = sprite_->GetTextureLeftTop();
        if (ImGui::DragFloat2("左上座標", &leftTop.x, 1.0f)) {
            sprite_->SetTextureLeftTop(leftTop);
            modified = true;
        }

        // アンカーポイント設定（0.0〜1.0の範囲）
        Vector2 anchor = sprite_->GetAnchorPoint();
        if (ImGui::DragFloat2("アンカーポイント", &anchor.x, 0.01f, 0.0f, 1.0f)) {
            sprite_->SetAnchorPoint(anchor);
            modified = true;
        }
    }

    // ホットリロード設定
    bool hotReload = hotReloadEnabled_;
    if (ImGui::Checkbox("ホットリロード", &hotReload)) {
        EnableHotReload(hotReload);
    }

    // 区切り線
    ImGui::Separator();

    // 変更保存ボタン
    if (ImGui::Button("変更を保存")) {
        if (SaveToJSON()) {
            ImGui::OpenPopup("SaveSuccessPopup");
        } else {
            ImGui::OpenPopup("SaveFailedPopup");
        }
    }

    // 保存成功ポップアップ
    if (ImGui::BeginPopupModal("SaveSuccessPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("設定が正常に保存されました。");
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 保存失敗ポップアップ
    if (ImGui::BeginPopupModal("SaveFailedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("設定の保存に失敗しました。");
        if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 変更があったことを視覚的に通知（オプション）
    if (modified) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "* 未保存の変更があります");
    }

    // ウィンドウ終了
    ImGui::End();
}

void UIBase::EnableHotReload(bool enable) {
    hotReloadEnabled_ = enable;
}

void UIBase::CheckForChanges() {
    if (configPath_.empty() || !std::filesystem::exists(configPath_)) {
        return;
    }

    auto currentModTime = std::filesystem::last_write_time(configPath_);

    // ファイルが変更された場合
    if (currentModTime != lastModTime_) {
        // JSONから再読み込み
        LoadFromJSON(configPath_);
        // 更新時間を更新
        lastModTime_ = currentModTime;
    }
}

bool UIBase::LoadFromJSON(const std::string& jsonPath) {
    try {
        // JSONファイルを開く
        std::ifstream file(jsonPath);
        if (!file.is_open()) {
            return false;
        }

        // JSONをパース
        nlohmann::json data;
        file >> data;
        file.close();

        // JSONを現在の状態に適用
        ApplyJSONToState(data);

        return true;
    }
    catch (const std::exception& e) {
        // エラーをログに記録（あなたのロギングシステムに置き換えてください）
        printf("JSONからUIの読み込み中にエラー発生: %s\n", e.what());
        return false;
    }
}

bool UIBase::SaveToJSON(const std::string& jsonPath) {
    std::string savePath = jsonPath.empty() ? configPath_ : jsonPath;

    if (savePath.empty()) {
        return false;
    }

    try {
        // 保存先ディレクトリが存在するか確認
        std::filesystem::path dirPath = std::filesystem::path(savePath).parent_path();

        // ディレクトリが存在しない場合は作成
        if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
            std::filesystem::create_directories(dirPath);
        }

        // 現在の状態からJSONを作成
        nlohmann::json data = CreateJSONFromCurrentState();

        // ファイルに書き込み
        std::ofstream file(savePath);
        if (!file.is_open()) {
            return false;
        }

        file << std::setw(4) << data << std::endl;
        file.close();

        return true;
    }
    catch (const std::exception& e) {
        // エラーをログに記録
        printf("JSONへのUI保存中にエラー発生: %s\n", e.what());
        return false;
    }
}

void UIBase::SetPosition(const Vector3& position) {
    if (sprite_) {
        sprite_->SetPosition(position);
    }
}

Vector3 UIBase::GetPosition() const {
    if (sprite_) {
        return sprite_->GetPosition();
    }
    return { 0.0f, 0.0f, 0.0f };
}

void UIBase::SetRotation(const Vector3& rotation) {
    if (sprite_) {
        sprite_->SetRotation(rotation);
    }
}

Vector3 UIBase::GetRotation() const {
    if (sprite_) {
        return sprite_->GetRotation();
    }
    return { 0.0f, 0.0f, 0.0f };
}

void UIBase::SetScale(const Vector2& scale) {
    if (sprite_) {
        sprite_->SetSize(scale);
    }
}

Vector2 UIBase::GetScale() const {
    if (sprite_) {
        return sprite_->GetSize();
    }
    return { 1.0f, 1.0f };
}

void UIBase::SetColor(const Vector4& color) {
    if (sprite_) {
        sprite_->SetColor(color);
    }
}

Vector4 UIBase::GetColor() const {
    if (sprite_) {
        return sprite_->GetColor();
    }
    return { 1.0f, 1.0f, 1.0f, 1.0f };
}

void UIBase::SetAlpha(float alpha) {
    if (sprite_) {
        sprite_->SetAlpha(alpha);
    }
}

float UIBase::GetAlpha() const {
    if (sprite_) {
        return sprite_->GetColor().w;
    }
    return 1.0f;
}

void UIBase::SetTexture(const std::string& texturePath) {
    if (sprite_) {
        sprite_->ChangeTexture(texturePath);
        texturePath_ = texturePath;
    }
}

std::string UIBase::GetTexturePath() const {
    return texturePath_;
}

void UIBase::SetCamera(Camera* camera) {
    if (sprite_) {
        sprite_->SetCamera(camera);
    }
}

void UIBase::SetName(const std::string& name) {
    name_ = name;
}

std::string UIBase::GetName() const {
    return name_;
}

void UIBase::SetFlipX(bool flipX) {
    if (sprite_) {
        sprite_->SetIsFlipX(flipX);
    }
}

void UIBase::SetFlipY(bool flipY) {
    if (sprite_) {
        sprite_->SetIsFlipY(flipY);
    }
}

bool UIBase::GetFlipX() const {
    if (sprite_) {
        return sprite_->GetIsFlipX();
    }
    return false;
}

bool UIBase::GetFlipY() const {
    if (sprite_) {
        return sprite_->GetIsFlipY();
    }
    return false;
}

nlohmann::json UIBase::CreateJSONFromCurrentState() {
    nlohmann::json data;

    // 基本プロパティ
    data["name"] = name_;
    data["texturePath"] = texturePath_;

    // トランスフォーム
    data["position"] = {
        {"x", GetPosition().x},
        {"y", GetPosition().y},
        {"z", GetPosition().z}
    };

    data["rotation"] = {
        {"x", GetRotation().x},
        {"y", GetRotation().y},
        {"z", GetRotation().z}
    };

    data["scale"] = {
        {"x", GetScale().x},
        {"y", GetScale().y}
    };

    // 色
    data["color"] = {
        {"r", GetColor().x},
        {"g", GetColor().y},
        {"b", GetColor().z},
        {"a", GetColor().w}
    };

    // 反転状態
    data["flipX"] = GetFlipX();
    data["flipY"] = GetFlipY();

    // テクスチャプロパティ
    if (sprite_) {
        data["textureLeftTop"] = {
            {"x", sprite_->GetTextureLeftTop().x},
            {"y", sprite_->GetTextureLeftTop().y}
        };

        //data["textureSize"] = {
        //    {"x", sprite_->GetTextureSize().x},
        //    {"y", sprite_->GetTextureSize().y}
        //};

        data["anchorPoint"] = {
            {"x", sprite_->GetAnchorPoint().x},
            {"y", sprite_->GetAnchorPoint().y}
        };
    }

    return data;
}

void UIBase::ApplyJSONToState(const nlohmann::json& data) {
    // テクスチャパスがある場合、スプライトを初期化
    if (data.contains("texturePath")) {
        texturePath_ = data["texturePath"];

        // スプライトがまだ作成されていない場合は初期化
        if (!sprite_) {
            sprite_ = std::make_unique<Sprite>();
            sprite_->Initialize(texturePath_);
        } else {
            // すでにある場合はテクスチャを変更
            sprite_->Initialize(texturePath_);
            //sprite_->ChangeTexture(texturePath_);
        }
    } else if (!sprite_) {
        // テクスチャが指定されていない場合はデフォルトテクスチャで初期化
        sprite_ = std::make_unique<Sprite>();
        sprite_->Initialize("./Resources/images/white.png");
        texturePath_ = "./Resources/images/white.png";
    }

    // 名前
    if (data.contains("name")) {
        name_ = data["name"];
    }

    // トランスフォーム
    if (data.contains("position")) {
        Vector3 position;
        position.x = data["position"]["x"];
        position.y = data["position"]["y"];
        position.z = data["position"]["z"];
        SetPosition(position);
    }

    if (data.contains("rotation")) {
        Vector3 rotation;
        rotation.x = data["rotation"]["x"];
        rotation.y = data["rotation"]["y"];
        rotation.z = data["rotation"]["z"];
        SetRotation(rotation);
    }

    if (data.contains("scale")) {
        Vector2 scale;
        scale.x = data["scale"]["x"];
        scale.y = data["scale"]["y"];
        SetScale(scale);
    }

    // 色
    if (data.contains("color")) {
        Vector4 color;
        color.x = data["color"]["r"];
        color.y = data["color"]["g"];
        color.z = data["color"]["b"];
        color.w = data["color"]["a"];
        SetColor(color);
    }

    // 反転状態
    if (data.contains("flipX")) {
        SetFlipX(data["flipX"]);
    }

    if (data.contains("flipY")) {
        SetFlipY(data["flipY"]);
    }

    // テクスチャプロパティ
    if (sprite_) {
        if (data.contains("textureLeftTop")) {
            Vector2 leftTop;
            leftTop.x = data["textureLeftTop"]["x"];
            leftTop.y = data["textureLeftTop"]["y"];
            sprite_->SetTextureLeftTop(leftTop);
        }

        //if (data.contains("textureSize")) {
        //    Vector2 size;
        //    size.x = data["textureSize"]["x"];
        //    size.y = data["textureSize"]["y"];
        //    //sprite_->SetTextureSize(size);
        //}

        if (data.contains("anchorPoint")) {
            Vector2 anchor;
            anchor.x = data["anchorPoint"]["x"];
            anchor.y = data["anchorPoint"]["y"];
            sprite_->SetAnchorPoint(anchor);
        }
    }
}

void UIBase::WatchFileChanges() {
    // これは適切なファイル監視システムで改善する必要があります
    // これは現在updateメソッドでの単純なチェックです
}