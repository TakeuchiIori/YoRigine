#pragma once

// C++
#include <vector>
#include <memory>
#include <string>
#include <functional>

// Engine
#include "MathFunc.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include <Systems/UI/UINumber.h>
#include <Loaders/Json/JsonManager.h>

// ============================================================
// 単一のダメージポップアップエントリ（プール用）
// ============================================================
struct DamagePopupEntry {
    std::unique_ptr<UINumber> number;
    Vector3 worldPos;
    float   timer = 0.0f;
    float   yOffset = 0.0f;
    float   xJitter = 0.0f;
    bool    active = false;
    bool    isSine = false;
};

// ============================================================
// ダメージ数値UIの管理クラス（シングルトン）
//
// 使用方法:
//   1. BattleScene::Initialize() で GetInstance()->Initialize() を呼ぶ
//   2. 敵ヒット時に SpawnDamage(damage, worldPos, isSine) を呼ぶ
//   3. BattleScene::Update() で Update(deltaTime, viewProj) を呼ぶ
//   4. BattleScene::DrawUI() で Draw() を呼ぶ
// ============================================================
class DamageNumberManager {
public:
    static DamageNumberManager* GetInstance();

    void Initialize();
    void Update(float deltaTime, const Matrix4x4& viewProj);
    void Draw();

    // damage   : ダメージ値
    // worldPos : ヒット座標（ワールド空間）
    // isSine   : comboDamageMultiplier > 1.0f のとき true → sineシート使用
    void SpawnDamage(int damage, const Vector3& worldPos, bool isSine = false);

private:
    DamageNumberManager() = default;
    DamageNumberManager(const DamageNumberManager&) = delete;
    DamageNumberManager& operator=(const DamageNumberManager&) = delete;


    void InitPool(std::vector<DamagePopupEntry>& pool,
        const std::string& texturePath, bool isSine);

    DamagePopupEntry* FindFreeEntry(std::vector<DamagePopupEntry>& pool);
    void SpawnIntoPool(std::vector<DamagePopupEntry>& pool,
        int damage, const Vector3& worldPos, bool isSine);

    void UpdatePool(std::vector<DamagePopupEntry>& pool,
        float deltaTime, const Matrix4x4& viewProj);

    void InitJson();

    // ============================================================
    // アニメーション定数
    // ============================================================
    static constexpr int   kPoolSize = 16;
    static constexpr float kMaxLifetime = 1.0f;         // ポップアップの総寿命（秒）
    static constexpr float kFloatDistance = 100.0f;     // 浮き上がる総距離（px）
    static constexpr float kPopSettle = 0.15f;          // ポップインが終わる時刻（正規化）
    static constexpr float kFadeStart = 0.70f;          // フェードアウト開始時刻（正規化）

    // ============================================================
    // テクスチャパス
    // ============================================================
    inline static const std::string kNormalSheet =
        "Resources/Textures/Option/attack_number_sheet.png";
    inline static const std::string kSineSheet =
        "Resources/Textures/Option/attack_number_sheet_sine.png";

    // ============================================================
    // メンバ変数
    // ============================================================
    std::vector<DamagePopupEntry> normalPool_;
    std::vector<DamagePopupEntry> sinePool_;
	std::unique_ptr<YoRigine::JsonManager> jsonManager_;

	// JSON調整可能なパラメータ群
	float offsetY_       = 5.0f;   // ワールド→スクリーン変換後のYオフセット（px）
	float worldOffsetY_  = 3.0f;   // SpawnDamage に渡されたワールド座標へ加算するY（敵の頭上）
	float digitHeight_   = 50.0f;  // 通常ダメージの1桁表示サイズ（px）
	float sineScaleMult_ = 1.3f;   // sine（倍率攻撃）のサイズ倍率
};