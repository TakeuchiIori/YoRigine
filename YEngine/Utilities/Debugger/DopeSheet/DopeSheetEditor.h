#pragma once

//=============================================================================
// DopeSheetEditor
// 汎用タイムライン（ドープシート）UIコンポーネント
//
// 使い方：
//   1. std::vector<DopeTrack> を用意してキーフレームを登録する
//   2. Draw() を呼ぶ
//   3. 戻り値が true なら tracks が変更されたのでデータに書き戻す
//=============================================================================

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <string>
#include <vector>
#include <functional>

//-----------------------------------------------------------------------------
// キーフレーム 1つ
//-----------------------------------------------------------------------------
struct DopeKey
{
	int   frame = 0;			// フレーム番号（0〜totalFrames）
	float value = 0.0f;			// 汎用値（踏み込み距離・SE番号など）
	int   subType = 0;			// サブ種別（色分け用。モーションなら T=0/R=1/S=2）
	bool  selected = false;

	DopeKey() = default;
	DopeKey(int f, float v = 0.0f, int sub = 0)
		: frame(f), value(v), subType(sub) {
	}
};

//-----------------------------------------------------------------------------
// トラック 1行
//-----------------------------------------------------------------------------
struct DopeTrack
{
	std::string label;						// 左端に表示するラベル
	ImVec4      color = { 1,1,1,1 };		// キーフレームのベース色

	// subType ごとに色を変えたい場合は subColors を使う（空なら color を使う）
	std::vector<ImVec4> subColors;

	std::vector<DopeKey> keys;

	bool readOnly = false;					// true = ドラッグ・追加・削除不可
	bool visible = true;					// false = この行を非表示

	// グループ見出し行として使う場合（keys は無視される）
	bool isGroupHeader = false;
	bool groupExpanded = true;				// 折りたたみ状態
	int  groupDepth = 0;					// インデントレベル
};

//-----------------------------------------------------------------------------
// ドープシートエディタ（汎用）
//-----------------------------------------------------------------------------
class DopeSheetEditor
{
public:
	DopeSheetEditor() = default;

	//=========================================================================
	// メイン描画
	//   tracks     : 呼び出し元が用意したトラック配列（変更される場合あり）
	//   totalFrames: タイムライン全長（フレーム数）
	//   fps        : 1秒あたりのフレーム数（ルーラー表示用）
	//   height     : 描画エリアの高さ（0 = 自動）
	//   returns    : tracks が変更されたら true
	//=========================================================================
	bool Draw(
		const char* id,
		std::vector<DopeTrack>& tracks,
		int                    totalFrames,
		int                    fps = 60,
		float                  height = 0.0f
	);

	//=========================================================================
	// アクセッサ
	//=========================================================================

	// 現在のシークフレーム（-1 = 未設定）
	int  GetSeekFrame() const { return seekFrame_; }
	void SetSeekFrame(int f) { seekFrame_ = f; }

	// ズーム（px/frame）
	float GetZoom() const { return zoomX_; }
	void  SetZoom(float z) { zoomX_ = z; }

	// ビューをリセット
	void ResetView() { zoomX_ = 12.0f; scrollX_ = 0.0f; seekFrame_ = 0; }

	// 右クリック追加ポップアップ用コールバック
	// 引数: trackIndex, frame, value
	using AddKeyCallback = std::function<void(int trackIdx, int frame, float value)>;
	void SetAddKeyCallback(AddKeyCallback cb) { onAddKey_ = cb; }

	// キー削除コールバック（readOnly==falseのとき）
	using DeleteKeyCallback = std::function<void(int trackIdx, int keyIdx)>;
	void SetDeleteKeyCallback(DeleteKeyCallback cb) { onDeleteKey_ = cb; }

	// シークフレーム変化コールバック
	using SeekCallback = std::function<void(int frame)>;
	void SetSeekCallback(SeekCallback cb) { onSeek_ = cb; }

	// ルーラー上に追加の目印を描画するコールバック（オプション）
	using RulerOverlayCallback = std::function<void(ImDrawList*, ImVec2 origin, float cellW, int totalFrames)>;
	void SetRulerOverlayCallback(RulerOverlayCallback cb) { onRulerOverlay_ = cb; }

private:
	//=========================================================================
	// 内部描画
	//=========================================================================
	void DrawRuler(ImDrawList* dl, ImVec2 origin, float cellW, int totalFrames, int fps);
	bool DrawTrackRow(ImDrawList* dl, ImVec2 rowMin, float cellW,
		int totalFrames, DopeTrack& track, int trackIdx);
	void DrawSeekBar(ImDrawList* dl, ImVec2 origin, float timelineH, int totalFrames);
	void DrawAddKeyPopup(std::vector<DopeTrack>& tracks);

	// ひし形キーを描画（ホバー・ドラッグも処理）
	// 戻り値: このキーが変更されたら true
	bool DrawKey(ImDrawList* dl, ImVec2 center, float radius,
		ImU32 fillCol, ImU32 outlineCol,
		DopeKey& key, int trackIdx, int keyIdx,
		float cellW, int totalFrames);

	ImU32 GetKeyColor(const DopeTrack& track, int subType, bool hovered) const;

private:
	//=========================================================================
	// 状態
	//=========================================================================
	float zoomX_ = 12.0f;
	float scrollX_ = 0.0f;
	int   seekFrame_ = 0;

	struct DragState {
		int  trackIdx = -1;
		int  keyIdx = -1;
		bool active = false;
		int  startFrame = 0;
		float startMouseX = 0.0f;
	};
	DragState drag_;

	// 追加ポップアップ
	int   pendingTrackIdx_ = -1;
	int   pendingFrame_ = 0;
	float pendingValue_ = 0.0f;
	bool  showAddPopup_ = false;

	// コールバック
	AddKeyCallback    onAddKey_;
	DeleteKeyCallback onDeleteKey_;
	SeekCallback      onSeek_;
	RulerOverlayCallback onRulerOverlay_;

	//=========================================================================
	// 定数
	//=========================================================================
public:
	static constexpr float kRowH = 22.0f;
	static constexpr float kLabelW = 110.0f;
	static constexpr float kRulerH = 20.0f;
	static constexpr float kHeaderH = 20.0f; // グループヘッダー行高さ
};