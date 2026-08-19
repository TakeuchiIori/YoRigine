#pragma once

#ifdef USE_IMGUI

#include "../Data/EnemyAttack.h"
#include "Debugger/CurveEditor/CurveDelegate.h"

#include <memory>

// ============================================================
// カーブ編集パネル
//
// 位置・回転・スケールの9チャンネルを ImCurveEdit で編集する。
// 全部同時に出すと線が重なって読めないので、グループ単位で
// 表示を切り替える（位置だけ / 回転だけ / スケールだけ）。
//
// 横軸は 0〜1 の進行度。実時間は攻撃側の duration が決める。
// ============================================================
class AttackCurvePanel {
public:
	// 表示するグループ
	enum class Group {
		Position,
		Rotation,
		Scale,
	};

	void Draw(EnemyAttack &attack);

	// 再生ヘッドの位置（0〜1）。負で非表示。
	void SetPlayhead(float t) { playhead_ = t; }

private:
	// 選択中グループのチャンネルを Delegate へ登録し直す
	void RebuildDelegate(EnemyAttack &attack);

	// グループに属する3チャンネルを返す
	void GetGroupChannels(AttackChannel outChannels[3]) const;

	// 縦軸の表示範囲を決める（回転はラジアンなので広く取る）
	void UpdateViewRange(const EnemyAttack &attack);

	// キーの追加・削除など、カーブ以外の操作
	void DrawChannelControls(EnemyAttack &attack);

private:
	std::unique_ptr<CurveDelegate> delegate_;
	Group group_ = Group::Position;

	// 直前に構築した状態。変わったら組み直す。
	Group builtGroup_ = Group::Position;
	const EnemyAttack *builtAttack_ = nullptr;
	bool built_ = false;

	float playhead_ = -1.0f;

	// 縦軸を手動で決めるか（自動だと編集中に伸び縮みして扱いにくい）
	bool autoRange_ = true;
	float manualMin_ = -5.0f;
	float manualMax_ = 10.0f;
};

#endif // USE_IMGUI
