#include "UIBase.h"
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include "Sprite/SpriteCommon.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Systems/GameTime/GameTime.h>

const std::string UIBase::PRESET_DIRECTORY = "./Resources/Json/UI/";

UIBase::UIBase(const std::string& name) :
	sprite_(nullptr),
	hotReloadEnabled_(false),
	name_(name) {
}

UIBase::~UIBase() {

}

void UIBase::Initialize(const std::string& jsonConfigPath) {
	configPath_ = jsonConfigPath;

	sprite_ = std::make_unique<Sprite>();

	bool jsonExists = std::filesystem::exists(jsonConfigPath);

	if (jsonExists) {
		LoadFromJSON(jsonConfigPath);
	} else {
		sprite_->Initialize("./Resources/images/white.png");
		texturePath_ = "./Resources/images/white.png";
		SaveToJSON();
	}

	if (std::filesystem::exists(configPath_)) {
		lastModTime_ = std::filesystem::last_write_time(configPath_);
	}
}

void UIBase::Update() {
	if (IsAnimating()) {
		UpdateAnimation(YoRigine::GameTime::GetDeltaTime());
	}

	if (hotReloadEnabled_) {
		CheckForChanges();
	}

	if (sprite_ && visible_) {
		sprite_->Update();
	}

#ifdef USE_IMGUI
	//ImGUi();
#endif
}

void UIBase::Draw() {
	if (sprite_ && visible_) {
		sprite_->Draw();
	}
}

/*==================================================================
						アニメーション機能
===================================================================*/

// シンプルなアニメーション
void UIBase::PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration,
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
	SetPosition(from);
}

void UIBase::PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration,
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
	anim.originalScale = GetScale();  // 現在のスケールを保存

	animations_.push_back(anim);
	isPaused_ = false;
	SetScale(from);
}

void UIBase::PlayRotationAnimation(const Vector3& from, const Vector3& to, float duration,
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
	SetRotation(from);
}

void UIBase::PlayAlphaAnimation(float from, float to, float duration,
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
	SetAlpha(from);
}

void UIBase::PlayColorAnimation(const Vector4& from, const Vector4& to, float duration,
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
	SetColor(from);
}

// プリセットアニメーション
void UIBase::PlayAnimation(const UIAnimation& anim) {
	UIAnimation newAnim = anim;
	newAnim.elapsed = 0.0f;

	// アニメーション開始時の初期化
	switch (anim.type) {
	case UIAnimationType::Shake:
	case UIAnimationType::Wobble:
		newAnim.originalPos = GetPosition();
		break;
	case UIAnimationType::Pulse:
	case UIAnimationType::Swing:
		newAnim.originalScale = GetScale();
		break;
	default:
		break;
	}

	animations_.push_back(newAnim);
	isPaused_ = false;
}

void UIBase::PlayFadeIn(float duration, Easing::Function easing) {
	auto anim = UIAnimationPresets::FadeIn(duration, easing);
	PlayAnimation(anim);
}

void UIBase::PlayFadeOut(float duration, Easing::Function easing) {
	auto anim = UIAnimationPresets::FadeOut(duration, easing);
	PlayAnimation(anim);
}

void UIBase::PlaySlideIn(SlideDirection dir, float distance, float duration) {
	auto anim = UIAnimationPresets::SlideIn(dir, distance, duration);
	anim.originalPos = GetPosition();
	anim.elapsed = 0.0f;

	// 開始位置を設定
	SetPosition(anim.originalPos + anim.startPos);

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlaySlideOut(SlideDirection dir, float distance, float duration) {
	auto anim = UIAnimationPresets::SlideOut(dir, distance, duration);
	anim.originalPos = GetPosition();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayZoomIn(float duration) {
	auto anim = UIAnimationPresets::ZoomIn(duration);
	anim.originalScale = Vector2{ 1.0f, 1.0f };
	anim.elapsed = 0.0f;

	SetScale(anim.startScale);
	SetAlpha(anim.startAlpha);

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayZoomOut(float duration) {
	auto anim = UIAnimationPresets::ZoomOut(duration);
	anim.originalScale = GetScale();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayShake(float intensity, float duration) {
	auto anim = UIAnimationPresets::Shake(intensity, duration);
	anim.originalPos = GetPosition();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayPulse(float scale, float duration, bool loop) {
	auto anim = UIAnimationPresets::Pulse(scale, duration, loop);
	anim.originalScale = GetScale();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayBounce(float height, float duration) {
	auto anim = UIAnimationPresets::Bounce(height, duration);
	anim.originalPos = GetPosition();
	anim.elapsed = 0.0f;

	SetPosition(anim.originalPos + anim.startPos);

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlaySwing(float angle, float duration, bool loop) {
	auto anim = UIAnimationPresets::Swing(angle, duration, loop);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayFlash(float duration, int times) {
	auto anim = UIAnimationPresets::Flash(duration, times);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayBlink(float duration, bool loop) {
	auto anim = UIAnimationPresets::Blink(duration, loop);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayWobble(float intensity, float duration) {
	auto anim = UIAnimationPresets::Wobble(intensity, duration);
	anim.originalPos = GetPosition();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayFlip(bool horizontal, float duration) {
	auto anim = UIAnimationPresets::Flip(horizontal, duration);
	anim.originalScale = GetScale();
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayRotateIn(float duration) {
	auto anim = UIAnimationPresets::RotateIn(duration);
	anim.elapsed = 0.0f;

	SetRotation(anim.startRotation);
	SetScale(anim.startScale);
	SetAlpha(anim.startAlpha);

	animations_.push_back(anim);
	isPaused_ = false;
}

void UIBase::PlayRotateOut(float duration) {
	auto anim = UIAnimationPresets::RotateOut(duration);
	anim.elapsed = 0.0f;

	animations_.push_back(anim);
	isPaused_ = false;
}

// アニメーション制御
void UIBase::StopAnimation(UIAnimationType type) {
	if (type == UIAnimationType::None) {
		// すべてのアニメーションを停止
		animations_.clear();
	} else {
		// 指定されたタイプのアニメーションのみ停止
		animations_.erase(
			std::remove_if(animations_.begin(), animations_.end(),
				[type](const UIAnimation& anim) { return anim.type == type; }),
			animations_.end()
		);
	}
	isPaused_ = false;
}

void UIBase::StopAllAnimations() {
	animations_.clear();
	isPaused_ = false;
}

void UIBase::PauseAnimation() {
	isPaused_ = true;
}

void UIBase::ResumeAnimation() {
	isPaused_ = false;
}

void UIBase::SetAnimationCompleteCallback(std::function<void()> callback) {
	if (!animations_.empty()) {
		animations_.back().onComplete = callback;
	}
}

void UIBase::SetAnimationUpdateCallback(std::function<void()> callback) {
	if (!animations_.empty()) {
		animations_.back().onUpdate = callback;
	}
}

/*==================================================================
					アニメーション更新処理
===================================================================*/

void UIBase::UpdateAnimation(float deltaTime) {
	if (!IsAnimating() || isPaused_) return;

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
		float t = anim.elapsed / anim.duration;

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
			} else if (anim.loop) {
				// ループ
				anim.elapsed = 0.0f;
				if (anim.pingpong) {
					anim.isReversing = !anim.isReversing;
				}
				t = 0.0f;
			} else {
				// アニメーション終了
				t = 1.0f;
				isComplete = true;

				if (anim.type == UIAnimationType::Flash) {
					this->SetAlpha(1.0f);
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

void UIBase::UpdateBasicAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	switch (anim.type) {
	case UIAnimationType::Position: {
		Vector3 pos;
		pos.x = anim.startPos.x + (anim.endPos.x - anim.startPos.x) * easedT;
		pos.y = anim.startPos.y + (anim.endPos.y - anim.startPos.y) * easedT;
		pos.z = anim.startPos.z + (anim.endPos.z - anim.startPos.z) * easedT;
		SetPosition(pos);
		break;
	}
	case UIAnimationType::Scale:
	case UIAnimationType::Flip: {
		Vector2 scale;
		scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
		scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
		SetScale(scale);
		break;
	}
	case UIAnimationType::Rotation: {
		Vector3 rot;
		rot.x = anim.startRotation.x + (anim.endRotation.x - anim.startRotation.x) * easedT;
		rot.y = anim.startRotation.y + (anim.endRotation.y - anim.startRotation.y) * easedT;
		rot.z = anim.startRotation.z + (anim.endRotation.z - anim.startRotation.z) * easedT;
		SetRotation(rot);
		break;
	}
	case UIAnimationType::Color: {
		Vector4 color;
		color.x = anim.startColor.x + (anim.endColor.x - anim.startColor.x) * easedT;
		color.y = anim.startColor.y + (anim.endColor.y - anim.startColor.y) * easedT;
		color.z = anim.startColor.z + (anim.endColor.z - anim.startColor.z) * easedT;
		color.w = anim.startColor.w + (anim.endColor.w - anim.startColor.w) * easedT;
		SetColor(color);
		break;
	}
	case UIAnimationType::Alpha:
	case UIAnimationType::FadeIn:
	case UIAnimationType::FadeOut: {
		float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
		SetAlpha(alpha);
		break;
	}
	default:
		break;
	}
}

void UIBase::UpdateShakeAnimation(UIAnimation& anim, float t) {
	// 減衰振動
	float damping = 1.0f - t;
	float shake = sin(t * anim.frequency * 3.14159f * 2.0f) * anim.intensity * damping;

	Vector3 offset;
	offset.x = shake * cos(anim.elapsed * 7.0f);
	offset.y = shake * sin(anim.elapsed * 5.0f);
	offset.z = 0.0f;

	SetPosition(anim.originalPos + offset);
}

void UIBase::UpdatePulseAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector2 scale;
	scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
	scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
	SetScale(scale);
}

void UIBase::UpdateBounceAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector3 pos;
	pos.x = anim.originalPos.x;
	pos.y = anim.originalPos.y + anim.startPos.y * (1.0f - easedT);
	pos.z = anim.originalPos.z;
	SetPosition(pos);
}

void UIBase::UpdateSwingAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector3 rot;
	rot.x = 0.0f;
	rot.y = 0.0f;
	rot.z = anim.startRotation.z + (anim.endRotation.z - anim.startRotation.z) * easedT;
	SetRotation(rot);
}

void UIBase::UpdateFlashAnimation(UIAnimation& anim, float t) {
	// 点滅回数に基づいて明滅
	float phase = t * anim.frequency;
	float alpha = (sin(phase * 3.14159f * 2.0f) > 0.0f) ? 1.0f : 0.3f;
	SetAlpha(alpha);
}

void UIBase::UpdateBlinkAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
	SetAlpha(alpha);
}

void UIBase::UpdateWobbleAnimation(UIAnimation& anim, float t) {
	// ウォブル効果(X方向の振動 + Y方向の小さな振動)
	float wobbleX = sin(t * anim.frequency * 3.14159f * 2.0f) * anim.intensity * (1.0f - t);
	float wobbleY = sin(t * anim.frequency * 3.14159f * 4.0f) * anim.intensity * 0.3f * (1.0f - t);

	Vector3 offset;
	offset.x = wobbleX;
	offset.y = wobbleY;
	offset.z = 0.0f;

	SetPosition(anim.originalPos + offset);
}

void UIBase::UpdateSlideAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector3 offset;
	if (anim.type == UIAnimationType::SlideIn) {
		offset.x = anim.startPos.x * (1.0f - easedT);
		offset.y = anim.startPos.y * (1.0f - easedT);
	} else {
		offset.x = anim.endPos.x * easedT;
		offset.y = anim.endPos.y * easedT;
	}
	offset.z = 0.0f;

	SetPosition(anim.originalPos + offset);
}

void UIBase::UpdateZoomAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	Vector2 scale;
	scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
	scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
	SetScale(scale);

	float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
	SetAlpha(alpha);
}

void UIBase::UpdateRotateInOutAnimation(UIAnimation& anim, float t) {
	float easedT = Easing::Ease(anim.easingType, t);

	// 回転
	Vector3 rot;
	rot.x = anim.startRotation.x + (anim.endRotation.x - anim.startRotation.x) * easedT;
	rot.y = anim.startRotation.y + (anim.endRotation.y - anim.startRotation.y) * easedT;
	rot.z = anim.startRotation.z + (anim.endRotation.z - anim.startRotation.z) * easedT;
	SetRotation(rot);

	// スケール
	Vector2 scale;
	scale.x = anim.startScale.x + (anim.endScale.x - anim.startScale.x) * easedT;
	scale.y = anim.startScale.y + (anim.endScale.y - anim.startScale.y) * easedT;
	SetScale(scale);

	// アルファ
	float alpha = anim.startAlpha + (anim.endAlpha - anim.startAlpha) * easedT;
	SetAlpha(alpha);
}

/*==================================================================
						グリッド・スナップ
===================================================================*/

Vector3 UIBase::SnapToGrid(const Vector3& position) const {
	if (!gridEnabled_) return position;

	Vector3 snapped;
	snapped.x = std::round(position.x / gridSize_) * gridSize_;
	snapped.y = std::round(position.y / gridSize_) * gridSize_;
	snapped.z = position.z;
	return snapped;
}

/*==================================================================
						プリセット機能
===================================================================*/

bool UIBase::SaveAsPreset(const std::string& presetName) {
	if (!std::filesystem::exists(PRESET_DIRECTORY)) {
		std::filesystem::create_directories(PRESET_DIRECTORY);
	}

	std::string presetPath = PRESET_DIRECTORY + presetName + ".json";
	return SaveToJSON(presetPath);
}

bool UIBase::LoadPreset(const std::string& presetName) {
	std::string presetPath = PRESET_DIRECTORY + presetName + ".json";
	if (!std::filesystem::exists(presetPath)) {
		return false;
	}
	return LoadFromJSON(presetPath);
}

std::vector<std::string> UIBase::GetAvailablePresets() const {
	std::vector<std::string> presets;

	if (!std::filesystem::exists(PRESET_DIRECTORY)) {
		return presets;
	}

	for (const auto& entry : std::filesystem::directory_iterator(PRESET_DIRECTORY)) {
		if (entry.is_regular_file() && entry.path().extension() == ".json") {
			presets.push_back(entry.path().stem().string());
		}
	}
	std::sort(presets.begin(), presets.end());
	return presets;
}

/*==================================================================
						プロパティコピー
===================================================================*/

void UIBase::CopyPropertiesFrom(const UIBase* other) {
	if (!other) return;

	SetPosition(other->GetPosition());
	SetRotation(other->GetRotation());
	SetScale(other->GetScale());
	SetColor(other->GetColor());
	SetFlipX(other->GetFlipX());
	SetFlipY(other->GetFlipY());
	SetAnchorPoint(other->GetAnchorPoint());
	SetTexture(other->GetTexturePath());
	SetTextureLeftTop(other->GetTextureLeftTop());
	SetTextureSize(other->GetTextureSize());
	SetUVTranslation(other->GetUVTranslation());
	SetUVRotation(other->GetUVRotation());
	SetUVScale(other->GetUVScale());
}

/*==================================================================
						ImGui拡張
===================================================================*/

void UIBase::ImGuiGridSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("グリッド設定")) {
		ImGui::Checkbox("グリッドを有効化", &gridEnabled_);

		if (gridEnabled_) {
			ImGui::DragFloat("グリッドサイズ", &gridSize_, 1.0f, 1.0f, 100.0f);

			if (ImGui::Button("位置をグリッドにスナップ")) {
				SetPosition(SnapToGrid(GetPosition()));
			}
		}
	}
#endif
}

/*==================================================================
					ImGui拡張アニメーション設定
===================================================================*/

void UIBase::ImGuiAnimationSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("アニメーション", ImGuiTreeNodeFlags_DefaultOpen)) {

		if (IsAnimating()) {
			ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "▶ アニメーション再生中...");
			ImGui::Text("アクティブなアニメーション数: %zu", animations_.size());

			ImGui::Spacing();

			// 各アニメーションの進捗を表示
			for (size_t i = 0; i < animations_.size(); ++i) {
				const UIAnimation& anim = animations_[i];

				ImGui::PushID(static_cast<int>(i));

				// アニメーションタイプを表示
				const char* typeName = "Unknown";
				switch (anim.type) {
				case UIAnimationType::Position: typeName = "位置"; break;
				case UIAnimationType::Scale: typeName = "スケール"; break;
				case UIAnimationType::Rotation: typeName = "回転"; break;
				case UIAnimationType::Alpha: typeName = "アルファ"; break;
				case UIAnimationType::Color: typeName = "色"; break;
				case UIAnimationType::FadeIn: typeName = "フェードイン"; break;
				case UIAnimationType::FadeOut: typeName = "フェードアウト"; break;
				case UIAnimationType::SlideIn: typeName = "スライドイン"; break;
				case UIAnimationType::SlideOut: typeName = "スライドアウト"; break;
				case UIAnimationType::ZoomIn: typeName = "ズームイン"; break;
				case UIAnimationType::ZoomOut: typeName = "ズームアウト"; break;
				case UIAnimationType::Shake: typeName = "シェイク"; break;
				case UIAnimationType::Pulse: typeName = "パルス"; break;
				case UIAnimationType::Bounce: typeName = "バウンス"; break;
				case UIAnimationType::Swing: typeName = "スイング"; break;
				case UIAnimationType::Flash: typeName = "フラッシュ"; break;
				case UIAnimationType::Blink: typeName = "ブリンク"; break;
				case UIAnimationType::Wobble: typeName = "ウォブル"; break;
				case UIAnimationType::Flip: typeName = "フリップ"; break;
				case UIAnimationType::RotateIn: typeName = "回転イン"; break;
				case UIAnimationType::RotateOut: typeName = "回転アウト"; break;
				}

				ImGui::Text("%zu: %s", i + 1, typeName);

				float progress = anim.elapsed / anim.duration;
				ImGui::ProgressBar(progress, ImVec2(-1, 0));
				ImGui::Text("%.2f / %.2f秒", anim.elapsed, anim.duration);

				ImGui::PopID();
				ImGui::Spacing();
			}

			// 制御ボタン
			if (isPaused_) {
				if (ImGui::Button("▶ 再開", ImVec2(100, 0))) {
					ResumeAnimation();
				}
			} else {
				if (ImGui::Button("⏸ 一時停止", ImVec2(100, 0))) {
					PauseAnimation();
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("■ 全停止", ImVec2(100, 0))) {
				StopAllAnimations();
			}

			ImGui::Separator();
		} else {
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "アニメーション停止中");
			ImGui::Separator();
		}

		ImGui::Spacing();

		// アニメーション設定
		static int selectedAnimType = 0;
		static int selectedEasing = 0;
		static float duration = 1.0f;
		static bool loop = false;
		static float intensity = 1.0f;

		ImGui::Text("📝 アニメーション設定");

		// アニメーションタイプ選択
		const char* animTypes[] = {
			"位置", "スケール", "回転", "アルファ", "色",
			"フェードイン", "フェードアウト",
			"スライドイン", "スライドアウト",
			"ズームイン", "ズームアウト",
			"シェイク", "パルス", "バウンス", "スイング",
			"フラッシュ", "ブリンク", "ウォブル", "フリップ",
			"回転イン", "回転アウト"
		};
		ImGui::Combo("タイプ", &selectedAnimType, animTypes, IM_ARRAYSIZE(animTypes));

		// イージング選択
		const char* easingTypes[] = {
			"Linear",
			"EaseInSine", "EaseOutSine", "EaseInOutSine",
			"EaseInQuad", "EaseOutQuad", "EaseInOutQuad",
			"EaseInCubic", "EaseOutCubic", "EaseInOutCubic",
			"EaseInQuart", "EaseOutQuart", "EaseInOutQuart",
			"EaseInQuint", "EaseOutQuint", "EaseInOutQuint",
			"EaseInExpo", "EaseOutExpo", "EaseInOutExpo",
			"EaseInCirc", "EaseOutCirc", "EaseInOutCirc",
			"EaseInBack", "EaseOutBack", "EaseInOutBack",
			"EaseInElastic", "EaseOutElastic", "EaseInOutElastic",
			"EaseInBounce", "EaseOutBounce", "EaseInOutBounce"
		};
		ImGui::Combo("イージング", &selectedEasing, easingTypes, IM_ARRAYSIZE(easingTypes));

		ImGui::DragFloat("時間(秒)", &duration, 0.1f, 0.1f, 10.0f);
		ImGui::Checkbox("ループ", &loop);

		// タイプ別のパラメータ
		if (selectedAnimType >= 11) { // エフェクト系
			ImGui::DragFloat("強度", &intensity, 0.1f, 0.1f, 100.0f);
		}

		ImGui::Spacing();

		// 再生ボタン
		if (ImGui::Button("▶ アニメーション再生", ImVec2(-1, 30))) {
			Easing::Function easing = static_cast<Easing::Function>(selectedEasing);

			Vector3 currentPos = GetPosition();
			Vector2 currentScale = GetScale();
			Vector3 currentRot = GetRotation();
			Vector4 currentColor = GetColor();
			float currentAlpha = GetAlpha();

			switch (selectedAnimType) {
			case 0: // 位置
				PlayPositionAnimation(currentPos,
					Vector3{ currentPos.x + 100.0f, currentPos.y + 50.0f, currentPos.z },
					duration, easing, loop);
				break;

			case 1: // スケール
				PlayScaleAnimation(currentScale,
					Vector2{ currentScale.x * 1.5f, currentScale.y * 1.5f },
					duration, easing, loop);
				break;

			case 2: // 回転
				PlayRotationAnimation(currentRot,
					Vector3{ 0, 0, currentRot.z + 360.0f },
					duration, easing, loop);
				break;

			case 3: // アルファ
				PlayAlphaAnimation(currentAlpha, 0.0f, duration, easing, loop);
				break;

			case 4: // 色
				PlayColorAnimation(currentColor,
					Vector4{ 1.0f, 0.0f, 0.0f, 1.0f }, duration, easing, loop);
				break;

			case 5: // フェードイン
				PlayFadeIn(duration, easing);
				break;

			case 6: // フェードアウト
				PlayFadeOut(duration, easing);
				break;

			case 7: // スライドイン
				PlaySlideIn(SlideDirection::Right, 200.0f, duration);
				break;

			case 8: // スライドアウト
				PlaySlideOut(SlideDirection::Left, 200.0f, duration);
				break;

			case 9: // ズームイン
				PlayZoomIn(duration);
				break;

			case 10: // ズームアウト
				PlayZoomOut(duration);
				break;

			case 11: // シェイク
				PlayShake(intensity * 10.0f, duration);
				break;

			case 12: // パルス
				PlayPulse(1.0f + intensity * 0.2f, duration, loop);
				break;

			case 13: // バウンス
				PlayBounce(intensity * 50.0f, duration);
				break;

			case 14: // スイング
				PlaySwing(intensity * 15.0f, duration, loop);
				break;

			case 15: // フラッシュ
				PlayFlash(duration, static_cast<int>(intensity * 3));
				break;

			case 16: // ブリンク
				PlayBlink(duration, loop);
				break;

			case 17: // ウォブル
				PlayWobble(intensity * 20.0f, duration);
				break;

			case 18: // フリップ
				PlayFlip(true, duration);
				break;

			case 19: // 回転イン
				PlayRotateIn(duration);
				break;

			case 20: // 回転アウト
				PlayRotateOut(duration);
				break;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// プリセットボタン
		if (ImGui::TreeNode("🎬 プリセット")) {
			ImGui::Text("登場アニメーション:");

			if (ImGui::Button("フェードイン", ImVec2(120, 0))) {
				PlayFadeIn(0.8f, Easing::Function::EaseOutQuad);
			}
			ImGui::SameLine();
			if (ImGui::Button("ズームイン", ImVec2(120, 0))) {
				PlayZoomIn(0.6f);
			}
			ImGui::SameLine();
			if (ImGui::Button("回転イン", ImVec2(120, 0))) {
				PlayRotateIn(0.8f);
			}

			if (ImGui::Button("スライド←", ImVec2(120, 0))) {
				PlaySlideIn(SlideDirection::Right, 300.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("スライド→", ImVec2(120, 0))) {
				PlaySlideIn(SlideDirection::Left, 300.0f);
			}
			ImGui::SameLine();
			if (ImGui::Button("スライド↑", ImVec2(120, 0))) {
				PlaySlideIn(SlideDirection::Down, 300.0f);
			}

			ImGui::Spacing();
			ImGui::Text("退場アニメーション:");

			if (ImGui::Button("フェードアウト", ImVec2(120, 0))) {
				PlayFadeOut(0.8f, Easing::Function::EaseInQuad);
			}
			ImGui::SameLine();
			if (ImGui::Button("ズームアウト", ImVec2(120, 0))) {
				PlayZoomOut(0.6f);
			}
			ImGui::SameLine();
			if (ImGui::Button("回転アウト", ImVec2(120, 0))) {
				PlayRotateOut(0.8f);
			}

			ImGui::Spacing();
			ImGui::Text("エフェクト:");

			if (ImGui::Button("シェイク", ImVec2(120, 0))) {
				PlayShake(15.0f, 0.5f);
			}
			ImGui::SameLine();
			if (ImGui::Button("バウンス", ImVec2(120, 0))) {
				PlayBounce(80.0f, 1.2f);
			}
			ImGui::SameLine();
			if (ImGui::Button("フラッシュ", ImVec2(120, 0))) {
				PlayFlash(0.6f, 4);
			}

			if (ImGui::Button("パルス", ImVec2(120, 0))) {
				PlayPulse(1.3f, 0.6f, true);
			}
			ImGui::SameLine();
			if (ImGui::Button("スイング", ImVec2(120, 0))) {
				PlaySwing(20.0f, 0.8f, true);
			}
			ImGui::SameLine();
			if (ImGui::Button("ウォブル", ImVec2(120, 0))) {
				PlayWobble(25.0f, 1.0f);
			}

			ImGui::TreePop();
		}
	}
#endif
}

void UIBase::ImGuiPresetSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("プリセット")) {
		static char presetName[128] = "";
		ImGui::InputText("プリセット名", presetName, sizeof(presetName));

		if (ImGui::Button("現在の設定を保存")) {
			if (strlen(presetName) > 0) {
				if (SaveAsPreset(presetName)) {
					ImGui::OpenPopup("PresetSaved");
				}
			}
		}

		ImGui::SameLine();

		if (ImGui::Button("プリセットから読み込み")) {
			ImGui::OpenPopup("LoadPresetPopup");
		}

		if (ImGui::BeginPopupModal("PresetSaved", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text("プリセットを保存しました!");
			if (ImGui::Button("OK")) {
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("LoadPresetPopup")) {
			ImGui::Text("プリセットを選択:");
			ImGui::Separator();

			auto presets = GetAvailablePresets();
			for (const auto& preset : presets) {
				if (ImGui::Selectable(preset.c_str())) {
					LoadPreset(preset);
					ImGui::CloseCurrentPopup();
				}
			}

			if (presets.empty()) {
				ImGui::TextDisabled("プリセットがありません");
			}

			ImGui::EndPopup();
		}
	}
#endif
}

void UIBase::ImGuiQuickAlignment() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("クイック配置")) {
		ImGui::Text("画面位置:");

		if (ImGui::Button("左上")) {
			SetPosition({ 0.0f, 0.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央上")) {
			SetPosition({ 640.0f, 0.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右上")) {
			SetPosition({ 1280.0f, 0.0f, GetPosition().z });
		}

		if (ImGui::Button("左中央")) {
			SetPosition({ 0.0f, 360.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央")) {
			SetPosition({ 640.0f, 360.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右中央")) {
			SetPosition({ 1280.0f, 360.0f, GetPosition().z });
		}

		if (ImGui::Button("左下")) {
			SetPosition({ 0.0f, 720.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("中央下")) {
			SetPosition({ 640.0f, 720.0f, GetPosition().z });
		}
		ImGui::SameLine();
		if (ImGui::Button("右下")) {
			SetPosition({ 1280.0f, 720.0f, GetPosition().z });
		}
	}
#endif
}

void UIBase::ImGuiUVSRTSettings() {
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("UV SRT")) {
		// UV Translation
		Vector2 uvTrans = GetUVTranslation();
		if (ImGui::DragFloat2("UV Translation", &uvTrans.x, 0.01f, -10.0f, 10.0f)) {
			SetUVTranslation(uvTrans);
		}

		// UV Rotation
		float uvRot = GetUVRotation();
		if (ImGui::DragFloat("UV Rotation", &uvRot, 0.01f, -3.14159f * 2.0f, 3.14159f * 2.0f)) {
			SetUVRotation(uvRot);
		}

		// UV Scale
		Vector2 uvSc = GetUVScale();
		if (ImGui::DragFloat2("UV Scale", &uvSc.x, 0.01f, -10.0f, 10.0f)) {
			SetUVScale(uvSc);
		}

		ImGui::Separator();

		// リセットボタン
		if (ImGui::Button("UVリセット")) {
			SetUVTranslation({ 0.0f, 0.0f });
			SetUVRotation(0.0f);
			SetUVScale({ 1.0f, 1.0f });
		}

		ImGui::SameLine();

		// プリセットボタン
		if (ImGui::Button("UV反転X")) {
			SetUVScale({ -GetUVScale().x, GetUVScale().y });
		}

		ImGui::SameLine();

		if (ImGui::Button("UV反転Y")) {
			SetUVScale({ GetUVScale().x, -GetUVScale().y });
		}

		// UV Tiling プリセット
		ImGui::Text("UV Tiling:");
		if (ImGui::Button("1x1")) {
			SetUVScale({ 1.0f, 1.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("2x2")) {
			SetUVScale({ 2.0f, 2.0f });
		}
		ImGui::SameLine();
		if (ImGui::Button("4x4")) {
			SetUVScale({ 4.0f, 4.0f });
		}
	}
#endif
}

void UIBase::ImGUi() {
#ifdef USE_IMGUI
	if (!sprite_) return;


	bool modified = false;

	char nameBuffer[256];
	strncpy_s(nameBuffer, name_.c_str(), sizeof(nameBuffer) - 1);
	nameBuffer[sizeof(nameBuffer) - 1] = '\0';
	if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer))) {
		name_ = nameBuffer;
		modified = true;
	}

	if (ImGui::CollapsingHeader("トランスフォーム", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector2 scale = GetScale();
		if (ImGui::DragFloat2("拡大縮小", &scale.x, 0.5f)) {
			SetScale(scale);
			modified = true;
		}

		Vector3 rotation = GetRotation();
		if (ImGui::DragFloat3("回転", &rotation.x, 0.1f)) {
			SetRotation(rotation);
			modified = true;
		}

		Vector3 position = GetPosition();
		if (ImGui::DragFloat3("位置", &position.x, 1.0f)) {
			SetPosition(position);
			modified = true;
		}
	}

	if (ImGui::CollapsingHeader("マテリアル", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector4 color = GetColor();
		if (ImGui::ColorEdit4("色", &color.x)) {
			SetColor(color);
			modified = true;
		}

		bool flipX = GetFlipX();
		if (ImGui::Checkbox("X軸反転", &flipX)) {
			SetFlipX(flipX);
			modified = true;
		}

		ImGui::SameLine();

		bool flipY = GetFlipY();
		if (ImGui::Checkbox("Y軸反転", &flipY)) {
			SetFlipY(flipY);
			modified = true;
		}
	}

	if (ImGui::CollapsingHeader("テクスチャ", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("現在のテクスチャ: %s", texturePath_.c_str());

		// 🔍 フィルタ入力
		static char textureFilter[128] = "";
		ImGui::InputTextWithHint("##filter", "ファイル名で検索...", textureFilter, sizeof(textureFilter));

		if (ImGui::Button("テクスチャを変更")) {
			ImGui::OpenPopup("TextureSelectPopup");
		}

		if (ImGui::BeginPopup("TextureSelectPopup")) {
			ImGui::Text("📁 画像を選択:");
			ImGui::Separator();

			std::string baseDir = "./Resources/Textures/";
			std::function<void(const std::filesystem::path&)> DrawFolderTree;

			DrawFolderTree = [&](const std::filesystem::path& folder) {
				for (const auto& entry : std::filesystem::directory_iterator(folder)) {
					if (entry.is_directory()) {
						// フォルダ表示（アイコン付き）
						std::string folderName = "📂 " + entry.path().filename().string();
						if (ImGui::TreeNode(folderName.c_str())) {
							DrawFolderTree(entry.path());
							ImGui::TreePop();
						}
					} else if (entry.is_regular_file()) {
						auto ext = entry.path().extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(),
							[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

						if (ext == ".png" || ext == ".jpg" || ext == ".dds") {
							std::string filename = entry.path().filename().string();

							// 検索フィルタ適用
							if (strlen(textureFilter) > 0 && filename.find(textureFilter) == std::string::npos) {
								continue;
							}

							std::string displayName = filename;
							bool isCurrent = (texturePath_ == entry.path().string());

							// 現在のテクスチャを強調表示
							if (isCurrent) {
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
							}

							if (ImGui::Selectable(displayName.c_str(), isCurrent, ImGuiSelectableFlags_AllowDoubleClick)) {
								std::string fullPath = entry.path().string();
								SetTexture(fullPath);
								ImGui::CloseCurrentPopup();
							}

							if (isCurrent) {
								ImGui::SameLine();
								ImGui::TextDisabled("（使用中）");
								ImGui::PopStyleColor();
							}
						}
					}
				}
				};

			if (std::filesystem::exists(baseDir)) {
				DrawFolderTree(baseDir);
			} else {
				ImGui::TextDisabled("Resources/images/ が存在しません。");
			}

			ImGui::EndPopup();
		}

		Vector2 leftTop = sprite_->GetTextureLeftTop();
		if (ImGui::DragFloat2("左上座標", &leftTop.x, 1.0f)) {
			sprite_->SetTextureLeftTop(leftTop);
			modified = true;
		}

		Vector2 textureSize = sprite_->GetTextureSize();
		if (ImGui::DragFloat2("テクスチャサイズ", &textureSize.x, 1.0f)) {
			sprite_->SetTextureSize(textureSize);
			modified = true;
		}

		Vector2 anchor = sprite_->GetAnchorPoint();
		if (ImGui::DragFloat2("アンカーポイント", &anchor.x, 0.01f, 0.0f, 1.0f)) {
			sprite_->SetAnchorPoint(anchor);
			modified = true;
		}
	}

	ImGuiGridSettings();
	ImGuiAnimationSettings();
	ImGuiPresetSettings();
	ImGuiQuickAlignment();
	ImGuiUVSRTSettings();

	if (ImGui::CollapsingHeader("表示設定")) {
		ImGui::Checkbox("表示", &visible_);
		ImGui::DragInt("レイヤー", &layer_, 1.0f, 0, 100);
	}

	bool hotReload = hotReloadEnabled_;
	if (ImGui::Checkbox("ホットリロード", &hotReload)) {
		EnableHotReload(hotReload);
	}

	ImGui::Separator();

	if (ImGui::Button("変更を保存")) {
		if (SaveToJSON()) {
			ImGui::OpenPopup("SaveSuccessPopup");
		} else {
			ImGui::OpenPopup("SaveFailedPopup");
		}
	}

	if (ImGui::BeginPopupModal("SaveSuccessPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("設定が正常に保存されました。");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal("SaveFailedPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("設定の保存に失敗しました。");
		if (ImGui::Button("OK")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (modified) {
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "* 未保存の変更があります");
	}

#endif
}

void UIBase::EnableHotReload(bool enable) {
	hotReloadEnabled_ = enable;
}

void UIBase::CheckForChanges() {
	if (configPath_.empty() || !std::filesystem::exists(configPath_)) {
		return;
	}

	auto currentModTime = std::filesystem::last_write_time(configPath_);

	if (currentModTime != lastModTime_) {
		LoadFromJSON(configPath_);
		lastModTime_ = currentModTime;
	}
}

bool UIBase::LoadFromJSON(const std::string& jsonPath) {
	try {
		std::ifstream file(jsonPath);
		if (!file.is_open()) {
			return false;
		}

		nlohmann::json data;
		file >> data;
		file.close();

		ApplyJSONToState(data);

		return true;
	}
	catch (const std::exception& e) {
		printf("JSONからUIの読み込み中にエラー発生: %s\n", e.what());
		return false;
	}
}

bool UIBase::SaveToJSON(const std::string& jsonPath) {
	std::string savePath = jsonPath.empty() ? configPath_ : jsonPath;

	if (savePath.empty()) {
		return false;
	}

	try {
		std::filesystem::path dirPath = std::filesystem::path(savePath).parent_path();

		if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
			std::filesystem::create_directories(dirPath);
		}

		nlohmann::json data = CreateJSONFromCurrentState();

		std::ofstream file(savePath);
		if (!file.is_open()) {
			return false;
		}

		file << std::setw(4) << data << std::endl;
		file.close();

		return true;
	}
	catch (const std::exception& e) {
		printf("JSONへのUI保存中にエラー発生: %s\n", e.what());
		return false;
	}
}

void UIBase::SetPosition(const Vector3& position) {
	if (sprite_) {
		sprite_->SetTranslate(position);
	}
}

Vector3 UIBase::GetPosition() const {
	if (sprite_) {
		return sprite_->GetTranslate();
	}
	return { 0.0f, 0.0f, 0.0f };
}

void UIBase::SetRotation(const Vector3& rotation) {
	if (sprite_) {
		sprite_->SetRotate(rotation);
	}
}

Vector3 UIBase::GetRotation() const {
	if (sprite_) {
		return sprite_->GetRotate();
	}
	return { 0.0f, 0.0f, 0.0f };
}

void UIBase::SetScale(const Vector2& scale) {
	if (sprite_) {
		sprite_->SetSize(scale);
	}
}

Vector2 UIBase::GetScale() const {
	if (sprite_) {
		return sprite_->GetSize();
	}
	return { 1.0f, 1.0f };
}

void UIBase::SetColor(const Vector4& color) {
	if (sprite_) {
		sprite_->SetColor(color);
	}
}

Vector4 UIBase::GetColor() const {
	if (sprite_) {
		return sprite_->GetColor();
	}
	return { 1.0f, 1.0f, 1.0f, 1.0f };
}

void UIBase::SetAlpha(float alpha) {
	if (sprite_) {
		sprite_->SetAlpha(alpha);
	}
}

float UIBase::GetAlpha() const {
	if (sprite_) {
		return sprite_->GetColor().w;
	}
	return 1.0f;
}

void UIBase::SetTexture(const std::string& texturePath) {
	if (sprite_) {
		sprite_->ChangeTexture(texturePath);
		texturePath_ = texturePath;
	}
}

std::string UIBase::GetTexturePath() const {
	return texturePath_;
}

void UIBase::SetCamera(Camera* camera) {
	if (sprite_) {
		sprite_->SetCamera(camera);
	}
}

void UIBase::SetName(const std::string& name) {
	name_ = name;
}

std::string UIBase::GetName() const {
	return name_;
}

void UIBase::SetFlipX(bool flipX) {
	if (sprite_) {
		sprite_->SetIsFlipX(flipX);
	}
}

void UIBase::SetFlipY(bool flipY) {
	if (sprite_) {
		sprite_->SetIsFlipY(flipY);
	}
}

bool UIBase::GetFlipX() const {
	if (sprite_) {
		return sprite_->GetIsFlipX();
	}
	return false;
}

bool UIBase::GetFlipY() const {
	if (sprite_) {
		return sprite_->GetIsFlipY();
	}
	return false;
}

void UIBase::SetTextureLeftTop(const Vector2& leftTop) {
	if (sprite_) {
		sprite_->SetTextureLeftTop(leftTop);
	}
}

Vector2 UIBase::GetTextureLeftTop() const {
	if (sprite_) {
		return sprite_->GetTextureLeftTop();
	}
	return { 0.0f, 0.0f };
}

void UIBase::SetTextureSize(const Vector2& size) {
	if (sprite_) {
		sprite_->SetTextureSize(size);
	}
}

Vector2 UIBase::GetTextureSize() const {
	if (sprite_) {
		return sprite_->GetTextureSize();
	}
	return { 1.0f, 1.0f };
}

void UIBase::SetAnchorPoint(const Vector2& anchor) {
	if (sprite_) {
		sprite_->SetAnchorPoint(anchor);
	}
}

Vector2 UIBase::GetAnchorPoint() const {
	if (sprite_) {
		return sprite_->GetAnchorPoint();
	}
	return { 0.0f, 0.0f };
}

/*==================================================================
						UV SRT制御
===================================================================*/

void UIBase::SetUVTranslation(const Vector2& translation) {
	uvTranslation_ = translation;
	if (sprite_) {
		sprite_->SetUVTranslation(translation);
	}
}

Vector2 UIBase::GetUVTranslation() const {
	return uvTranslation_;
}

void UIBase::SetUVRotation(float rotation) {
	uvRotation_ = rotation;
	if (sprite_) {
		sprite_->SetUVRotation(rotation);
	}
}

float UIBase::GetUVRotation() const {
	return uvRotation_;
}

void UIBase::SetUVScale(const Vector2& scale) {
	uvScale_ = scale;
	if (sprite_) {
		sprite_->SetUVScale(scale);
	}
}

Vector2 UIBase::GetUVScale() const {
	return uvScale_;
}

nlohmann::json UIBase::CreateJSONFromCurrentState() {
	nlohmann::json data;

	data["name"] = name_;
	data["texturePath"] = texturePath_;

	data["position"] = {
		{"x", GetPosition().x},
		{"y", GetPosition().y},
		{"z", GetPosition().z}
	};

	data["rotation"] = {
		{"x", GetRotation().x},
		{"y", GetRotation().y},
		{"z", GetRotation().z}
	};

	data["scale"] = {
		{"x", GetScale().x},
		{"y", GetScale().y}
	};

	data["color"] = {
		{"r", GetColor().x},
		{"g", GetColor().y},
		{"b", GetColor().z},
		{"a", GetColor().w}
	};

	data["flipX"] = GetFlipX();
	data["flipY"] = GetFlipY();

	if (sprite_) {
		data["textureLeftTop"] = {
			{"x", sprite_->GetTextureLeftTop().x},
			{"y", sprite_->GetTextureLeftTop().y}
		};

		data["anchorPoint"] = {
			{"x", sprite_->GetAnchorPoint().x},
			{"y", sprite_->GetAnchorPoint().y}
		};

		data["textureSize"] = {
			{"x", sprite_->GetTextureSize().x},
			{"y", sprite_->GetTextureSize().y}
		};
	}

	data["visible"] = visible_;
	data["layer"] = layer_;

	// UV SRT
	data["uvTranslation"] = {
		{"x", uvTranslation_.x},
		{"y", uvTranslation_.y}
	};
	data["uvRotation"] = uvRotation_;
	data["uvScale"] = {
		{"x", uvScale_.x},
		{"y", uvScale_.y}
	};

	return data;
}

void UIBase::ApplyJSONToState(const nlohmann::json& data) {
	if (data.contains("texturePath")) {
		texturePath_ = data["texturePath"];

		if (!sprite_) {
			sprite_ = std::make_unique<Sprite>();
			sprite_->Initialize(texturePath_);
		} else {
			sprite_->Initialize(texturePath_);
		}
	} else if (!sprite_) {
		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize("./Resources/images/white.png");
		texturePath_ = "./Resources/images/white.png";
	}

	if (data.contains("name")) {
		name_ = data["name"];
	}

	if (data.contains("position")) {
		Vector3 position;
		position.x = data["position"]["x"];
		position.y = data["position"]["y"];
		position.z = data["position"]["z"];
		SetPosition(position);
	}

	if (data.contains("rotation")) {
		Vector3 rotation;
		rotation.x = data["rotation"]["x"];
		rotation.y = data["rotation"]["y"];
		rotation.z = data["rotation"]["z"];
		SetRotation(rotation);
	}

	if (data.contains("scale")) {
		Vector2 scale;
		scale.x = data["scale"]["x"];
		scale.y = data["scale"]["y"];
		SetScale(scale);
	}

	if (data.contains("color")) {
		Vector4 color;
		color.x = data["color"]["r"];
		color.y = data["color"]["g"];
		color.z = data["color"]["b"];
		color.w = data["color"]["a"];
		SetColor(color);
	}

	if (data.contains("flipX")) {
		SetFlipX(data["flipX"]);
	}

	if (data.contains("flipY")) {
		SetFlipY(data["flipY"]);
	}

	if (sprite_) {
		if (data.contains("textureLeftTop")) {
			Vector2 leftTop;
			leftTop.x = data["textureLeftTop"]["x"];
			leftTop.y = data["textureLeftTop"]["y"];
			sprite_->SetTextureLeftTop(leftTop);
		}

		if (data.contains("anchorPoint")) {
			Vector2 anchor;
			anchor.x = data["anchorPoint"]["x"];
			anchor.y = data["anchorPoint"]["y"];
			sprite_->SetAnchorPoint(anchor);
		}

		if (data.contains("textureSize")) {
			Vector2 size;
			size.x = data["textureSize"]["x"];
			size.y = data["textureSize"]["y"];
			sprite_->SetTextureSize(size);
		}
	}

	if (data.contains("visible")) {
		visible_ = data["visible"];
	}

	if (data.contains("layer")) {
		layer_ = data["layer"];
	}

	// UV SRT
	if (data.contains("uvTranslation")) {
		Vector2 uvTranslation;
		uvTranslation.x = data["uvTranslation"]["x"];
		uvTranslation.y = data["uvTranslation"]["y"];
		SetUVTranslation(uvTranslation);
	}

	if (data.contains("uvRotation")) {
		SetUVRotation(data["uvRotation"]);
	}

	if (data.contains("uvScale")) {
		Vector2 uvScale;
		uvScale.x = data["uvScale"]["x"];
		uvScale.y = data["uvScale"]["y"];
		SetUVScale(uvScale);
	}
}