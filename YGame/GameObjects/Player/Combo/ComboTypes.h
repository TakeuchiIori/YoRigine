#pragma once

#include <string>
#include <vector>
#include <map>
#include "Vector3.h"
#include "Vector2.h"

// Engine
#include "Loaders/Json/StructSerializer.h"

//=============================================================================
// コンボ状態
//=============================================================================
enum class ComboState {
    Idle,           // 待機状態
    Attacking,      // 攻撃中
    CanContinue,    // 次の攻撃可能
    Recovery,       // 硬直中
    Finished        // コンボ終了
};

//=============================================================================
// 攻撃タイプ
//=============================================================================
enum class AttackType {
    A_Arte,         // A（軽攻撃）- 素早い基本攻撃、CC消費少
    B_Arte,         // B（重攻撃）- 威力の高い特殊攻撃、CC消費中
    Arcane_Arte     // 奥義（必殺技）- 最高威力の技、CC消費大
};

//=============================================================================
// AttackData
// 1つの攻撃に関するすべてのデータを持つ構造体
//
// 【設計方針】
//   - ゲームロジックが参照する「秒単位」フィールドと
//     エディタが編集する「フレーム単位」フィールドを1つにまとめる
//   - フレーム単位フィールドを編集したら SyncFramesToSeconds() を呼ぶことで
//     ゲーム側の秒フィールドに自動反映される
//=============================================================================
struct AttackData {

    //-------------------------------------------------------------------------
    // 識別・基本情報
    //-------------------------------------------------------------------------
    std::string name;                       // 攻撃名
    std::string animationName;              // 使用するアニメーション名
    AttackType  type = AttackType::A_Arte;
    std::string cameraEffect = "None";      // カメラ演出名

    //-------------------------------------------------------------------------
    // タイムライン設定（エディタ用・フレーム単位）
    // ドープシートで直接編集する値
    //-------------------------------------------------------------------------
    int totalFrames = 60;       // この攻撃の全体フレーム数
    int fps = 60;               // フレームレート（秒換算に使用）

    int hitStart = 0;           // 攻撃判定  開始フレーム
    int hitEnd = 10;            // 攻撃判定  終了フレーム
    int recoveryStart = 0;      // 硬直      開始フレーム
    int recoveryEnd = 0;        // 硬直      終了フレーム
    int cancelStart = 0;        // キャンセル受付 開始フレーム
    int comboWindowStart = 0;   // コンボ入力受付 開始フレーム
    int comboWindowEnd = 0;     // コンボ入力受付 終了フレーム
    int invincibleStart = 0;    // 無敵      開始フレーム
    int invincibleEnd = 0;      // 無敵      終了フレーム

    // フレームイベント（エフェクト・SE の発生タイミング）
    struct FrameEvent {
        int         frame = 0;
        std::string tag;                // エフェクト名・SE名など
    };
    std::vector<FrameEvent> effects;    // エフェクト発生タイミング一覧
    std::vector<FrameEvent> sounds;     // SE 再生タイミング一覧

    //-------------------------------------------------------------------------
    // タイミング設定（ゲームロジック用・秒単位）
    // SyncFramesToSeconds() で自動計算される。直接編集も可能。
    //-------------------------------------------------------------------------
    float duration = 0.0f;          // 攻撃判定の持続時間（秒）
    float recovery = 0.0f;          // 硬直時間（秒）
    float continueWindow = 0.0f;    // コンボ入力受付時間（秒）

    //-------------------------------------------------------------------------
    // ダメージ・物理効果
    //-------------------------------------------------------------------------
    float   baseDamage = 0.0f;              // 基本ダメージ
    float   knockback = 0.0f;               // ノックバック力
    float   knockbackDuration = 0.0f;       // ノックバック持続時間（秒）
    float   stepDistance = 0.0f;            // 攻撃時の踏み込み距離

    //-------------------------------------------------------------------------
    // CC（コンバットコスト）システム
    //-------------------------------------------------------------------------
    int ccCost = 0;    // CC 消費量
    int ccOnHit = 0;   // ヒット時 CC 回復量

    //-------------------------------------------------------------------------
    // コンボ特性
    //-------------------------------------------------------------------------
    bool canCancel = true;                  // 他の攻撃でキャンセル可能か
    bool canChainToAny = true;              // 任意の攻撃に繋げられるか
    std::vector<AttackType> preferredNext;  // 推奨次攻撃（ボーナス有）

    //-------------------------------------------------------------------------
    // 特殊効果フラグ
    //-------------------------------------------------------------------------
    bool        launches = false;           // 敵を浮かす
    bool        wallBounce = false;         // 壁バウンド誘発
    bool        groundBounce = false;       // 地面バウンド誘発
    std::string effect;                     // 特殊エフェクト名
    float       motionSpeed = 1.0f;         // アニメーション再生速度

    //-------------------------------------------------------------------------
    // フレーム → 秒 の自動計算
    // ドープシートで編集した後に呼ぶことで、ゲーム側の秒フィールドに反映される
    //-------------------------------------------------------------------------
    void SyncFramesToSeconds()
    {
        if (fps <= 0) return;
        const float invFps = 1.0f / static_cast<float>(fps);
        //recovery = static_cast<float>(recoveryEnd - recoveryStart) * invFps;
        //continueWindow = static_cast<float>(comboWindowEnd - comboWindowStart) * invFps;
    }

    // デフォルトコンストラクタ
    AttackData() = default;
};

//=============================================================================
// CC 設定
//=============================================================================
struct CCConfig {
    int   maxCC = 5;                    // 最大 CC 値
    float regenRate = 1.0f;             // CC 回復速度（毎秒）
    float regenDelay = 1.5f;            // 攻撃後の CC 回復開始遅延（秒）
    int   dodgeRecovery = 2;            // 回避成功時の CC 回復量
    int   counterRecovery = 1;          // カウンター成功時の CC 回復量

    CCConfig() = default;
    CCConfig(int max, float rate, float delay, int dodge, int counter)
        : maxCC(max), regenRate(rate), regenDelay(delay),
        dodgeRecovery(dodge), counterRecovery(counter) {
    }
};

//=============================================================================
// コンボ設定
//=============================================================================
struct ComboConfig {
    int   maxLength = 20;               // 最大コンボ長
    float damageDecay = 0.95f;          // コンボ減衰率（3ヒット目以降）
    float chainBonus = 1.15f;           // 推奨チェーンボーナス倍率
    bool  enableFreeChain = true;       // 自由チェーン有効
    float comboResetTime = 1.0f;        // コンボリセット時間（秒）

    ComboConfig() = default;
    ComboConfig(int length, float decay, float bonus, bool freeChain, float resetTime)
        : maxLength(length), damageDecay(decay), chainBonus(bonus),
        enableFreeChain(freeChain), comboResetTime(resetTime) {
    }
};

//=============================================================================
// JSON シリアライザー登録
//=============================================================================

BEGIN_STRUCT_SERIALIZER(AttackData)
// 識別・基本
SERIALIZE_FIELD(AttackData, name)
SERIALIZE_FIELD(AttackData, animationName)
SERIALIZE_ENUM_FIELD(AttackData, type)
SERIALIZE_FIELD(AttackData, cameraEffect)
// タイムライン（フレーム単位）
SERIALIZE_FIELD(AttackData, totalFrames)
SERIALIZE_FIELD(AttackData, fps)
SERIALIZE_FIELD(AttackData, hitStart)
SERIALIZE_FIELD(AttackData, hitEnd)
SERIALIZE_FIELD(AttackData, recoveryStart)
SERIALIZE_FIELD(AttackData, recoveryEnd)
SERIALIZE_FIELD(AttackData, cancelStart)
SERIALIZE_FIELD(AttackData, comboWindowStart)
SERIALIZE_FIELD(AttackData, comboWindowEnd)
SERIALIZE_FIELD(AttackData, invincibleStart)
SERIALIZE_FIELD(AttackData, invincibleEnd)
// タイミング（秒単位）
SERIALIZE_FIELD(AttackData, duration)
SERIALIZE_FIELD(AttackData, recovery)
SERIALIZE_FIELD(AttackData, continueWindow)
// ダメージ・物理
SERIALIZE_FIELD(AttackData, baseDamage)
SERIALIZE_FIELD(AttackData, knockback)
SERIALIZE_FIELD(AttackData, knockbackDuration)
SERIALIZE_FIELD(AttackData, stepDistance)
// CC
SERIALIZE_FIELD(AttackData, ccCost)
SERIALIZE_FIELD(AttackData, ccOnHit)
// コンボ特性
SERIALIZE_FIELD(AttackData, canCancel)
SERIALIZE_FIELD(AttackData, canChainToAny)
SERIALIZE_FIELD(AttackData, preferredNext)
// 特殊効果
SERIALIZE_FIELD(AttackData, launches)
SERIALIZE_FIELD(AttackData, wallBounce)
SERIALIZE_FIELD(AttackData, groundBounce)
SERIALIZE_FIELD(AttackData, effect)
SERIALIZE_FIELD(AttackData, motionSpeed)
END_STRUCT_SERIALIZER(AttackData)

BEGIN_STRUCT_SERIALIZER(CCConfig)
SERIALIZE_FIELD(CCConfig, maxCC)
SERIALIZE_FIELD(CCConfig, regenRate)
SERIALIZE_FIELD(CCConfig, regenDelay)
SERIALIZE_FIELD(CCConfig, dodgeRecovery)
SERIALIZE_FIELD(CCConfig, counterRecovery)
END_STRUCT_SERIALIZER(CCConfig)

BEGIN_STRUCT_SERIALIZER(ComboConfig)
SERIALIZE_FIELD(ComboConfig, maxLength)
SERIALIZE_FIELD(ComboConfig, damageDecay)
SERIALIZE_FIELD(ComboConfig, chainBonus)
SERIALIZE_FIELD(ComboConfig, enableFreeChain)
SERIALIZE_FIELD(ComboConfig, comboResetTime)
END_STRUCT_SERIALIZER(ComboConfig)