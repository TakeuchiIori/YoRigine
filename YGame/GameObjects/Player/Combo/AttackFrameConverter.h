#pragma once

#include <vector>
#include "ComboTypes.h"
#include <Debugger/DopeSheet/DopeTrack.h>

// ============================================================
// アタックフレームコンバーター
// AttackDataが持つフレーム設定と、DopeSheet用のトラックデータを双方向に変換する
//
// [用途]
//   ドープシート表示時 : BuildTracks で AttackData の値をUI用のトラックデータに変換する
//   ドープシート編集時 : ApplyTracks でUIの編集結果を AttackData へ書き戻す
// ============================================================
class AttackFrameConverter
{
public:
	// ============================================================
	// AttackData から DopeTrack リストの生成
	// 攻撃データ選択時やリロード時に呼び出し、各区間を視覚的に表現するトラックを作る
	// ============================================================
	static std::vector<DopeSheet::DopeTrack> BuildTracks(const AttackData& attack)
	{
		std::vector<DopeSheet::DopeTrack> tracks;

		// ------------------------------------------------------------
		// 攻撃判定発生区間 (Hitbox)
		// ------------------------------------------------------------
		tracks.push_back(MakeBarTrack(
			DopeSheet::TrackType::AttackHitbox,
			attack.hitStart, attack.hitEnd));

		// ------------------------------------------------------------
		// 攻撃後の硬直区間 (Recovery)
		// ArmorFrame用のトラックを流用し、ラベルを "Recovery" に差し替える
		// ------------------------------------------------------------
		tracks.push_back(MakeBarTrack(
			DopeSheet::TrackType::ArmorFrame,
			attack.recoveryStart, attack.recoveryEnd,
			"Recovery"));

		// ------------------------------------------------------------
		// キャンセル受付区間 (Cancel Window)
		// ------------------------------------------------------------
		tracks.push_back(MakeBarTrack(
			DopeSheet::TrackType::CancelWindow,
			attack.cancelStart, attack.totalFrames));

		// ------------------------------------------------------------
		// コンボ入力受付区間 (Combo Window)
		// ------------------------------------------------------------
		tracks.push_back(MakeBarTrack(
			DopeSheet::TrackType::ComboWindow,
			attack.comboWindowStart, attack.comboWindowEnd));

		// ------------------------------------------------------------
		// 無敵区間 (Invincible)
		// ------------------------------------------------------------
		tracks.push_back(MakeBarTrack(
			DopeSheet::TrackType::InvincibleFrame,
			attack.invincibleStart, attack.invincibleEnd));

		return tracks;
	}

	// ============================================================
	// DopeTrack リストから AttackData への反映
	// エディタ上での変更をゲームデータとして保存するために使用する
	// ============================================================
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

			case DopeSheet::TrackType::ArmorFrame: // Recoveryとして使用
				ReadBarTrack(track, attack.recoveryStart, attack.recoveryEnd);
				break;

			case DopeSheet::TrackType::CancelWindow:
				// キャンセル開始フレームのみ保存、終了はアニメーションの長さに依存
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
	// ============================================================
	// ヘルパー：バー形状（区間）のトラックを1つ生成
	// ============================================================
	static DopeSheet::DopeTrack MakeBarTrack(
		DopeSheet::TrackType type,
		int startFrame,
		int endFrame,
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

	// ============================================================
	// ヘルパー：ポイント形状（イベント）のキーを1つ生成
	// ============================================================
	static DopeSheet::DopeKey MakeEventKey(int frame, const std::string& tag)
	{
		DopeSheet::DopeKey key;
		key.frame = frame;
		key.tag = tag;
		key.shape = DopeSheet::KeyShape::Diamond;
		return key;
	}

	// ============================================================
	// ヘルパー：トラック情報から開始・終了フレームを読み取る
	// ============================================================
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