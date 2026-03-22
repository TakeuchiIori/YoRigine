#pragma once

#include <vector>
#include "ComboTypes.h"
#include <Debugger/DopeSheet/DopeTrack.h>

//=============================================================================
// AttackFrameConverter
// AttackData（フレームフィールド）と vector<DopeTrack> を双方向に変換する
//
// 【使い方】
//   攻撃を選択したとき   : tracks = BuildTracks(attack)
//   ドープシート編集後   : ApplyTracks(tracks, attack) → attack.SyncFramesToSeconds()
//=============================================================================
class AttackFrameConverter
{
public:
    //=========================================================================
    // AttackData → vector<DopeTrack>
    // 攻撃を選択したとき・リロード時に呼ぶ
    //=========================================================================
    static std::vector<DopeSheet::DopeTrack> BuildTracks(const AttackData& attack)
    {
        std::vector<DopeSheet::DopeTrack> tracks;

        // Attack Hitbox（攻撃判定の区間）
        tracks.push_back(MakeBarTrack(
            DopeSheet::TrackType::AttackHitbox,
            attack.hitStart, attack.hitEnd));

        // Recovery（硬直の区間）
        // ArmorFrame を Recovery トラックとして流用しラベルを上書き
        tracks.push_back(MakeBarTrack(
            DopeSheet::TrackType::ArmorFrame,
            attack.recoveryStart, attack.recoveryEnd,
            "Recovery"));

        // Cancel Window（キャンセル受付の区間）
        tracks.push_back(MakeBarTrack(
            DopeSheet::TrackType::CancelWindow,
            attack.cancelStart, attack.totalFrames));

        // Combo Window（コンボ入力受付の区間）
        tracks.push_back(MakeBarTrack(
            DopeSheet::TrackType::ComboWindow,
            attack.comboWindowStart, attack.comboWindowEnd));

        // Invincible（無敵の区間）
        tracks.push_back(MakeBarTrack(
            DopeSheet::TrackType::InvincibleFrame,
            attack.invincibleStart, attack.invincibleEnd));

        //// Effect（エフェクト発生タイミング・ポイントキー）
        //{
        //    auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::Effect);
        //    for (const auto& ev : attack.effects)
        //        track.AddKey(MakeEventKey(ev.frame, ev.tag));
        //    tracks.push_back(std::move(track));
        //}

        //// Sound（SE 再生タイミング・ポイントキー）
        //{
        //    auto track = DopeSheet::DopeTrack::Make(DopeSheet::TrackType::Sound);
        //    for (const auto& ev : attack.sounds)
        //        track.AddKey(MakeEventKey(ev.frame, ev.tag));
        //    tracks.push_back(std::move(track));
        //}

        return tracks;
    }

    //=========================================================================
    // vector<DopeTrack> → AttackData
    // ドープシートの編集結果を書き戻すときに呼ぶ
    // 呼び出し後に attack.SyncFramesToSeconds() を呼ぶこと
    //=========================================================================
    static void ApplyTracks(
        const std::vector<DopeSheet::DopeTrack>& tracks,
        AttackData& attack)
    {
        for (const auto& track : tracks)
        {
            switch (track.type)
            {
            case DopeSheet::TrackType::AttackHitbox:
                ReadBarTrack(track, attack.hitStart, attack.hitEnd);
                break;

                // ArmorFrame = Recovery トラック
            case DopeSheet::TrackType::ArmorFrame:
                ReadBarTrack(track, attack.recoveryStart, attack.recoveryEnd);
                break;

            case DopeSheet::TrackType::CancelWindow:
                // キャンセル受付は開始フレームのみ保持
                attack.cancelStart = track.keys.empty()
                    ? attack.totalFrames
                    : track.keys.front().frame;
                break;

            case DopeSheet::TrackType::ComboWindow:
                ReadBarTrack(track, attack.comboWindowStart, attack.comboWindowEnd);
                break;

            case DopeSheet::TrackType::InvincibleFrame:
                ReadBarTrack(track, attack.invincibleStart, attack.invincibleEnd);
                break;

            case DopeSheet::TrackType::Effect:
                attack.effects.clear();
                for (const auto& key : track.keys)
                    attack.effects.push_back({ key.frame, key.tag });
                break;

            case DopeSheet::TrackType::Sound:
                attack.sounds.clear();
                for (const auto& key : track.keys)
                    attack.sounds.push_back({ key.frame, key.tag });
                break;

            default:
                break;
            }
        }
    }

private:
    // 区間バートラックを1つ生成する
    static DopeSheet::DopeTrack MakeBarTrack(
        DopeSheet::TrackType   type,
        int                    startFrame,
        int                    endFrame,
        const std::string& labelOverride = "")
    {
        auto track = DopeSheet::DopeTrack::Make(type, labelOverride);

        if (endFrame > startFrame)
        {
            DopeSheet::DopeKey key;
            key.frame = startFrame;
            key.duration = endFrame - startFrame;
            key.shape = DopeSheet::KeyShape::Bar;
            track.AddKey(key);
        }
        return track;
    }

    // ポイントイベントキーを1つ生成する
    static DopeSheet::DopeKey MakeEventKey(int frame, const std::string& tag)
    {
        DopeSheet::DopeKey key;
        key.frame = frame;
        key.tag = tag;
        key.shape = DopeSheet::KeyShape::Diamond;
        return key;
    }

    // 区間バートラックの先頭キーから start/end を読み取る
    static void ReadBarTrack(
        const DopeSheet::DopeTrack& track,
        int& outStart,
        int& outEnd)
    {
        if (!track.keys.empty())
        {
            outStart = track.keys.front().frame;
            outEnd = track.keys.front().EndFrame();
        }
        else
        {
            outStart = 0;
            outEnd = 0;
        }
    }
};