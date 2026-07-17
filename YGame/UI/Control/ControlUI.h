#pragma once
#include <Systems/UI/UIManager.h>
#include <Systems/UI/UIBase.h>
#include "GameObjects/Player/Style/PlayerStyle.h"

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
	// UI はスタイル値だけを受け取り、PlayerCombat / PlayerMagic の中身には触れない。
	void SetPlayerStyle(PlayerStyle style);

	// ロックオン照準を一瞬出して「赤→黄にフェードしながら消える」演出を1回再生する。
	// 画面外の敵に攻撃が当たってカメラが敵方向へ向いた瞬間に呼ぶ。
	void FlashLockOn();

	// フラッシュ照準のスクリーン座標を設定（GameScene が敵を投影して毎フレーム渡す＝敵追従）
	void SetLockOnFlashPos(const Vector3& screenPos) {
		if (lockOnFlash_) lockOnFlash_->SetPosition(screenPos);
	}
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
	void ApplyStyleTextures();
	// 操作表示はUIレイアウトの責務なので、入力スタイルの画像差し替えで
	// Sprite 側の「テクスチャ実寸に合わせる」都合を外へ漏らさない。
	void SetButtonTextureKeepLayout(UIBase* button, const std::string& texturePath);
private:
	///************************* メンバ変数 *************************///

	UIBase* button_[4] = {};
	UIBase* ripples = nullptr;

	// A/B/X 攻撃ボタンの背面アウトライン枠（outlineUI〜outlineUI4）。攻撃ボタンと一緒に戦闘中のみ表示。
	UIBase* attackOutline_[4] = {};
	// A/B/X 攻撃ボタンの黒背景（buttonBG 系）。これも攻撃ボタンと一緒に戦闘中のみ表示。
	UIBase* buttonBG_[4] = {};

	// LB / RB の操作ヒント（バトル中に押すとアニメ）
	UIBase* lbButton_ = nullptr;
	UIBase* rbButton_ = nullptr;
	bool    battleActive_ = false;
	PlayerStyle currentStyle_ = PlayerStyle::Sword;

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
