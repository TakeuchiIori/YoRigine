#pragma once
#include "Vector3.h"
#include "Vector4.h"
#include "Vector2.h"

/// <summary>
/// パーティクル 1 粒のランタイムデータ
///
/// Spawn モジュールが OnSpawn() で初期値を書き込み、
/// Update モジュールが OnUpdate() で毎フレーム更新する。
/// YParticleSystem::Emit() 内で全フィールドがゼロリセットされてから
/// Spawn モジュールが適用される。
/// </summary>
struct ParticleAttribute {
    // ── コアフィールド ─────────────────────────────────────────────────
    Vector3 position{};
    Vector3 velocity{};
    Vector3 scale{ 1.0f, 1.0f, 1.0f };
    Vector3 rotation{};
    Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
    float   lifeTime    = 1.0f;   // 寿命（秒）
    float   currentTime = 0.0f;   // 経過時間（秒）
    bool    isActive    = false;
    bool    immortal    = false;  // true なら寿命で死なず、更新され続ける（Persistent 用）

    // ── UV アニメーション ──────────────────────────────────────────────
    Vector2 uvOffset{};
    Vector2 uvScale{ 1.0f, 1.0f };

    // ── 拡張フィールド（新規モジュール対応） ────────────────────────────

    /// 生成位置 — UpdateBounce / UpdateLimitByDistance / UpdateOrbital で参照
    Vector3 origin{};

    /// 生成直後のスケール — UpdateSizeOverLifetime / UpdateScaleFromInitial で補間起点として使用
    Vector3 initialScale{ 1.0f, 1.0f, 1.0f };

    /// 生成直後のカラー — UpdateColorOverLifetime / UpdateColorFromInitial で補間起点として使用
    Vector4 initialColor{ 1.0f, 1.0f, 1.0f, 1.0f };

    /// 角速度 (度/秒 × 軸) — SpawnAngularVelocity で設定、UpdateAngularVelocity で適用
    Vector3 angularVelocity{};

    /// 位相オフセット [0, 2π] — SpawnPhase で設定、UpdateSinMovement などで利用
    float phase = 0.0f;

    /// フリップブック 現在フレームインデックス — UpdateFlipbook で加算
    float flipbookFrame = 0.0f;

    // ── ユーティリティ ─────────────────────────────────────────────────

    /// 正規化寿命 [0, 1] — lifeTime == 0 のとき 1 を返す（ゼロ除算防止）
    float GetNormalizedAge() const {
        return (lifeTime > 0.0f) ? (currentTime / lifeTime) : 1.0f;
    }

    /// 速さ（スカラー）
    float GetSpeed() const {
        return std::sqrt(velocity.x * velocity.x +
                         velocity.y * velocity.y +
                         velocity.z * velocity.z);
    }
};
