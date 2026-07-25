#pragma once
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <filesystem>
#include "Sprite/Sprite.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "json.hpp"
#include "UIAnimation.h"
#include "UIAnimator.h"
#include "Loaders/Json/Use/AutoJson.h"

// 前方宣言
class SpriteCommon;
namespace YoRigine { class Camera; }

///************************* UI基本クラス *************************///
class UIBase {
public:
	///************************* 基本関数 *************************///

	UIBase(const std::string& name = "UIBase");
	virtual ~UIBase();

	void Initialize(const std::string& jsonConfigPath);
	virtual void Update();
	virtual void Draw();
	void ImGUi();

private:
	void EnableHotReload(bool enable);
	void CheckForChanges();
	bool LoadFromJSON(const std::string& jsonPath);

public:
	///************************* 基本アクセッサ *************************///

	bool SaveToJSON(const std::string& jsonPath = "");

	void SetPosition(const Vector3& position);
	Vector3 GetPosition() const;

	void SetRotation(const Vector3& rotation);
	Vector3 GetRotation() const;

	// 基準表示サイズ(px)。レイアウト用。
	void SetSize(const Vector2& size);
	Vector2 GetSize() const;

	// 拡縮倍率(1.0=等倍)。アニメーション/演出用。表示サイズ = Size(px) × Scale。
	void SetScale(const Vector2& scale);
	Vector2 GetScale() const;

	void SetColor(const Vector4& color);
	Vector4 GetColor() const;

	void SetAlpha(float alpha);
	float GetAlpha() const;

	void SetTexture(const std::string& texturePath);
	std::string GetTexturePath() const;

	void SetCamera(YoRigine::Camera* camera);
	void SetName(const std::string& name);
	std::string GetName() const;

	void SetFlipX(bool flipX);
	void SetFlipY(bool flipY);
	bool GetFlipX() const;
	bool GetFlipY() const;

	YoRigine::Sprite* GetSprite() { return sprite_.get(); }

	void SetTextureLeftTop(const Vector2& leftTop);
	Vector2 GetTextureLeftTop() const;

	void SetTextureSize(const Vector2& size);
	Vector2 GetTextureSize() const;

	void SetAnchorPoint(const Vector2& anchor);
	Vector2 GetAnchorPoint() const;

	///************************* UV SRT制御 *************************///

	void SetUVTranslation(const Vector2& translation);
	Vector2 GetUVTranslation() const;

	void SetUVRotation(float rotation);
	float GetUVRotation() const;

	void SetUVScale(const Vector2& scale);
	Vector2 GetUVScale() const;

	///************************* グリッド・スナップ機能 *************************///

	void SetGridEnabled(bool enabled) { gridEnabled_ = enabled; }
	bool IsGridEnabled() const { return gridEnabled_; }

	void SetGridSize(float size) { gridSize_ = size; }
	float GetGridSize() const { return gridSize_; }

	Vector3 SnapToGrid(const Vector3& position) const;

	///************************* 表示制御 *************************///

	void SetVisible(bool visible) { visible_ = visible; }
	bool IsVisible() const { return visible_; }

	void SetLayer(int layer) { layer_ = layer; }
	int GetLayer() const { return layer_; }

	// 実行中だけ生成する通知・チュートリアル等。シーンJSONへ保存しない。
	void SetTransient(bool transient) { transient_ = transient; }
	bool IsTransient() const { return transient_; }

	///************************* 基本アニメーション制御 *************************///

	// シンプルなアニメーション
	void PlayPositionAnimation(const Vector3& from, const Vector3& to, float duration,
		Easing::Function easing = Easing::Function::Linear, bool loop = false);
	void PlayScaleAnimation(const Vector2& from, const Vector2& to, float duration,
		Easing::Function easing = Easing::Function::Linear, bool loop = false);
	void PlayRotationAnimation(const Vector3& from, const Vector3& to, float duration,
		Easing::Function easing = Easing::Function::Linear, bool loop = false);
	void PlayAlphaAnimation(float from, float to, float duration,
		Easing::Function easing = Easing::Function::Linear, bool loop = false);
	void PlayColorAnimation(const Vector4& from, const Vector4& to, float duration,
		Easing::Function easing = Easing::Function::Linear, bool loop = false);

	// プリセットアニメーション
	void PlayAnimation(const UIAnimation& anim);
	void PlayFadeIn(float duration = 1.0f, Easing::Function easing = Easing::Function::EaseOutQuad);
	void PlayFadeOut(float duration = 1.0f, Easing::Function easing = Easing::Function::EaseInQuad);
	void PlaySlideIn(SlideDirection dir, float distance, float duration = 0.5f);
	void PlaySlideOut(SlideDirection dir, float distance, float duration = 0.5f);
	void PlayZoomIn(float duration = 0.5f);
	void PlayZoomOut(float duration = 0.5f);
	void PlayShake(float intensity = 10.0f, float duration = 0.5f);
	void PlayPulse(float scale = 1.2f, float duration = 0.5f, bool loop = true);
	void PlayBounce(float height = 50.0f, float duration = 1.0f);
	void PlaySwing(float angle = 15.0f, float duration = 0.5f, bool loop = true);
	void PlayFlash(float duration = 0.5f, int times = 3);
	void PlayBlink(float duration = 2.0f, bool loop = true);
	void PlayWobble(float intensity = 20.0f, float duration = 1.0f);
	void PlayFlip(bool horizontal = true, float duration = 0.6f);
	void PlayRotateIn(float duration = 0.8f);
	void PlayRotateOut(float duration = 0.8f);

	// アニメーション制御
	void StopAnimation(UIAnimationType type = UIAnimationType::None);  // None指定で全停止
	void StopAllAnimations();  // 全アニメーション停止
	void PauseAnimation();
	void ResumeAnimation();
	bool IsAnimating() const { return animator_.IsAnimating(); }
	bool IsPaused() const { return animator_.IsPaused(); }

	// アニメーションエンジンへの直接アクセス（インスペクタ等が中身を参照する用途）
	UIAnimator& GetAnimator() { return animator_; }
	const UIAnimator& GetAnimator() const { return animator_; }

	///************************* アニメーションクリップ（データ駆動） *************************///

	// 保存済みクリップを1つ再生
	void PlayClip(const UIAnimationClip& clip);
	// 名前でクリップを再生（見つからなければ false を返す）
	bool PlayAnimation(const std::string& clipName);
	// 名前でクリップを停止
	void StopClip(const std::string& clipName);
	// 名前でクリップが再生中か確認
	bool IsClipPlaying(const std::string& clipName) const;
	// 指定トリガを持つクリップをまとめて再生（OnInit / OnShow の自動再生に使用）
	void PlayClipsByTrigger(UIAnimTrigger trigger);

	// エディタ等がクリップ一覧を編集する用途
	std::vector<UIAnimationClip>& GetClips() { return clips_; }
	const std::vector<UIAnimationClip>& GetClips() const { return clips_; }

	// コールバック設定（最後に追加されたアニメーションに設定）
	void SetAnimationCompleteCallback(std::function<void()> callback);
	void SetAnimationUpdateCallback(std::function<void()> callback);

	// 遅延設定（最後に追加されたアニメーションに設定）
	void SetAnimationDelay(float delay) { animator_.SetDelay(delay); }
	///************************* プリセット機能 *************************///

	bool SaveAsPreset(const std::string& presetName);
	bool LoadPreset(const std::string& presetName);
	std::vector<std::string> GetAvailablePresets() const;

	///************************* 複製機能 *************************///

	void CopyPropertiesFrom(const UIBase* other);

	static const std::string PRESET_DIRECTORY;

protected:
	///************************* メンバ変数 *************************///

	std::unique_ptr<YoRigine::Sprite> sprite_;
	YoRigine::Camera* camera_ = nullptr;
	std::string configPath_;
	std::filesystem::file_time_type lastModTime_;
	std::string name_;
	std::string texturePath_;
	bool hotReloadEnabled_;

	bool visible_ = true;
	int layer_ = 0;
	bool transient_ = false;

	bool gridEnabled_ = false;
	float gridSize_ = 10.0f;

	// ============================================================
	// 実描画値は sprite_ が保持するため、保存直前に sprite_→ここへ、
	// 読み込み直後にここ→sprite_ へ同期する（SyncSpriteToData / ApplyDataToSprite）。
	// ============================================================
	Vector3 position_ = { 0.0f, 0.0f, 0.0f };
	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
	Vector2 size_ = { 100.0f, 100.0f };   // 基準サイズ(px)
	Vector2 scale_ = { 1.0f, 1.0f };      // 拡縮倍率(1.0=等倍)
	Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	bool flipX_ = false;
	bool flipY_ = false;
	Vector2 textureLeftTop_ = { 0.0f, 0.0f };
	Vector2 anchorPoint_ = { 0.0f, 0.0f };
	Vector2 textureSize_ = { 100.0f, 100.0f };

	// Json読み込み、保存
	AutoJson aj_;

	// アニメーション管理（再生ロジックは UIAnimator に分離）
	UIAnimator animator_;

	// データ駆動アニメーションクリップ
	std::vector<UIAnimationClip> clips_;
	bool prevVisible_ = true;

	// UV SRT
	Vector2 uvTranslation_ = { 0.0f, 0.0f };
	float uvRotation_ = 0.0f;
	Vector2 uvScale_ = { 1.0f, 1.0f };

	///************************* 内部関数 *************************///

	nlohmann::json CreateJSONFromCurrentState();
	void ApplyJSONToState(const nlohmann::json& json);

	// AutoJson への変数登録
	void SetupJsonBindings();
	// sprite_ の現在値 → 永続化用メンバへ（保存直前）
	void SyncSpriteToData();
	// 永続化用メンバ → sprite_ へ反映（読み込み直後）
	void ApplyDataToSprite();

	///************************* ImGui関連 *************************///

	void ImGuiGridSettings();
	void ImGuiAnimationSettings();
	void ImGuiPresetSettings();
	void ImGuiQuickAlignment();
	void ImGuiUVSRTSettings();
};
