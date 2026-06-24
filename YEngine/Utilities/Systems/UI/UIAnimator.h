#pragma once

// Engine
#include "UIAnimation.h"

// C++
#include <vector>
#include <functional>

// 前方宣言
class UIBase;

// ============================================================
// UIAnimator
// ============================================================
// UIBase のアニメーション再生を担うエンジン。
// 対象 UIBase の公開 Setter/Getter を通して
// 位置・拡縮・回転・色・アルファを時間で動かす。
//
// UIBase は本クラスをメンバとして保持し、Play 系 API を転送する。
// そのため UIBase を使う側のコードは従来どおりで動く。
// ============================================================
class UIAnimator {
public:
	// ============================================================
	// 接続
	// ============================================================
	// 駆動対象の UIBase を設定する（UIBase 側の Initialize で自分を渡す）。
	void SetTarget(UIBase* target) { target_ = target; }

	// ============================================================
	// 基本アニメーション
	// ============================================================
	void PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration,
		Easing::Function easing, bool loop);
	void PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration,
		Easing::Function easing, bool loop);
	void PlayRotationAnimation(const Vector3& from, const Vector3& to, float duration,
		Easing::Function easing, bool loop);
	void PlayAlphaAnimation(float from, float to, float duration,
		Easing::Function easing, bool loop);
	void PlayColorAnimation(const Vector4& from, const Vector4& to, float duration,
		Easing::Function easing, bool loop);

	// ============================================================
	// プリセットアニメーション
	// ============================================================
	void PlayAnimation(const UIAnimation& anim);

	// ============================================================
	// データ駆動クリップ再生（新キーフレーム方式）
	// ============================================================
	// クリップを再生開始する。同名クリップが再生中なら最初からやり直す。
	void PlayClip(const UIAnimationClip& clip);
	// 名前でクリップを停止する
	void StopClip(const std::string& clipName);
	// 指定名のクリップが再生中か
	bool IsClipPlaying(const std::string& clipName) const;
	void PlayFadeIn(float duration, Easing::Function easing);
	void PlayFadeOut(float duration, Easing::Function easing);
	void PlaySlideIn(SlideDirection dir, float distance, float duration);
	void PlaySlideOut(SlideDirection dir, float distance, float duration);
	void PlayZoomIn(float duration);
	void PlayZoomOut(float duration);
	void PlayShake(float intensity, float duration);
	void PlayPulse(float scale, float duration, bool loop);
	void PlayBounce(float height, float duration);
	void PlaySwing(float angle, float duration, bool loop);
	void PlayFlash(float duration, int times);
	void PlayBlink(float duration, bool loop);
	void PlayWobble(float intensity, float duration);
	void PlayFlip(bool horizontal, float duration);
	void PlayRotateIn(float duration);
	void PlayRotateOut(float duration);

	// ============================================================
	// 制御
	// ============================================================
	void StopAnimation(UIAnimationType type = UIAnimationType::None);  // None で全停止
	void StopAllAnimations();
	void PauseAnimation();
	void ResumeAnimation();

	// 直近に追加したアニメーションへの設定
	void SetCompleteCallback(std::function<void()> callback);
	void SetUpdateCallback(std::function<void()> callback);
	void SetDelay(float delay);

	bool IsAnimating() const { return !animations_.empty() || !activeClips_.empty(); }
	bool IsPaused() const { return isPaused_; }

	// ============================================================
	// 更新
	// ============================================================
	void Update(float deltaTime);

	// インスペクタ等から中身を参照するためのアクセサ
	std::vector<UIAnimation>& GetAnimations() { return animations_; }
	const std::vector<UIAnimation>& GetAnimations() const { return animations_; }

private:
	// ============================================================
	// 再生中クリップの状態
	// ============================================================
	struct ActiveClip {
		UIAnimationClip clip;      // クリップのコピー（再生中に参照）
		float elapsed = 0.0f;      // 経過秒数
		std::function<void()> onComplete;
	};
	// ============================================================
	// タイプ別更新処理
	// ============================================================
	void UpdateBasicAnimation(UIAnimation& anim, float t);
	void UpdateShakeAnimation(UIAnimation& anim, float t);
	void UpdatePulseAnimation(UIAnimation& anim, float t);
	void UpdateBounceAnimation(UIAnimation& anim, float t);
	void UpdateSwingAnimation(UIAnimation& anim, float t);
	void UpdateFlashAnimation(UIAnimation& anim, float t);
	void UpdateBlinkAnimation(UIAnimation& anim, float t);
	void UpdateWobbleAnimation(UIAnimation& anim, float t);
	void UpdateSlideAnimation(UIAnimation& anim, float t);
	void UpdateZoomAnimation(UIAnimation& anim, float t);
	void UpdateRotateInOutAnimation(UIAnimation& anim, float t);

	// クリップ更新（毎フレーム全トラックを Evaluate して target_ に書き込む）
	void UpdateClips(float deltaTime);
	// トラック値を target_ の対応 Setter に書き込む
	void ApplyTrackValue(UIAnimTrackType type, float value);

	// ============================================================
	// メンバ変数
	// ============================================================
	UIBase* target_ = nullptr;
	std::vector<UIAnimation> animations_;       // プリセット系（旧方式）
	std::vector<ActiveClip>  activeClips_;      // データ駆動クリップ（新方式）
	bool isPaused_ = false;
};
