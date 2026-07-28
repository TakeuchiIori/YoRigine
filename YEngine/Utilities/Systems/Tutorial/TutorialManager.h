#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "Vector2.h"
#include "Vector4.h"

#include "TutorialCondition.h"
#include "TutorialSignal.h"
#include "TutorialSpotlight.h"

namespace YoRigine {

	enum class TutorialWaitType {
		Confirm,
		Seconds,
		Event,
	};

	// 各説明ページへ追加表示する画像UI。
	// 操作図、キー画像、ゲーム画面の注目箇所などを本文と一緒に表示できる。
	struct TutorialStepUI {
		std::string name = "補足画像";
		std::string texturePath;
		Vector2 position{ 640.0f, 360.0f };
		Vector2 size{ 320.0f, 180.0f };
		Vector2 anchorPoint{ 0.5f, 0.5f };
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		int layerOffset = 4;
	};

	// 説明ページごとのレイアウト。
	// ページによって本文量や補足画像の位置が異なるため、共通Styleとは分離して保存する。
	struct TutorialStepLayout {
		Vector2 panelPosition{ 640.0f, 570.0f };
		Vector2 panelSize{ 1120.0f, 240.0f };
		Vector2 textOffset{ 0.0f, -35.0f };
		float textMaxWidth = 1020.0f;
		Vector2 hintOffset{ 0.0f, 85.0f };
		Vector2 hintPanelSize{ 540.0f, 54.0f };
	};

	struct TutorialStep {
		std::string name = "新しいステップ";
		std::string speaker;
		std::string text = "説明文を入力してください";
		// 完了条件。type が None のときだけ、従来の waitType へフォールバックする。
		// 既存の JSON には complete が無いため、そのまま読み込めば従来動作になる。
		TutorialCondition complete;
		// 注目させたい場所以外を暗幕で覆う設定。enabled が false なら何もしない。
		TutorialSpotlightConfig spotlight;
		TutorialWaitType waitType = TutorialWaitType::Confirm;
		float waitSeconds = 2.0f;
		std::string eventName;
		std::string targetUIId;
		bool pauseGameplay = true;
		bool skippable = true;
		TutorialStepLayout layout;
		std::vector<TutorialStepUI> additionalUIs;
	};

	struct TutorialStyle {
		std::string fontFamily = "Meiryo";
		std::string fontFilePath;
		float fontSize = 32.0f;
		Vector4 textColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		float outlineWidth = 2.0f;
		Vector4 outlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
		Vector4 panelColor{ 0.03f, 0.04f, 0.07f, 0.90f };
		std::string panelTexturePath;
		float textPadding = 8.0f;
		int textAlign = 0;
		bool textShadow = false;
		Vector2 shadowOffset{ 3.0f, 3.0f };
		Vector4 shadowColor{ 0.0f, 0.0f, 0.0f, 0.65f };
		bool showControlHint = true;
		std::string hintText = "[SPACE / A] 次へ";
		std::string skipHintText = "[ESC / B] スキップ";
		std::string hintPanelTexturePath;
		Vector4 hintPanelColor{ 0.10f, 0.24f, 0.38f, 0.95f };
		float hintFontSize = 22.0f;
		Vector4 hintTextColor{ 1.0f, 0.95f, 0.72f, 1.0f };
		float hintOutlineWidth = 1.0f;
		Vector4 hintOutlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
		float hintPadding = 6.0f;
		// ページ切り替え演出
		float fadeInSeconds = 0.25f;
		float fadeOutSeconds = 0.2f;
		// 操作案内。キー名はエディターの候補から選択する。
		std::string confirmKeyboardKey = "SPACE";
		std::string confirmGamepadButton = "A";
		std::string skipKeyboardKey = "ESC";
		std::string skipGamepadButton = "B";
		bool autoBuildControlHint = true;
		int layer = 1000;
	};

	struct TutorialData {
		std::string name = "NewTutorial";
		TutorialStyle style;
		std::vector<TutorialStep> steps;
	};

	class TutorialManager {
	public:
		static TutorialManager* GetInstance();

		bool Save(const TutorialData& data, const std::string& path) const;
		bool Load(TutorialData& data, const std::string& path);
		bool LoadAndStart(const std::string& path, std::size_t startStep = 0);

		void Start(const TutorialData& data, std::size_t startStep = 0);
		void Stop();
		void Update();
		void NotifyEvent(const std::string& eventName);
		void RegisterEventName(const std::string& eventName);
		const std::vector<std::string>& GetKnownEventNames() const { return knownEventNames_; }
		void Advance();

		bool IsPlaying() const { return playing_; }
		std::size_t GetCurrentStepIndex() const { return currentStep_; }
		const TutorialData& GetCurrentData() const { return currentData_; }

#ifdef USE_IMGUI
		void DrawEditor();
#endif

	private:
		TutorialManager() = default;
		void EnterStep(std::size_t index);
		void SubscribeSignals();
		void UnsubscribeSignals();
		void RefreshRuntimeUI();
		void HideRuntimeUI();
		void ApplyRuntimeOpacity();
		void CompleteAdvance();
		void ApplyTargetHighlight();
		void ClearTargetHighlight();
		void ApplyGameplayPause(bool pause);
		bool IsConfirmTriggered() const;
		bool IsSkipTriggered() const;
		std::string BuildConfirmHintText() const;
		std::string BuildSkipHintText() const;

		enum class TransitionPhase {
			FadeIn,
			Showing,
			FadeOut,
		};

		TutorialData currentData_;
		std::size_t currentStep_ = 0;
		float stepElapsed_ = 0.0f;
		bool playing_ = false;
		bool runtimeUIDirty_ = false;
		TransitionPhase transitionPhase_ = TransitionPhase::Showing;
		float transitionElapsed_ = 0.0f;
		float transitionOpacity_ = 1.0f;
		float transitionStartOpacity_ = 1.0f;
		std::size_t activeAdditionalUICount_ = 0;
		bool gameplayPauseOwned_ = false;
		bool gameplayWasPaused_ = false;
		std::unordered_set<std::string> receivedEvents_;
		std::vector<std::string> knownEventNames_;

		// 完了条件の評価状態。currentData_ 内の定義を指すため、
		// currentData_ が差し替わるタイミング（Start / Stop）で必ず張り直す。
		TutorialConditionRuntime completeRuntime_;
		TutorialSignal::Handle signalHandle_ = 0;
		std::string highlightedUIId_;
		Vector2 highlightedOriginalScale_{ 1.0f, 1.0f };

#ifdef USE_IMGUI
		TutorialData editorData_;
		std::string editorPath_ = "Resources/Json/Tutorials/GameStartTutorial.json";
		int editorSelectedStep_ = 0;
		std::string editorStatus_;
		bool editorLivePreview_ = true;
		bool editorPreviewPending_ = false;
		double editorPreviewChangeTime_ = 0.0;
#endif
	};

} // namespace YoRigine
