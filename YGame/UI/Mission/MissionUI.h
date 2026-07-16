#pragma once
#include "Vector2.h"
#include "Vector4.h"
#include <string>
#include <cstdint>

class UIBase;

// ============================================================
// ミッションUI
//   ウェイポイント進行（WaypointManager）を毎フレーム参照（ポーリング）し、
//   フィールドシーンでのみ現在の目標文と撃破進捗を表示する。
//   目標クリア時は Check をポップ表示する。
//
//   背景: MissionWindow / 枠: frame_square / 目標文: ipaexg でベイク / Check。
//   位置・サイズは UIConfig（UIEditor）で調整可能。UIManager に登録する。
// ============================================================
class MissionUI {
public:
	void Initialize();
	void Update();
	void Draw();

	// フィールドシーンのときだけ表示する（バトル中は隠す）。GameUI から毎フレーム設定。
	void SetFieldActive(bool active) { fieldActive_ = active; }

	// 目標クリアの瞬間だけ Check をポップさせるイベント（WaypointManager から中継）。
	void OnMissionCleared();

private:
	// 指定IDのUIを取得。無ければ layer に生成して configPath を持たせる（UIEditor 調整用）。
	UIBase* GetOrCreate(const std::string& id, const std::string& texturePath,
		const Vector2& pos, const Vector2& size, const Vector2& anchor, int layer);

	// 現在の目標文＋「現在/必要」を焼き直して text_ に反映
	void RebakeText();
	std::string BakeMissionText(const std::string& title, int cur, int req);

	void HideWindow();  // 全部隠す

private:
	UIBase* window_ = nullptr;   // 背景ウィンドウ（MissionWindow）
	UIBase* frame_  = nullptr;   // 枠（frame_square）
	UIBase* text_   = nullptr;   // 目標文＋撃破数（ベイク）
	UIBase* check_  = nullptr;   // クリア時にポップ表示する Check

	bool fieldActive_ = false;   // フィールドシーンか（バトル中は false で隠す）
	bool windowShown_ = false;   // ウィンドウ表示中か（多重フェード/多重hide防止）

	// 表示時に焼き直す基準カラー（＝UIEditorで設定した config の色）。
	// 実行時にアルファがゴミ値に化けても、表示のたびにこの色で上書きして復元する。
	// アルファ成分がフェードの到達値（＝エディタのアルファ設定の反映）にもなる。
	Vector4 windowColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 frameColor_  = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 textColor_   = { 1.0f, 1.0f, 1.0f, 1.0f };

	uint32_t shownSerial_ = 0;   // 表示中の目標の通し番号（WaypointManager と比較して新目標を検知）
	std::string currentTitle_;
	int  curCount_ = 0;
	int  reqCount_ = 1;

	bool  checkVisible_ = false; // Check 表示中か
	float checkTimer_   = 0.0f;  // Check の表示経過（一定時間で自動的に隠す）
	bool  completionPending_ = false; // チェック演出後の次ミッション表示待ち

	int bakeSeq_ = 0;            // ベイクPNGのファイル名連番（テクスチャキャッシュ回避）
};
