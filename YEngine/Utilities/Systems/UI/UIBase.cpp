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
	SetupJsonBindings();
}

UIBase::~UIBase() {

}

void UIBase::Initialize(const std::string& jsonConfigPath) {
	configPath_ = jsonConfigPath;

	// アニメーションエンジンに自分を接続（Play 系 API はこの animator_ へ転送される）
	animator_.SetTarget(this);

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

	// 初期可視状態を記録し、OnInit トリガのクリップを自動再生する。
	prevVisible_ = visible_;
	PlayClipsByTrigger(UIAnimTrigger::OnInit);
}

void UIBase::Update() {
	// アニメーションの進行はエンジンに委譲
	animator_.Update(YoRigine::GameTime::GetDeltaTime());

	// 非表示→表示に切り替わったフレームで OnShow トリガを再生
	if (!prevVisible_ && visible_) {
		PlayClipsByTrigger(UIAnimTrigger::OnShow);
	}
	prevVisible_ = visible_;

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

// シンプルなアニメーション（実処理は UIAnimator に委譲）
void UIBase::PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration,
	Easing::Function easing, bool loop) {
	animator_.PlayPositionAnimation(from, to, duration, easing, loop);
}

void UIBase::PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration,
	Easing::Function easing, bool loop) {
	animator_.PlayScaleAnimation(from, to, duration, easing, loop);
}

void UIBase::PlayRotationAnimation(const Vector3& from, const Vector3& to, float duration,
	Easing::Function easing, bool loop) {
	animator_.PlayRotationAnimation(from, to, duration, easing, loop);
}

void UIBase::PlayAlphaAnimation(float from, float to, float duration,
	Easing::Function easing, bool loop) {
	animator_.PlayAlphaAnimation(from, to, duration, easing, loop);
}

void UIBase::PlayColorAnimation(const Vector4& from, const Vector4& to, float duration,
	Easing::Function easing, bool loop) {
	animator_.PlayColorAnimation(from, to, duration, easing, loop);
}

// プリセットアニメーション（実処理は UIAnimator に委譲）
void UIBase::PlayAnimation(const UIAnimation& anim) {
	animator_.PlayAnimation(anim);
}

void UIBase::PlayFadeIn(float duration, Easing::Function easing) {
	animator_.PlayFadeIn(duration, easing);
}

void UIBase::PlayFadeOut(float duration, Easing::Function easing) {
	animator_.PlayFadeOut(duration, easing);
}

void UIBase::PlaySlideIn(SlideDirection dir, float distance, float duration) {
	animator_.PlaySlideIn(dir, distance, duration);
}

void UIBase::PlaySlideOut(SlideDirection dir, float distance, float duration) {
	animator_.PlaySlideOut(dir, distance, duration);
}

void UIBase::PlayZoomIn(float duration) {
	animator_.PlayZoomIn(duration);
}

void UIBase::PlayZoomOut(float duration) {
	animator_.PlayZoomOut(duration);
}

void UIBase::PlayShake(float intensity, float duration) {
	animator_.PlayShake(intensity, duration);
}

void UIBase::PlayPulse(float scale, float duration, bool loop) {
	animator_.PlayPulse(scale, duration, loop);
}

void UIBase::PlayBounce(float height, float duration) {
	animator_.PlayBounce(height, duration);
}

void UIBase::PlaySwing(float angle, float duration, bool loop) {
	animator_.PlaySwing(angle, duration, loop);
}

void UIBase::PlayFlash(float duration, int times) {
	animator_.PlayFlash(duration, times);
}

void UIBase::PlayBlink(float duration, bool loop) {
	animator_.PlayBlink(duration, loop);
}

void UIBase::PlayWobble(float intensity, float duration) {
	animator_.PlayWobble(intensity, duration);
}

void UIBase::PlayFlip(bool horizontal, float duration) {
	animator_.PlayFlip(horizontal, duration);
}

void UIBase::PlayRotateIn(float duration) {
	animator_.PlayRotateIn(duration);
}

void UIBase::PlayRotateOut(float duration) {
	animator_.PlayRotateOut(duration);
}

// アニメーション制御（実処理は UIAnimator に委譲）
void UIBase::StopAnimation(UIAnimationType type) {
	animator_.StopAnimation(type);
}

void UIBase::StopAllAnimations() {
	animator_.StopAllAnimations();
}

void UIBase::PauseAnimation() {
	animator_.PauseAnimation();
}

void UIBase::ResumeAnimation() {
	animator_.ResumeAnimation();
}

void UIBase::SetAnimationCompleteCallback(std::function<void()> callback) {
	animator_.SetCompleteCallback(callback);
}

void UIBase::SetAnimationUpdateCallback(std::function<void()> callback) {
	animator_.SetUpdateCallback(callback);
}

// データ駆動クリップの再生（実処理は UIAnimator に委譲）
void UIBase::PlayClip(const UIAnimationClip& clip) {
	animator_.PlayClip(clip);
}

void UIBase::PlayClipsByTrigger(UIAnimTrigger trigger) {
	for (const auto& clip : clips_) {
		if (clip.trigger == trigger) {
			animator_.PlayClip(clip);
		}
	}
}

// アニメーションの更新処理は UIAnimator へ移動した（UIBase::Update が animator_.Update を呼ぶ）。

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

// ============================================================
// ImGui パネル群（ImGuiGridSettings / ImGuiAnimationSettings /
// ImGuiPresetSettings / ImGuiQuickAlignment / ImGuiUVSRTSettings / ImGUi）は
// UIBaseEditor.cpp に分離した。
// ============================================================

// （ImGuiPresetSettings / ImGuiQuickAlignment / ImGuiUVSRTSettings / ImGUi の
//  実装は UIBaseEditor.cpp に移動した）

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

// 基準表示サイズ(px)。レイアウト用。
void UIBase::SetSize(const Vector2& size) {
	if (sprite_) {
		sprite_->SetSize(size);
	}
}

Vector2 UIBase::GetSize() const {
	if (sprite_) {
		return sprite_->GetSize();
	}
	return { 100.0f, 100.0f };
}

// 拡縮倍率(1.0=等倍)。アニメーション/演出用。表示サイズ = Size(px) × Scale。
void UIBase::SetScale(const Vector2& scale) {
	if (sprite_) {
		sprite_->SetScale(scale);
	}
}

Vector2 UIBase::GetScale() const {
	if (sprite_) {
		return sprite_->GetScale();
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

// ============================================================
// AutoJson への変数登録（コンストラクタで一度だけ）
// ============================================================
// 登録した変数は aj_.Save / aj_.Load で一括して読み書きされる。
// color は既存JSONが r/g/b/a 形式（Vector4 既定の x/y/z/w とは別）なので
// 後方互換のため AutoJson には載せず CreateJSON/ApplyJSON で個別に処理する。
void UIBase::SetupJsonBindings() {
	aj_.Add("name", &name_)
		.Add("texturePath", &texturePath_)
		.Add("position", &position_)
		.Add("rotation", &rotation_)
		.Add("size", &size_)
		.Add("scale", &scale_)
		.Add("flipX", &flipX_)
		.Add("flipY", &flipY_)
		.Add("textureLeftTop", &textureLeftTop_)
		.Add("anchorPoint", &anchorPoint_)
		.Add("textureSize", &textureSize_)
		.Add("visible", &visible_)
		.Add("layer", &layer_)
		.Add("uvTranslation", &uvTranslation_)
		.Add("uvRotation", &uvRotation_)
		.Add("uvScale", &uvScale_)
		.Add("animations", &clips_);
}

// ============================================================
// sprite_ の現在値 → 永続化用メンバへ（保存直前）
// ============================================================
void UIBase::SyncSpriteToData() {
	position_ = GetPosition();
	rotation_ = GetRotation();
	size_ = GetSize();
	scale_ = GetScale();
	color_ = GetColor();
	flipX_ = GetFlipX();
	flipY_ = GetFlipY();
	textureLeftTop_ = GetTextureLeftTop();
	anchorPoint_ = GetAnchorPoint();
	textureSize_ = GetTextureSize();
	// uvTranslation_/uvRotation_/uvScale_ は Setter でメンバ側も常に同期済み。
	// name_/visible_/layer_/clips_ はメンバが直接ソース。
}

// ============================================================
// 永続化用メンバ → sprite_ へ反映（読み込み直後）
// ============================================================
void UIBase::ApplyDataToSprite() {
	if (texturePath_.empty()) {
		texturePath_ = "./Resources/images/white.png";
	}
	if (!sprite_) {
		sprite_ = std::make_unique<Sprite>();
	}
	sprite_->Initialize(texturePath_);

	SetPosition(position_);
	SetRotation(rotation_);
	SetSize(size_);
	SetScale(scale_);
	SetColor(color_);
	SetFlipX(flipX_);
	SetFlipY(flipY_);
	sprite_->SetTextureLeftTop(textureLeftTop_);
	sprite_->SetAnchorPoint(anchorPoint_);
	sprite_->SetTextureSize(textureSize_);
	SetUVTranslation(uvTranslation_);
	SetUVRotation(uvRotation_);
	SetUVScale(uvScale_);
}

nlohmann::json UIBase::CreateJSONFromCurrentState() {
	// sprite_ が保持する実描画値をメンバへ吸い上げてから一括保存
	SyncSpriteToData();

	nlohmann::json data;
	aj_.Save(data);

	// 色は後方互換のため r/g/b/a で保存する（Vector4 既定の x/y/z/w にしない）
	data["color"] = {
		{"r", color_.x},
		{"g", color_.y},
		{"b", color_.z},
		{"a", color_.w}
	};

	return data;
}

void UIBase::ApplyJSONToState(const nlohmann::json& data) {
	// 登録済みフィールド（color 以外）を一括ロード。未知キーは無視される。
	aj_.Load(data);

	// 色（r/g/b/a 後方互換）
	if (data.contains("color")) {
		const auto& c = data["color"];
		color_.x = c.value("r", 1.0f);
		color_.y = c.value("g", 1.0f);
		color_.z = c.value("b", 1.0f);
		color_.w = c.value("a", 1.0f);
	}

	// 旧フォーマット: "size" が無く "scale" だけある場合、その "scale" は
	// 実体としてピクセルサイズ。基準サイズ(px)へ読み替え、倍率は等倍に戻す。
	if (!data.contains("size") && data.contains("scale")) {
		size_ = scale_;
		scale_ = { 1.0f, 1.0f };
	}

	// メンバ → sprite_ へ反映
	ApplyDataToSprite();
}