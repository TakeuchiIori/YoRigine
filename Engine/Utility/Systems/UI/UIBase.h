#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include "Sprite/Sprite.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "json.hpp" 

#include <filesystem>
#include <algorithm>
#include <vector>
#include <string>

// Windows APIを使用する場合
#include <Windows.h>

// 前方宣言
class SpriteCommon;
class Camera;

class UIBase {
public:
    // コンストラクタ & デストラクタ
    UIBase(const std::string& name = "UIBase");
    virtual ~UIBase();

    // 初期化
    void Initialize(const std::string& jsonConfigPath);

    // コア機能
    virtual void Update();
    virtual void Draw();

    void ImGUi();

    // ホットリロード機能
    void EnableHotReload(bool enable);
    void CheckForChanges();

    // JSON設定
    bool LoadFromJSON(const std::string& jsonPath);
    bool SaveToJSON(const std::string& jsonPath = "");

    // トランスフォームアクセサ
    void SetPosition(const Vector3& position);
    Vector3 GetPosition() const;

    void SetRotation(const Vector3& rotation);
    Vector3 GetRotation() const;

    void SetScale(const Vector2& scale);
    Vector2 GetScale() const;

    // UIプロパティ
    void SetColor(const Vector4& color);
    Vector4 GetColor() const;

    void SetAlpha(float alpha);
    float GetAlpha() const;

    void SetTexture(const std::string& texturePath);
    std::string GetTexturePath() const;

    void SetCamera(Camera* camera);

    // UI要素の識別
    void SetName(const std::string& name);
    std::string GetName() const;

    // 反転制御用
    void SetFlipX(bool flipX);
    void SetFlipY(bool flipY);
    bool GetFlipX() const;
    bool GetFlipY() const;

    // 基盤となるスプライトポインタを返す
    Sprite* GetSprite() { return sprite_.get(); }

protected:
    // 基盤となるスプライト
    std::unique_ptr<Sprite> sprite_;

    // 設定ファイルパス
    std::string configPath_;

    // ホットリロード用の最終更新時間
    std::filesystem::file_time_type lastModTime_;

    // UIプロパティ
    std::string name_;
    std::string texturePath_;

    // ホットリロード設定
    bool hotReloadEnabled_;

    bool showFileBrowser = false;
    std::string currentDir;

    // JSONヘルパー
    nlohmann::json CreateJSONFromCurrentState();
    void ApplyJSONToState(const nlohmann::json& json);

    // ファイル変更の監視（ホットリロード用）
    void WatchFileChanges();
};