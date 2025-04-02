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

#include <Windows.h>

// 前方宣言
class SpriteCommon;
class Camera;

class UIBase {
public:
    /// <summary>
    ///  コンストラクタ
    /// </summary>
    UIBase(const std::string& name = "UIBase");

    /// <summary>
    ///  デストラクタ
    /// </summary>
    virtual ~UIBase();

    /// <summary>
    ///  初期化処理。JSON設定を読み込みスプライトを構成
    /// </summary>
    void Initialize(const std::string& jsonConfigPath);

    /// <summary>
    ///  毎フレーム更新処理（ホットリロードなど）
    /// </summary>
    virtual void Update();

    /// <summary>
    ///  描画処理
    /// </summary>
    virtual void Draw();

    /// <summary>
    ///  デバッグ用のImGuiウィンドウ表示
    /// </summary>
    void ImGUi();

private:

    /// <summary>
    ///  ホットリロードを有効/無効にする
    /// </summary>
    void EnableHotReload(bool enable);

    /// <summary>
    ///  設定ファイルの変更チェックと再読み込み
    /// </summary>
    void CheckForChanges();

    /// <summary>
    ///  JSONファイルから設定を読み込む
    /// </summary>
    bool LoadFromJSON(const std::string& jsonPath);

public:
    /*==================================================================
    
                                アクセッサ
    
    ===================================================================*/

    /// <summary>
    ///  JSONファイルに現在の設定を保存
    /// </summary>
    bool SaveToJSON(const std::string& jsonPath = "");

    /// <summary>
    ///  UIの位置を設定
    /// </summary>
    void SetPosition(const Vector3& position);
    Vector3 GetPosition() const;

    /// <summary>
    ///  UIの回転を設定
    /// </summary>
    void SetRotation(const Vector3& rotation);
    Vector3 GetRotation() const;

    /// <summary>
    ///  UIのスケールを設定
    /// </summary>
    void SetScale(const Vector2& scale);
    Vector2 GetScale() const;

    /// <summary>
    ///  色（RGBA）を設定
    /// </summary>
    void SetColor(const Vector4& color);
    Vector4 GetColor() const;

    /// <summary>
    ///  アルファ値のみを設定
    /// </summary>
    void SetAlpha(float alpha);
    float GetAlpha() const;

    /// <summary>
    ///  テクスチャを変更
    /// </summary>
    void SetTexture(const std::string& texturePath);
    std::string GetTexturePath() const;

    /// <summary>
    ///  使用するカメラを設定
    /// </summary>
    void SetCamera(Camera* camera);

    /// <summary>
    ///  UIの名前を設定
    /// </summary>
    void SetName(const std::string& name);
    std::string GetName() const;

    /// <summary>
    ///  軸反転を設定
    /// </summary>
    void SetFlipX(bool flipX);
    void SetFlipY(bool flipY);

    /// <summary>
    ///  軸反転の状態を取得
    /// </summary>
    bool GetFlipX() const;
    bool GetFlipY() const;

    /// <summary>
    ///  スプライトポインタを取得
    /// </summary>
    Sprite* GetSprite() { return sprite_.get(); }

    /// <summary>
    /// テクスチャの左上UV座標を設定
    /// </summary>
    void SetTextureLeftTop(const Vector2& leftTop);
    Vector2 GetTextureLeftTop() const;

    /// <summary>
    /// テクスチャのサイズ（UV範囲）を設定
    /// </summary>
    void SetTextureSize(const Vector2& size);
    Vector2 GetTextureSize() const;

    /// <summary>
    /// アンカーポイントを設定
    /// </summary>
    void SetAnchorPoint(const Vector2& anchor);
    Vector2 GetAnchorPoint() const;

protected:
    std::unique_ptr<Sprite> sprite_;                     // スプライト本体
    std::string configPath_;                             // JSON設定ファイルパス
    std::filesystem::file_time_type lastModTime_;        // 最終更新時刻（ホットリロード用）
    std::string name_;                                   // UI名
    std::string texturePath_;                            // テクスチャパス
    bool hotReloadEnabled_;                              // ホットリロード有効フラグ

    bool showFileBrowser = false;                        // ImGuiファイルブラウザ表示フラグ
    std::string currentDir;                              // ファイルブラウザの現在のディレクトリ

    /// <summary>
    ///  現在の状態をJSON形式で作成
    /// </summary>
    nlohmann::json CreateJSONFromCurrentState();

    /// <summary>
    ///  JSONから現在の状態に適用
    /// </summary>
    void ApplyJSONToState(const nlohmann::json& json);

    /// <summary>
    ///  ファイル変更の監視（未使用または将来的な拡張用）
    /// </summary>
    void WatchFileChanges();
};
