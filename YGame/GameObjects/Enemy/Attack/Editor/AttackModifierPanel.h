#pragma once

#ifdef USE_IMGUI

#include "../Data/EnemyAttack.h"
#include "Debugger/DopeSheet/DopeSheetEditor.h"

#include <vector>

// ============================================================
// モディファイア編集パネル
//
// 攻撃判定・無敵・向き追従・追尾・弾を時間軸の区間として並べる。
// 「いつ攻撃判定が出るか」「無敵はどこまでか」は数値の羅列では
// 判断できないので、ドープシートで重なりが見える形にする。
//
// 種別ごとに1トラックを割り当てるので、同じ種別が複数あっても
// 同じ行に並んで前後関係が分かる。
// ============================================================
class AttackModifierPanel {
public:
	void Draw(EnemyAttack &attack);

	// 再生ヘッドの位置（秒）。負で非表示。
	void SetPlayheadTime(float seconds) { playheadTime_ = seconds; }

private:
	// モディファイアからトラックを作る
	void BuildTracks(const EnemyAttack &attack);

	// ドープシートで動かした区間をデータへ書き戻す
	void ApplyTracks(EnemyAttack &attack);

	// 選択中のモディファイアの詳細編集
	void DrawInspector(EnemyAttack &attack);

	// 追加ボタン群
	void DrawAddButtons(EnemyAttack &attack);

	// 秒 ↔ フレーム（ドープシートはフレーム単位で動く）
	int ToFrames(float seconds) const;
	float ToSeconds(int frames) const;

private:
	DopeSheet::DopeSheetEditor dope_;
	std::vector<DopeSheet::DopeTrack> tracks_;

	// トラック上のキーが、どのモディファイア（添字）に対応するか。
	// トラック番号とキー番号から引く。
	std::vector<std::vector<int>> keyToModifier_;

	int selectedModifier_ = -1;

	// 表示解像度。データは秒なのでここは見た目の細かさだけを決める。
	int editorFps_ = 60;

	bool dirty_ = true;
	const EnemyAttack *builtAttack_ = nullptr;
	float builtDuration_ = 0.0f;
	size_t builtCount_ = 0;

	float playheadTime_ = -1.0f;
};

#endif // USE_IMGUI
