#pragma once

#include <vector>
#include "AttackFrameData.h"
#include <Debugger/DopeSheet/DopeTrack.h>

//=============================================================================
// AttackFrameConverter
// AttackFrameData（フレーム単位データ）と
// vector<DopeTrack>（DopeSheetEditor用）を双方向に変換する
//=============================================================================
class AttackFrameConverter
{
public:
    //=========================================================================
    // AttackFrameData → vector<DopeTrack>
    // 攻撃を選択したとき・リロード時に呼ぶ
    //=========================================================================
    static std::vector<DopeSheet::DopeTrack> BuildTracks(const AttackFrameData& data)
    {
        std::vector<DopeSheet::DopeTrack> tracks;

        // -----------------------------------------------------------------
        // 1. Attack Hitbox トラック（区間バー）
        // -----------------------------------------------------------------
        {
            auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::AttackHitbox);
            if (data.hitEnd > data.hitStart)
            {
                DopeSheet::DopeKey key;
                key.frame = data.hitStart;
                key.duration = data.hitEnd - data.hitStart;
                key.shape = DopeSheet::KeyShape::Bar;
                track.AddKey(key);
            }
            tracks.push_back(std::move(track));
        }

        // -----------------------------------------------------------------
        // 2. Cancel Window トラック（区間バー）
        // -----------------------------------------------------------------
        {
            auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::CancelWindow);
            if (data.cancelStart < data.totalFrames)
            {
                DopeSheet::DopeKey key;
                key.frame = data.cancelStart;
                key.duration = data.totalFrames - data.cancelStart;
                key.shape = DopeSheet::KeyShape::Bar;
                track.AddKey(key);
            }
            tracks.push_back(std::move(track));
        }

        // -----------------------------------------------------------------
        // 3. Combo Window トラック（区間バー）
        // -----------------------------------------------------------------
        {
            auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::ComboWindow);
            if (data.comboWindowEnd > data.comboWindowStart)
            {
                DopeSheet::DopeKey key;
                key.frame = data.comboWindowStart;
                key.duration = data.comboWindowEnd - data.comboWindowStart;
                key.shape = DopeSheet::KeyShape::Bar;
                track.AddKey(key);
            }
            tracks.push_back(std::move(track));
        }

        // -----------------------------------------------------------------
        // 4. Invincible Frame トラック（区間バー）
        // -----------------------------------------------------------------
        {
            auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::InvincibleFrame);
            if (data.invincibleEnd > data.invincibleStart)
            {
                DopeSheet::DopeKey key;
                key.frame = data.invincibleStart;
                key.duration = data.invincibleEnd - data.invincibleStart;
                key.shape = DopeSheet::KeyShape::Bar;
                track.AddKey(key);
            }
            tracks.push_back(std::move(track));
        }

        // -----------------------------------------------------------------
        // 5. Effect トラック（ポイントキー）
        // -----------------------------------------------------------------
        {
            auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::Effect);
            for (const auto& ev : data.effects)
            {
                DopeSheet::DopeKey key;
                key.frame = ev.frame;
                key.tag = ev.tag;
                key.shape = DopeSheet::KeyShape::Diamond;
                track.AddKey(key);
            }
            tracks.push_back(std::move(track));
        }

        // -----------------------------------------------------------------
        // 6. Sound トラック（ポイントキー）
        // -----------------------------------------------------------------
        {
            auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::Sound);
            for (const auto& ev : data.sounds)
            {
                DopeSheet::DopeKey key;
                key.frame = ev.frame;
                key.tag = ev.tag;
                key.shape = DopeSheet::KeyShape::Diamond;
                track.AddKey(key);
            }
            tracks.push_back(std::move(track));
        }

        return tracks;
    }

    //=========================================================================
    // vector<DopeTrack> → AttackFrameData
    // DopeSheetEditor の編集結果を書き戻すときに呼ぶ
    //=========================================================================
    static void ApplyTracks(
        const std::vector<DopeSheet::DopeTrack>& tracks,
        AttackFrameData& data)
    {
        for (const auto& track : tracks)
        {
            switch (track.type)
            {
                // -----------------------------------------------------------------
                // Attack Hitbox：先頭のバーキーだけ読む
                // -----------------------------------------------------------------
            case DopeSheet::TrackType::AttackHitbox:
            {
                if (!track.keys.empty())
                {
                    const auto& key = track.keys.front();
                    data.hitStart = key.frame;
                    data.hitEnd = key.EndFrame();
                }
                else
                {
                    data.hitStart = 0;
                    data.hitEnd = 0;
                }
                break;
            }

            // -----------------------------------------------------------------
            // Cancel Window：先頭キーの開始フレームだけ読む
            // -----------------------------------------------------------------
            case DopeSheet::TrackType::CancelWindow:
            {
                if (!track.keys.empty())
                    data.cancelStart = track.keys.front().frame;
                else
                    data.cancelStart = data.totalFrames;
                break;
            }

            // -----------------------------------------------------------------
            // Combo Window：先頭のバーキーだけ読む
            // -----------------------------------------------------------------
            case DopeSheet::TrackType::ComboWindow:
            {
                if (!track.keys.empty())
                {
                    const auto& key = track.keys.front();
                    data.comboWindowStart = key.frame;
                    data.comboWindowEnd = key.EndFrame();
                }
                else
                {
                    data.comboWindowStart = 0;
                    data.comboWindowEnd = 0;
                }
                break;
            }

            // -----------------------------------------------------------------
            // Invincible Frame：先頭のバーキーだけ読む
            // -----------------------------------------------------------------
            case DopeSheet::TrackType::InvincibleFrame:
            {
                if (!track.keys.empty())
                {
                    const auto& key = track.keys.front();
                    data.invincibleStart = key.frame;
                    data.invincibleEnd = key.EndFrame();
                }
                else
                {
                    data.invincibleStart = 0;
                    data.invincibleEnd = 0;
                }
                break;
            }

            // -----------------------------------------------------------------
            // Effect：全キーをイベントリストに書き戻す
            // -----------------------------------------------------------------
            case DopeSheet::TrackType::Effect:
            {
                data.effects.clear();
                for (const auto& key : track.keys)
                {
                    AttackFrameData::FrameEvent ev;
                    ev.frame = key.frame;
                    ev.tag = key.tag;
                    data.effects.push_back(ev);
                }
                break;
            }

            // -----------------------------------------------------------------
            // Sound：全キーをイベントリストに書き戻す
            // -----------------------------------------------------------------
            case DopeSheet::TrackType::Sound:
            {
                data.sounds.clear();
                for (const auto& key : track.keys)
                {
                    AttackFrameData::FrameEvent ev;
                    ev.frame = key.frame;
                    ev.tag = key.tag;
                    data.sounds.push_back(ev);
                }
                break;
            }

            default:
                break;
            }
        }
    }

    //=========================================================================
    // AttackFrameData → AttackData への秒単位書き戻し
    // fps を使ってフレーム→秒に変換する
    //=========================================================================
    static void SyncToAttackData(const AttackFrameData& frame, AttackData& attack)
    {
        if (frame.fps <= 0) return;

        const float invFps = 1.0f / static_cast<float>(frame.fps);

        // 攻撃判定の終了フレームを duration に反映
        attack.duration = static_cast<float>(frame.hitEnd) * invFps;

        // キャンセル開始フレームを continueWindow に反映
        // （totalFrames - cancelStart = キャンセル可能な残り時間）
        attack.continueWindow = static_cast<float>(frame.totalFrames - frame.cancelStart) * invFps;
    }
};