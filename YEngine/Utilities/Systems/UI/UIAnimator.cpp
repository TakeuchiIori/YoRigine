#include "UIAnimator.h"

// Engine
#include "UIBase.h"

// C++
#include <algorithm>
#include <cmath>

// ============================================================
// 基本アニメーション : 位置
// ============================================================
void UIAnimator::PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration,
	Easing::Function easing, bool loop) {
	UIAnimation anim;
	anim.type = UIAnimationType::Position;
	anim.easingType = easing;
	anim.startPos = from;
	anim.endPos = to;
	anim.duration = duration;
	anim.elapsed = 0.0f;
	anim.loop = loop;
	anim.delay = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
	target_->SetPosition(from);
}

// ============================================================
// 基本アニメーション : 拡縮
// ============================================================
void UIAnimator::PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration,
	Easing::Function easing, bool loop) {
	UIAnimation anim;
	anim.type = UIAnimationType::Scale;
	anim.easingType = easing;
	anim.startScale = from;
	anim.endScale = to;
	anim.duration = duration;
	anim.elapsed = 0.0f;
	anim.loop = loop;
	anim.delay = 0.0f;
	anim.originalScale = target_->GetScale();  // 現在のスケールを保存

	animations_.push_back(anim);
	isPaused_ = false;
	target_->SetScale(from);
}

// ============================================================
// 基本アニメーション : 回転
// ============================================================
void UIAnimator::PlayRotationAnimation(const Vector3& from, const Vector3& to, float duration,
	Easing::Function easing, bool loop) {
	UIAnimation anim;
	anim.type = UIAnimationType::Rotation;
	anim.easingType = easing;
	anim.startRotation = from;
	anim.endRotation = to;
	anim.duration = duration;
	anim.elapsed = 0.0f;
	anim.loop = loop;
	anim.delay = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
	target_->SetRotation(from);
}

// ============================================================
// 基本アニメーション : アルファ
// ============================================================
void UIAnimator::PlayAlphaAnimation(float from, float to, float duration,
	Easing::Function easing, bool loop) {
	UIAnimation anim;
	anim.type = UIAnimationType::Alpha;
	anim.easingType = easing;
	anim.startAlpha = from;
	anim.endAlpha = to;
	anim.duration = duration;
	anim.elapsed = 0.0f;
	anim.loop = loop;
	anim.delay = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
	target_->SetAlpha(from);
}

// ============================================================
// 基本アニメーション : 色
// ============================================================
void UIAnimator::PlayColorAnimation(const Vector4& from, const Vector4& to, float duration,
	Easing::Function easing, bool loop) {
	UIAnimation anim;
	anim.type = UIAnimationType::Color;
	anim.easingType = easing;
	anim.startColor = from;
	anim.endColor = to;
	anim.duration = duration;
	anim.elapsed = 0.0f;
	anim.loop = loop;
	anim.delay = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
	target_->SetColor(from);
}

// ============================================================
// プリセットアニメーション : 共通投入処理
// ============================================================
void UIAnimator::PlayAnimation(const UIAnimation& anim) {
	UIAnimation newAnim = anim;
	newAnim.elapsed = 0.0f;

	// アニメーション開始時の初期化
	switch (anim.type) {
	case UIAnimationType::Shake:
	case UIAnimationType::Wobble:
		newAnim.originalPos = target_->GetPosition();
		break;
	case UIAnimationType::Pulse:
	case UIAnimationType::Swing:
		newAnim.originalScale = target_->GetScale();
		break;
	default:
		break;
	}

	animations_.push_back(newAnim);
	isPaused_ = false;
}

// ============================================================
// データ駆動クリップ : type → 既存 Play〜 へディスパッチ
// ============================================================
// クリップは start/end を持たないため、各 Play〜 が対象の現在状態を
// 基準にアニメを組み立てる方式に乗せる。基本タイプ(位置/拡縮/回転/色/アルファ)は
// エディタの試し再生と同じく「現在値からの相対変化」として扱う。
// ============================================================
// データ駆動クリップ : 再生開始
// ============================================================
// 同名クリップが既に再生中なら elapsed をリセットして最初からやり直す。
// クリップのコピーを activeClips_ に積み、UpdateClips() が毎フレーム評価する。
void UIAnimator::PlayClip(const UIAnimationClip& clip) {
	if (!target_ || clip.tracks.empty()) return;

	// 同名クリップが既に再生中なら elapsed リセット
	for (auto& ac : activeClips_) {
		if (ac.clip.name == clip.name) {
			ac.elapsed = 0.0f;
			return;
		}
	}

	ActiveClip ac;
	ac.clip = clip;
	ac.elapsed = 0.0f;
	activeClips_.push_back(std::move(ac));
	isPaused_ = false;
}

// ============================================================
// データ駆動クリップ : 名前で停止
// ============================================================
void UIAnimator::StopClip(const std::string& clipName) {
	activeClips_.erase(
		std::remove_if(activeClips_.begin(), activeClips_.end(),
			[&](const ActiveClip& ac) { return ac.clip.name == clipName; }),
		activeClips_.end()
	);
}

// ============================================================
// データ駆動クリップ : 再生中か確認
// ============================================================
bool UIAnimator::IsClipPlaying(const std::string& clipName) const {
	for (const auto& ac : activeClips_)
		if (ac.clip.name == clipName) return true;
	return false;
}

// ============================================================
// データ駆動クリップ : 毎フレーム更新
// ============================================================
// 各 ActiveClip の elapsed を進め、全トラックを Evaluate して target_ へ反映。
// duration 到達後はループなら elapsed をラップ、非ループなら完了コールバック後に削除。
void UIAnimator::UpdateClips(float deltaTime) {
	if (!target_ || activeClips_.empty()) return;

	std::vector<size_t> finished;

	for (size_t i = 0; i < activeClips_.size(); ++i) {
		ActiveClip& ac = activeClips_[i];
		const float dur = ac.clip.duration > 0.0f ? ac.clip.duration : 1.0f;

		ac.elapsed += deltaTime;

		// 正規化時間 [0, 1] を計算
		float t = ac.elapsed / dur;

		if (t >= 1.0f) {
			if (ac.clip.loop) {
				// ループ: 端数を次サイクルへ持ち越す
				ac.elapsed = std::fmod(ac.elapsed, dur);
				t = ac.elapsed / dur;
			}
			else {
				// 非ループ: 終端にクランプして反映後に削除リストへ
				t = 1.0f;
			}
		}

		// 全トラックを評価して target_ へ書き込む
		for (const auto& track : ac.clip.tracks) {
			float value = track.Evaluate(t);
			ApplyTrackValue(track.type, value);
		}

		if (t >= 1.0f && !ac.clip.loop) {
			if (ac.onComplete) ac.onComplete();
			finished.push_back(i);
		}
	}

	// 後ろから削除（インデックスがずれないように）
	for (auto it = finished.rbegin(); it != finished.rend(); ++it)
		activeClips_.erase(activeClips_.begin() + *it);
}

// ============================================================
// トラックタイプ → target_ の Setter へ書き込む
// ============================================================
void UIAnimator::ApplyTrackValue(UIAnimTrackType type, float value) {
	switch (type) {
	case UIAnimTrackType::Alpha:
		target_->SetAlpha(value);
		break;
	case UIAnimTrackType::PosX: {
		auto p = target_->GetPosition(); p.x = value; target_->SetPosition(p);
		break;
	}
	case UIAnimTrackType::PosY: {
		auto p = target_->GetPosition(); p.y = value; target_->SetPosition(p);
		break;
	}
	case UIAnimTrackType::ScaleX: {
		auto s = target_->GetScale(); s.x = value; target_->SetScale(s);
		break;
	}
	case UIAnimTrackType::ScaleY: {
		auto s = target_->GetScale(); s.y = value; target_->SetScale(s);
		break;
	}
	case UIAnimTrackType::RotationZ: {
		auto r = target_->GetRotation(); r.z = value; target_->SetRotation(r);
		break;
	}
	case UIAnimTrackType::R: {
		auto c = target_->GetColor(); c.x = value; target_->SetColor(c);
		break;
	}
	case UIAnimTrackType::G: {
		auto c = target_->GetColor(); c.y = value; target_->SetColor(c);
		break;
	}
	case UIAnimTrackType::B: {
		auto c = target_->GetColor(); c.z = value; target_->SetColor(c);
		break;
	}
	}
}

// ============================================================
// プリセット : フェードイン
// ============================================================
void UIAnimator::PlayFadeIn(float duration, Easing::Function easing) {
	auto anim = UIAnimationPresets::FadeIn(duration, easing);
	PlayAnimation(anim);
}

// ============================================================
// プリセット : フェードアウト
// ============================================================
void UIAnimator::PlayFadeOut(float duration, Easing::Function easing) {
	auto anim = UIAnimationPresets::FadeOut(duration, easing);
	PlayAnimation(anim);
}

// ============================================================
// プリセット : スライドイン
// ============================================================
void UIAnimator::PlaySlideIn(SlideDirection dir, float distance, float duration) {
	auto anim = UIAnimationPresets::SlideIn(dir, distance, duration);
	anim.originalPos = target_->GetPosition();
	anim.elapsed = 0.0f;

	// 開始位置を設定
	target_->SetPosition(anim.originalPos + anim.startPos);

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : スライドアウト
// ============================================================
void UIAnimator::PlaySlideOut(SlideDirection dir, float distance, float duration) {
	auto anim = UIAnimationPresets::SlideOut(dir, distance, duration);
	anim.originalPos = target_->GetPosition();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : ズームイン
// ============================================================
void UIAnimator::PlayZoomIn(float duration) {
	auto anim = UIAnimationPresets::ZoomIn(duration);
	anim.originalScale = Vector2{ 1.0f, 1.0f };
	anim.elapsed = 0.0f;

	target_->SetScale(anim.startScale);
	target_->SetAlpha(anim.startAlpha);

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : ズームアウト
// ============================================================
void UIAnimator::PlayZoomOut(float duration) {
	auto anim = UIAnimationPresets::ZoomOut(duration);
	anim.originalScale = target_->GetScale();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : シェイク
// ============================================================
void UIAnimator::PlayShake(float intensity, float duration) {
	auto anim = UIAnimationPresets::Shake(intensity, duration);
	anim.originalPos = target_->GetPosition();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : パルス
// ============================================================
void UIAnimator::PlayPulse(float scale, float duration, bool loop) {
	auto anim = UIAnimationPresets::Pulse(scale, duration, loop);
	anim.originalScale = target_->GetScale();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : バウンス
// ============================================================
void UIAnimator::PlayBounce(float height, float duration) {
	auto anim = UIAnimationPresets::Bounce(height, duration);
	anim.originalPos = target_->GetPosition();
	anim.elapsed = 0.0f;

	target_->SetPosition(anim.originalPos + anim.startPos);

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : スイング
// ============================================================
void UIAnimator::PlaySwing(float angle, float duration, bool loop) {
	auto anim = UIAnimationPresets::Swing(angle, duration, loop);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : フラッシュ
// ============================================================
void UIAnimator::PlayFlash(float duration, int times) {
	auto anim = UIAnimationPresets::Flash(duration, times);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : ブリンク
// ============================================================
void UIAnimator::PlayBlink(float duration, bool loop) {
	auto anim = UIAnimationPresets::Blink(duration, loop);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : ウォブル
// ============================================================
void UIAnimator::PlayWobble(float intensity, float duration) {
	auto anim = UIAnimationPresets::Wobble(intensity, duration);
	anim.originalPos = target_->GetPosition();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : フリップ
// ============================================================
void UIAnimator::PlayFlip(bool horizontal, float duration) {
	auto anim = UIAnimationPresets::Flip(horizontal, duration);
	anim.originalScale = target_->GetScale();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : 回転登場
// ============================================================
void UIAnimator::PlayRotateIn(float duration) {
	auto anim = UIAnimationPresets::RotateIn(duration);
	anim.elapsed = 0.0f;

	target_->SetRotation(anim.startRotation);
	target_->SetScale(anim.startScale);
	target_->SetAlpha(anim.startAlpha);

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// プリセット : 回転退場
// ============================================================
void UIAnimator::PlayRotateOut(float duration) {
	auto anim = UIAnimationPresets::RotateOut(duration);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// ============================================================
// 制御 : 停止
// ============================================================
void UIAnimator::StopAnimation(UIAnimationType type) {
	if (type == UIAnimationType::None) {
		// すべてのアニメーションを停止
		animations_.clear();
	}
	else {
		// 指定されたタイプのアニメーションのみ停止
		animations_.erase(
			std::remove_if(animations_.begin(), animations_.end(),
				[type](const UIAnimation& anim) { return anim.type == type; }),
			animations_.end()
		);
	}
	isPaused_ = false;
}

// ============================================================
// 制御 : 全停止
// ============================================================
void UIAnimator::StopAllAnimations() {
	animations_.clear();
	activeClips_.clear();
	isPaused_ = false;
}

// ============================================================
// 制御 : 一時停止 / 再開
// ============================================================
void UIAnimator::PauseAnimation() {
	isPaused_ = true;
}

void UIAnimator::ResumeAnimation() {
	isPaused_ = false;
}

// ============================================================
// 制御 : コールバック / 遅延（直近に追加したものへ）
// ============================================================
void UIAnimator::SetCompleteCallback(std::function<void()> callback) {
	if (!animations_.empty()) {
		animations_.back().onComplete = callback;
	}
}

void UIAnimator::SetUpdateCallback(std::function<void()> callback) {
	if (!animations_.empty()) {
		animations_.back().onUpdate = callback;
	}
}

void UIAnimator::SetDelay(float delay) {
	if (!animations_.empty()) {
		animations_.back().delay = delay;
	}
}

// ============================================================
// 更新（全アニメーションの進行）
// ============================================================
void UIAnimator::Update(float deltaTime) {
	if (!target_ || isPaused_) return;

	// データ駆動クリップ（新方式）を先に更新
	UpdateClips(deltaTime);

	if (animations_.empty()) return;

	// 完了したアニメーションを追跡するためのリスト
	std::vector<size_t> completedIndices;

	// すべてのアニメーションを更新
	for (size_t i = 0; i < animations_.size(); ++i) {
		UIAnimation& anim = animations_[i];

		// 遅延処理
		if (anim.delay > 0.0f) {
			anim.delay -= deltaTime;
			continue;
		}

		anim.elapsed += deltaTime;
		// 起動直後や読み込み後は 1 フレームの deltaTime が duration を
		// 大きく超えることがある。t > 1 のまま EaseOutQuad 等に渡すと
		// 大きな負値になり、アルファへそのまま書き込まれるため、
		// 各アニメーションの評価前に必ず [0, 1] へ制限する。
		const float duration = (anim.duration > 0.0f) ? anim.duration : 0.0001f;
		float t = std::clamp(anim.elapsed / duration, 0.0f, 1.0f);

		// アニメーション完了チェック
		bool isComplete = false;
		// 往復時は進行を反転
		if (anim.isReversing) {
			t = 1.0f - t;
		}

		// アニメーションタイプ別の更新
		switch (anim.type) {
		case UIAnimationType::Position:
		case UIAnimationType::Scale:
		case UIAnimationType::Rotation:
		case UIAnimationType::Color:
		case UIAnimationType::Alpha:
		case UIAnimationType::FadeIn:
		case UIAnimationType::FadeOut:
			UpdateBasicAnimation(anim, t);
			break;

		case UIAnimationType::Shake:
			UpdateShakeAnimation(anim, t);
			break;

		case UIAnimationType::Pulse:
			UpdatePulseAnimation(anim, t);
			break;

		case UIAnimationType::Bounce:
			UpdateBounceAnimation(anim, t);
			break;

		case UIAnimationType::Swing:
			UpdateSwingAnimation(anim, t);
			break;

		case UIAnimationType::Flash:
			UpdateFlashAnimation(anim, t);
			break;

		case UIAnimationType::Blink:
			UpdateBlinkAnimation(anim, t);
			break;

		case UIAnimationType::Wobble:
			UpdateWobbleAnimation(anim, t);
			break;

		case UIAnimationType::SlideIn:
		case UIAnimationType::SlideOut:
			UpdateSlideAnimation(anim, t);
			break;

		case UIAnimationType::ZoomIn:
		case UIAnimationType::ZoomOut:
			UpdateZoomAnimation(anim, t);
			break;

		case UIAnimationType::RotateIn:
		case UIAnimationType::RotateOut:
			UpdateRotateInOutAnimation(anim, t);
			break;

		case UIAnimationType::Flip:
			UpdateBasicAnimation(anim, t);
			break;

		default:
			break;
		}

		if (t >= 1.0f) {
			if (anim.pingpong && !anim.isReversing) {
				// 往復アニメーションの折り返し
				anim.isReversing = true;
				anim.elapsed = 0.0f;
				t = 0.0f;
			}
			else if (anim.loop) {
				// ループ
				anim.elapsed = 0.0f;
				if (anim.pingpong) {
					anim.isReversing = !anim.isReversing;
				}
				t = 0.0f;
			}
			else {
				// アニメーション終了
				t = 1.0f;
				isComplete = true;

				if (anim.type == UIAnimationType::Flash) {
					target_->SetAlpha(1.0f);
				}
			}
		}

		// 更新コールバック
		if (anim.onUpdate) {
			anim.onUpdate();
		}

		// 完了処理
		if (isComplete) {
			if (anim.onComplete) {
				anim.onComplete();
			}
			completedIndices.push_back(i);
		}
	}

	// 完了したアニメーションを削除（後ろから削除）
	for (auto it = completedIndices.rbegin(); it != completedIndices.rend(); ++it) {
		animations_.erase(animations_.begin() + *it);
	}
}

// ============================================================
// タイプ別更新 : 基本（線形補間）
// ============================================================
void UIAnimator::UpdateBasicAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	switch (anim.type) {
	case UIAnimationType::Position: {
		Vector3 pos;
		pos.x = anim.startPos.x + (anim.endPos.x - anim.startPos.x) * easedT;
		pos.y = anim.startPos.y + (anim.endPos.y - anim.startPos.y) * easedT;
		pos.z = anim.startPos.z + (anim.endPos.z - anim.startPos.z) * easedT;
		target_->SetPosition(pos);
		break;
	}
	case UIAnimationType::Scale:
	case UIAnimationType::Flip: {
		Vector2 scale;
		scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
		scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
		target_->SetScale(scale);
		break;
	}
	case UIAnimationType::Rotation: {
		Vector3 rot;
		rot.x = anim.startRotation.x + (anim.endRotation.x - anim.startRotation.x) * easedT;
		rot.y = anim.startRotation.y + (anim.endRotation.y - anim.startRotation.y) * easedT;
		rot.z = anim.startRotation.z + (anim.endRotation.z - anim.startRotation.z) * easedT;
		target_->SetRotation(rot);
		break;
	}
	case UIAnimationType::Color: {
		Vector4 color;
		color.x = anim.startColor.x + (anim.endColor.x - anim.startColor.x) * easedT;
		color.y = anim.startColor.y + (anim.endColor.y - anim.startColor.y) * easedT;
		color.z = anim.startColor.z + (anim.endColor.z - anim.startColor.z) * easedT;
		color.w = anim.startColor.w + (anim.endColor.w - anim.startColor.w) * easedT;
		target_->SetColor(color);
		break;
	}
	case UIAnimationType::Alpha:
	case UIAnimationType::FadeIn:
	case UIAnimationType::FadeOut: {
		float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
		target_->SetAlpha(alpha);
		break;
	}
	default:
		break;
	}
}

// ============================================================
// タイプ別更新 : シェイク（減衰振動）
// ============================================================
void UIAnimator::UpdateShakeAnimation(UIAnimation& anim, float t) {
	// 減衰振動
	float damping = 1.0f - t;
	float shake = sin(t * anim.frequency * 3.14159f * 2.0f) * anim.intensity * damping;

	Vector3 offset;
	offset.x = shake * cos(anim.elapsed * 7.0f);
	offset.y = shake * sin(anim.elapsed * 5.0f);
	offset.z = 0.0f;

	target_->SetPosition(anim.originalPos + offset);
}

// ============================================================
// タイプ別更新 : パルス
// ============================================================
void UIAnimator::UpdatePulseAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector2 scale;
	scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
	scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
	target_->SetScale(scale);
}

// ============================================================
// タイプ別更新 : バウンス
// ============================================================
void UIAnimator::UpdateBounceAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector3 pos;
	pos.x = anim.originalPos.x;
	pos.y = anim.originalPos.y + anim.startPos.y * (1.0f - easedT);
	pos.z = anim.originalPos.z;
	target_->SetPosition(pos);
}

// ============================================================
// タイプ別更新 : スイング
// ============================================================
void UIAnimator::UpdateSwingAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector3 rot;
	rot.x = 0.0f;
	rot.y = 0.0f;
	rot.z = anim.startRotation.z + (anim.endRotation.z - anim.startRotation.z) * easedT;
	target_->SetRotation(rot);
}

// ============================================================
// タイプ別更新 : フラッシュ（点滅）
// ============================================================
void UIAnimator::UpdateFlashAnimation(UIAnimation& anim, float t) {
	// 点滅回数に基づいて明滅
	float phase = t * anim.frequency;
	float alpha = (sin(phase * 3.14159f * 2.0f) > 0.0f) ? 1.0f : 0.3f;
	target_->SetAlpha(alpha);
}

// ============================================================
// タイプ別更新 : ブリンク（瞬き）
// ============================================================
void UIAnimator::UpdateBlinkAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
	target_->SetAlpha(alpha);
}

// ============================================================
// タイプ別更新 : ウォブル（ぐらぐら）
// ============================================================
void UIAnimator::UpdateWobbleAnimation(UIAnimation& anim, float t) {
	// ウォブル効果(X方向の振動 + Y方向の小さな振動)
	float wobbleX = sin(t * anim.frequency * 3.14159f * 2.0f) * anim.intensity * (1.0f - t);
	float wobbleY = sin(t * anim.frequency * 3.14159f * 4.0f) * anim.intensity * 0.3f * (1.0f - t);

	Vector3 offset;
	offset.x = wobbleX;
	offset.y = wobbleY;
	offset.z = 0.0f;

	target_->SetPosition(anim.originalPos + offset);
}

// ============================================================
// タイプ別更新 : スライド
// ============================================================
void UIAnimator::UpdateSlideAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector3 offset;
	if (anim.type == UIAnimationType::SlideIn) {
		offset.x = anim.startPos.x * (1.0f - easedT);
		offset.y = anim.startPos.y * (1.0f - easedT);
	}
	else {
		offset.x = anim.endPos.x * easedT;
		offset.y = anim.endPos.y * easedT;
	}
	offset.z = 0.0f;

	target_->SetPosition(anim.originalPos + offset);
}

// ============================================================
// タイプ別更新 : ズーム
// ============================================================
void UIAnimator::UpdateZoomAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector2 scale;
	scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
	scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
	target_->SetScale(scale);

	float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
	target_->SetAlpha(alpha);
}

// ============================================================
// タイプ別更新 : 回転登場 / 退場
// ============================================================
void UIAnimator::UpdateRotateInOutAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	// 回転
	Vector3 rot;
	rot.x = anim.startRotation.x + (anim.endRotation.x - anim.startRotation.x) * easedT;
	rot.y = anim.startRotation.y + (anim.endRotation.y - anim.startRotation.y) * easedT;
	rot.z = anim.startRotation.z + (anim.endRotation.z - anim.startRotation.z) * easedT;
	target_->SetRotation(rot);

	// スケール
	Vector2 scale;
	scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
	scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
	target_->SetScale(scale);

	// アルファ
	float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
	target_->SetAlpha(alpha);
}
