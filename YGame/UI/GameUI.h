#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>
#include "Control/ControlUI.h"
#include <map>
/// <summary>
/// ゲームシーンで表示するUIクラス
/// </summary>
class GameUI
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void DrawAll();
	void Draw();

	// バトル中かどうかを ControlUI へ伝える（LB/RB 押下アニメのゲート用）
	void SetBattleActive(bool active);

	// 画面外ヒット時のロックオン照準フラッシュを再生する（ControlUI へ委譲）
	void FlashLockOn();
	// フラッシュ照準のスクリーン座標を更新（敵追従用・ControlUI へ委譲）
	void SetLockOnFlashPos(const Vector3& screenPos);

	/// <summary>
	///  ゲームオーバーUIをフェード表示する
	/// </summary>
	/// <param name="duration">フェード時間（秒）</param>
	void ShowGameOverWithFade(float duration = 0.6f);

	/// <summary>
	///  ゲームオーバーUIの状態をリセットし、描画を停止する
	/// </summary>
	void ResetGameOver();

	/// <summary>
	///  フェードが完了したかどうかを取得
	/// </summary>
	bool IsFadeCompleted() const { return goFadeCompleted_; }

	/// <summary>
	///  リトライボタンが押されたかチェック
	/// </summary>
	bool IsRetryRequested() const { return retryRequested_; }

	/// <summary>
	///  タイトルに戻るボタンが押されたかチェック
	/// </summary>
	bool IsReturnToTitleRequested() const { return returnToTitleRequested_; }

	/// <summary>
	///  リクエストフラグをリセット
	/// </summary>
	void ClearRequests();

private:

	void UpdatePauseVisuals(float t);
	// ポーズメニューの「隠れ状態」を、保存しても壊れない値（作者値スケール/色＋visible=false）
	// に戻す。scale0/alpha0 のまま放置して UI 保存で焼き付くのを防ぐ。
	void RestorePauseRestingState();
	// ポーズ画面用 UI を取得。無ければ layer2 で生成して configPath を持たせる
	// （個別保存・次回起動の復元のため）。size の x/y が 0 ならテクスチャ実寸を使う。
	UIBase* GetOrCreatePauseUI(const std::string& id, const std::string& texturePath,
		const Vector2& pos, const Vector2& size);

	// ポーズ画面のレイアウトを構築（2パネル：左メニュー＋右操作一覧）。
	// 既存UIは再配置、無いものは生成。layer2 キャプチャより前に呼ぶ。
	void BuildPauseLayout();
	// レイアウト用：UIを取得/生成し、位置・サイズ・色・レイヤーを強制設定する。
	//   tex 空 = テクスチャ据え置き（既存流用） / size 0 = 実寸据え置き。
	UIBase* SetupPauseUI(const std::string& id, const std::string& tex,
		const Vector2& pos, const Vector2& size, const Vector4& color);

	void ApplyAlpha(float a);
private:
	///************************* メンバ変数 *************************///
	UIBase* gameOver_ = nullptr;
	UIBase* background_ = nullptr;
	UIBase* padA_ = nullptr;
	UIBase* padB_ = nullptr;
	UIBase* padLB_ = nullptr;
	UIBase* strong_ = nullptr;
	UIBase* weak_ = nullptr;
	UIBase* guard_ = nullptr;
	UIBase* uiBackGround_ = nullptr;
	UIBase* startButton_ = nullptr;
	UIBase* toTitleButton_ = nullptr;
	UIBase* toGameButton_ = nullptr;

	// ゲームオーバー関連UI
	UIBase* retryButton_ = nullptr;
	UIBase* titleButton_ = nullptr;

	// フェード管理
	bool   goVisible_ = false;  // 表示中か（描画のON/OFF）
	bool   goFadingIn_ = false;  // フェード中か
	bool   goFadeCompleted_ = false;  // フェード完了フラグ
	float  goFadeTimer_ = 0.0f;   // 経過時間
	float  goFadeDuration_ = 1.6f;   // フェード時間
	float  goAlpha_ = 0.0f;   // 現在アルファ（0～1）

	// ゲームオーバー選択関連
	bool retryRequested_ = false;
	bool returnToTitleRequested_ = false;

	// 選択UI管理
	enum class GameOverSelection {
		RETRY,
		TITLE
	};
	GameOverSelection currentSelection_ = GameOverSelection::RETRY;
	float selectionScale_ = 1.25f;  // 選択時の拡大率
	float normalScale_ = 1.0f;     // 通常時のスケール
	Vector2 retryButtonOriginalSize_ = { 0.0f, 0.0f };  // リトライボタンの元のサイズ
	Vector2 titleButtonOriginalSize_ = { 0.0f, 0.0f };  // タイトルボタンの元のサイズ

	// 操作用UI関連
	bool isVisible_ = false;

	// ポーズメニュー（操作UI）の選択管理
	enum class PauseSelection {
		TO_GAME,    // ゲームに戻る
		TO_TITLE    // タイトルへ
	};
	PauseSelection currentPauseSelection_ = PauseSelection::TO_GAME; // 初期選択はゲームに戻る
	Vector2 toGameOriginalSize_ = { 0.0f, 0.0f };   // toGameボタンの元サイズ
	Vector2 toTitleOriginalSize_ = { 0.0f, 0.0f };  // toTitleボタンの元サイズ

	std::map<UIBase*, Vector4> pauseOriginalColors_;
	std::map<UIBase*, Vector2> pauseOriginalScales_;

	// ポーズメニューのアニメーション管理用
	enum class PauseState {
		HIDDEN,     // 非表示
		OPENING,    // 開くアニメーション中
		ACTIVE,     // 操作可能
		CLOSING     // 閉じるアニメーション中
	};
	PauseState pauseState_ = PauseState::HIDDEN;

	float pauseAnimTimer_ = 0.0f;       // アニメーション経過時間
	const float pauseAnimDuration_ = 0.125f; // アニメーションにかかる時間（秒）

	// ポーズ中だけ追加する背景ブラーのエフェクト index（-1=未追加）
	int pauseBlurIndex_ = -1;

	// 選択ハイライト（左メニューの選択中項目の裏に出す金バー）
	UIBase* pauseSelectHighlight_ = nullptr;

private:
	///************************* 他のUI *************************///
	std::unique_ptr<ControlUI> controlUI_;
};