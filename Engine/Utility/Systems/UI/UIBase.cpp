#include "UIBase.h"
#include "UIBase.h"
#include <fstream>
#include <chrono>
#include <thread>
#include "Sprite/SpriteCommon.h"

UIBase::UIBase() :
    sprite_(nullptr),
    hotReloadEnabled_(false),
    name_("UIElement") {
}

UIBase::~UIBase() {
    // 設定パスがある場合は現在の状態を保存
    if (!configPath_.empty()) {
        SaveToJSON();
    }
}

void UIBase::Initialize(const std::string& jsonConfigPath) {
    configPath_ = jsonConfigPath;

    // スプライトを作成
    sprite_ = std::make_unique<Sprite>();

    // まずJSONからの読み込みを試みる
    if (!LoadFromJSON(jsonConfigPath)) {
        // JSONの読み込みに失敗した場合はデフォルト初期化
        sprite_->Initialize("./Assets/Images/default.png");
        texturePath_ = "./Assets/Images/default.png";
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
}

void UIBase::Draw() {
    if (sprite_) {
        sprite_->Draw();
    }
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
        // エラーをログに記録（あなたのロギングシステムに置き換えてください）
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

        data["textureSize"] = {
            {"x", sprite_->GetTextureSize().x},
            {"y", sprite_->GetTextureSize().y}
        };

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
            sprite_->ChangeTexture(texturePath_);
        }
    } else if (!sprite_) {
        // テクスチャが指定されていない場合はデフォルトテクスチャで初期化
        sprite_ = std::make_unique<Sprite>();
        sprite_->Initialize("./Assets/Images/default.png");
        texturePath_ = "./Assets/Images/default.png";
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

        if (data.contains("textureSize")) {
            Vector2 size;
            size.x = data["textureSize"]["x"];
            size.y = data["textureSize"]["y"];
            sprite_->SetTextureSize(size);
        }

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