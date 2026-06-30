#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>

#include <list>
/// <summary>
/// ゲームシーンで表示するUIクラス
/// </summary>
class ControlUI
{
public:
	///************************* 基本的な関数 *************************///
	void Initialize();
	void Update();
	void Draw();

	// バトル中かどうか（LB/RB の押下アニメはバトル中のみ発火する）
	void SetBattleActive(bool active) { battleActive_ = active; }

	// ロックオン照準を一瞬出して「赤→黄にフェードしながら消える」演出を1回再生する。
	// 画面外の敵に攻撃が当たってカメラが敵方向へ向いた瞬間に呼ぶ。
	void FlashLockOn();
private:
	///************************* 内部関数 *************************///

	// 波紋エフェクトの更新と削除
	void UpdateRipples();
	// 波紋を特定のボタン位置で再生する
	void TriggerRipple(UIBase* targetButton);
	// 指定IDのUIを取得。無ければ生成して layer1 に登録する（LB/RB ヒント用）
	UIBase* GetOrCreateButton(const std::string& id, const std::string& texturePath,
		const Vector2& pos, const Vector2& size);
	// ボタン押下時の共通アニメ（波紋＋拡縮ポップ＋フラッシュ）。A/B と全く同じ挙動。
	void PlayButtonPress(UIBase* button);
private:
	///************************* メンバ変数 *************************///

	UIBase* button_[3];
	UIBase* ripples = nullptr;

	// LB / RB の操作ヒント（バトル中に押すとアニメ）
	UIBase* lbButton_ = nullptr;
	UIBase* rbButton_ = nullptr;
	bool    battleActive_ = false;

	// ロックオン操作ヒント（照準アイコン＋右スティック＋「ロックオン」文字）。バトル中のみ表示。
	UIBase* lockOnIcon_  = nullptr;  // LockOn.png（照準）
	UIBase* lockOnStick_ = nullptr;  // Right stick.png

	// 画面外ヒット時に一瞬だけ出す照準（赤→黄フェード）。普段は非表示、FlashLockOn で再生。
	UIBase* lockOnFlash_ = nullptr;

	// スティック押し込みバウンド（手動・位置アニメと違いエディタ移動と両立する）
	//   基準位置に sin オフセットを足して毎フレーム適用。エディタで動かされたら
	//   前回オフセットを差し引いて基準を再取得する（テレスコープ方式）。
	Vector3 stickBase_{};
	float   stickBobPhase_    = 0.0f;
	float   stickPrevOffsetY_ = 0.0f;
	bool    stickBaseInit_    = false;
	std::list<std::unique_ptr<UIBase>> activeRipples_;
	// 波紋のテクスチャパス（キャッシュ用）
	std::string rippleTexturePath_;

	float duration_ = 0.3f;
	Vector2 originalSize_{};
	Vector2 pushSize_ = { 80.0f ,80.0f };

	bool isVisble_ = true;
};
