#pragma once

#include <string>
#include <vector>
#include <json.hpp>

//=============================================================================
// AttackFrameData
// AttackData（秒単位）に対応するフレーム単位の演出・判定データ
// attackName をキーとして AttackData と 1対1 でリンクする
//=============================================================================
struct AttackFrameData
{
    // AttackData::name と対応するキー
    std::string attackName;

    // タイムライン全体の長さ
    int totalFrames = 60;
    int fps         = 60;

    //-------------------------------------------------------------------------
    // フレーム区間
    //-------------------------------------------------------------------------
    int hitStart          = 0;   // 攻撃判定 開始
    int hitEnd            = 0;   // 攻撃判定 終了
    int cancelStart       = 0;   // キャンセル受付 開始
    int comboWindowStart  = 0;   // コンボ入力受付 開始
    int comboWindowEnd    = 0;   // コンボ入力受付 終了
    int invincibleStart   = 0;   // 無敵 開始
    int invincibleEnd     = 0;   // 無敵 終了

    //-------------------------------------------------------------------------
    // ポイントイベント
    //-------------------------------------------------------------------------
    struct FrameEvent
    {
        int         frame = 0;
        std::string tag;          // エフェクト名・SE名など

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(FrameEvent, frame, tag)
    };

    std::vector<FrameEvent> effects;   // エフェクト発生タイミング
    std::vector<FrameEvent> sounds;    // SE再生タイミング

    //-------------------------------------------------------------------------
    // JSON シリアライズ
    //-------------------------------------------------------------------------
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(
        AttackFrameData,
        attackName,
        totalFrames,
        fps,
        hitStart, hitEnd,
        cancelStart,
        comboWindowStart, comboWindowEnd,
        invincibleStart,  invincibleEnd,
        effects,
        sounds
    )
};
