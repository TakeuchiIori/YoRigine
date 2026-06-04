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
		// 先行入力受付区間 (Input Buffer)
		// inputBufferStart 〜 comboWindowEnd を「受付帯」として可視化する。
		// この帯内で押された入力が次の攻撃にチェインする唯一の経路。
		// ------------------------------------------------------------
		tracks.push_back(MakeBarTrack(
			DopeSheet::TrackType::CancelWindow,
			attack.inputBufferStart, attack.comboWindowEnd,
			"Input Buffer"));

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

			case DopeSheet::TrackType::CancelWindow:
				// 先行入力受付帯：開始 → inputBufferStart、終了 → comboWindowEnd
				ReadBarTrack(track, attack.inputBufferStart, attack.comboWindowEnd);
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