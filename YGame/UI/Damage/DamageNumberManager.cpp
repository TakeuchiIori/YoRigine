#include "DamageNumberManager.h"

// Engine
#include "Systems/GameTime/GameTime.h"

// C++
#include <cmath>
#include <cstdlib>
#include <algorithm>

// ============================================================
// シングルトン取得
// ============================================================
DamageNumberManager* DamageNumberManager::GetInstance() {
    static DamageNumberManager instance;
    return &instance;
}

// ============================================================
// 初期化
// ============================================================
void DamageNumberManager::Initialize() {
    InitPool(normalPool_, kNormalSheet, false);
    InitPool(sinePool_, kSineSheet, true);
}

// ============================================================
// プール初期化ヘルパー
// ============================================================
void DamageNumberManager::InitPool(
    std::vector<DamagePopupEntry>& pool,
    const std::string& texturePath,
    bool isSine)
{
    pool.resize(kPoolSize);

    for (auto& entry : pool) {
        entry.number = std::make_unique<UINumber>("DamageNum");
        entry.number->Initialize(texturePath, /*maxDigits=*/6);
        entry.number->SetAlignment(UINumber::Alignment::Center);
        // ※ SetDigitSize はここでは呼ばない
        //    digitSprites_ がまだ空なので SetSize は効かず digitSize_ だけ変わる。
        //    サイズ設定は SpawnIntoPool で SetNumber の直前に行う。
        entry.number->SetVisible(false);
        entry.active = false;
        entry.isSine = isSine;
    }
}

// ============================================================
// 毎フレーム更新
// ============================================================
void DamageNumberManager::Update(float deltaTime, const Matrix4x4& viewProj) {
    UpdatePool(normalPool_, deltaTime, viewProj);
    UpdatePool(sinePool_, deltaTime, viewProj);
}

// ============================================================
// プール更新ヘルパー
// MHW風アニメーション:
//   0     〜 kPopSettle : アルファ 0→1 (素早くポップイン)
//   kPopSettle〜kFadeStart : α=1 維持、上方向に浮き上がり
//   kFadeStart〜 1.0    : フェードアウトしながら浮き続ける
//
// !! SetDigitSize はここで呼ばない !!
//    毎フレーム SetDigitSize を呼ぶと Sprite::SetSize が
//    ApplyDigitUV で設定した UV(TextureSize) を上書きしてしまい、
//    全10桁が映る / t=0でサイズ0になって消えるなどの不具合が起きる。
// ============================================================
void DamageNumberManager::UpdatePool(
    std::vector<DamagePopupEntry>& pool,
    float deltaTime,
    const Matrix4x4& viewProj)
{
    for (auto& entry : pool) {
        if (!entry.active) continue;

        entry.timer += deltaTime;
        const float t = entry.timer / kMaxLifetime;

        // 寿命終了
        if (t >= 1.0f) {
            entry.active = false;
            entry.number->SetVisible(false);
            continue;
        }

        // ── スクリーン座標を毎フレーム計算（カメラが動いても追従） ──
        auto projected = Coordinate::WorldToScreen(entry.worldPos, viewProj);
        if (!projected.has_value()) {
            entry.number->SetVisible(false);
            continue;
        }
        const Vector3& screenPos = projected->screenPos;

        // ── 浮き上がりアニメーション（ease-out: sqrt） ────────────
        const float floatT = std::sqrtf(t);
        entry.yOffset = -kFloatDistance * floatT;

        // ── アルファアニメーション ─────────────────────────────────
        // ポップイン : 0 〜 kPopSettle  → 0→1
        // 維持       : kPopSettle〜kFadeStart → 1
        // フェードアウト: kFadeStart〜1.0 → 1→0
        float alpha = 1.0f;
        if (t < kPopSettle) {
            alpha = t / kPopSettle;
        }
        else if (t > kFadeStart) {
            alpha = 1.0f - (t - kFadeStart) / (1.0f - kFadeStart);
        }
        alpha = std::max(0.0f, alpha);

        // ── UINumber へ反映 ─────────────────────────────────────
        entry.number->SetPosition({ screenPos.x + entry.xJitter,
                                    screenPos.y + entry.yOffset, 0.0f });
        entry.number->SetAlpha(alpha);
        entry.number->SetVisible(true);
        entry.number->Update();
    }
}

// ============================================================
// 描画
// ============================================================
void DamageNumberManager::Draw() {
    for (auto& entry : normalPool_) {
        if (entry.active) entry.number->Draw();
    }
    for (auto& entry : sinePool_) {
        if (entry.active) entry.number->Draw();
    }
}

// ============================================================
// ダメージポップアップをスポーン
// ============================================================
void DamageNumberManager::SpawnDamage(int damage, const Vector3& worldPos, bool isSine) {
    if (isSine) {
        SpawnIntoPool(sinePool_, damage, worldPos, true);
    }
    else {
        SpawnIntoPool(normalPool_, damage, worldPos, false);
    }
}

// ============================================================
// プールへの登録処理
//
// [重要] 呼び出し順序:
//   1. SetDigitSize  → digitSize_ と既存スプライトの SetSize を更新
//   2. SetNumber     → RebuildDigits → ApplyDigitUV で UV (SetTextureLeftTop/SetTextureSize)
//                      を正しく再設定する
//
//   SetDigitSize の後に SetNumber を呼ぶことで、
//   SetSize が UV を上書きしても ApplyDigitUV が正しい値に戻す。
// ============================================================
void DamageNumberManager::SpawnIntoPool(
    std::vector<DamagePopupEntry>& pool,
    int damage,
    const Vector3& worldPos,
    bool isSine)
{
    DamagePopupEntry* entry = FindFreeEntry(pool);
    if (!entry) return;

    entry->active = true;
    entry->isSine = isSine;
    entry->timer = 0.0f;
    entry->yOffset = 0.0f;
    entry->worldPos = worldPos;
    entry->xJitter = static_cast<float>((std::rand() % 49) - 24);

    // [1] 表示サイズを設定（sineは通常より大きく）
    const float displaySize = isSine ? kDigitHeight * kSineScaleMult : kDigitHeight;
    entry->number->SetDigitSize({ displaySize, displaySize });

    // [2] 数値設定 → RebuildDigits → ApplyDigitUV で UV を正しく再設定
    //     これにより SetDigitSize が UV を汚した場合でも正しく戻る
    entry->number->SetNumber(damage);

    entry->number->SetAlpha(0.0f);   // ポップイン開始はα=0
    entry->number->SetVisible(true);
}

// ============================================================
// 空きエントリを探す（なければ最古を再利用）
// ============================================================
DamagePopupEntry* DamageNumberManager::FindFreeEntry(std::vector<DamagePopupEntry>& pool) {
    for (auto& e : pool) {
        if (!e.active) return &e;
    }
    DamagePopupEntry* oldest = &pool[0];
    for (auto& e : pool) {
        if (e.timer > oldest->timer) oldest = &e;
    }
    return oldest;
}