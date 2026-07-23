#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "Vector2.h"
#include "Vector4.h"

namespace YoRigine {

	enum class TutorialWaitType {
		Confirm,
		Seconds,
		Event,
	};

	struct TutorialStep {
		std::string name = "新しいステップ";
		std::string speaker;
		std::string text = "説明文を入力してください";
		TutorialWaitType waitType = TutorialWaitType::Confirm;
		float waitSeconds = 2.0f;
		std::string eventName;
		std::string targetUIId;
		bool pauseGameplay = true;
		bool skippable = true;
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
		Vector2 panelPosition{ 640.0f, 610.0f };
		Vector2 panelSize{ 1120.0f, 170.0f };
		Vector2 textOffset{ 0.0f, 0.0f };
		float textMaxWidth = 1020.0f;
		float textPadding = 8.0f;
		int textAlign = 0;
		bool textShadow = false;
		Vector2 shadowOffset{ 3.0f, 3.0f };
		Vector4 shadowColor{ 0.0f, 0.0f, 0.0f, 0.65f };
		bool showControlHint = true;
		std::string hintText = "[SPACE / A] 次へ";
		std::string skipHintText = "[ESC / B] スキップ";
		Vector2 hintOffset{ 420.0f, 50.0f };
		Vector2 hintPanelSize{ 230.0f, 54.0f };
		std::string hintPanelTexturePath;
		Vector4 hintPanelColor{ 0.10f, 0.24f, 0.38f, 0.95f };
		float hintFontSize = 22.0f;
		Vector4 hintTextColor{ 1.0f, 0.95f, 0.72f, 1.0f };
		float hintOutlineWidth = 1.0f;
		Vector4 hintOutlineColor{ 0.0f, 0.0f, 0.0f, 1.0f };
		float hintPadding = 6.0f;
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
		void RefreshRuntimeUI();
		void HideRuntimeUI();
		void ApplyTargetHighlight();
		void ClearTargetHighlight();
		void ApplyGameplayPause(bool pause);

		TutorialData currentData_;
		std::size_t currentStep_ = 0;
		float stepElapsed_ = 0.0f;
		bool playing_ = false;
		bool runtimeUIDirty_ = false;
		bool gameplayPauseOwned_ = false;
		bool gameplayWasPaused_ = false;
		std::unordered_set<std::string> receivedEvents_;
		std::vector<std::string> knownEventNames_;
		std::string highlightedUIId_;
		Vector2 highlightedOriginalScale_{ 1.0f, 1.0f };

#ifdef USE_IMGUI
		TutorialData editorData_;
		std::string editorPath_ = "Resources/Json/Tutorials/NewTutorial.json";
		int editorSelectedStep_ = 0;
		std::string editorStatus_;
		bool editorLivePreview_ = true;
		bool editorPreviewPending_ = false;
		double editorPreviewChangeTime_ = 0.0;
#endif
	};

} // namespace YoRigine
