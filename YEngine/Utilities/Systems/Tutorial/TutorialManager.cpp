#include "TutorialManager.h"

#include <algorithm>
#include <filesystem>

#include "Loaders/Json/Use/AutoJson.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Input/Input.h"
#include "Systems/Text/TextTextureBaker.h"
#include "Systems/UI/UIBase.h"
#include "Systems/UI/UIManager.h"
#include "Loaders/Texture/TextureManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include "Editor/Widgets/YEditorWidget.h"
#endif

namespace {
	constexpr const char* kPanelUIId = "__TutorialRuntimePanel";
	constexpr const char* kTextUIId = "__TutorialRuntimeText";
	constexpr const char* kHintPanelUIId = "__TutorialRuntimeHintPanel";
	constexpr const char* kHintTextUIId = "__TutorialRuntimeHintText";
	constexpr const char* kRuntimeTexturePath = "Resources/UITex/__tutorial_runtime.png";
	constexpr const char* kRuntimeHintTexturePath = "Resources/UITex/__tutorial_runtime_hint.png";

	const char* WaitTypeName(YoRigine::TutorialWaitType type) {
		switch (type) {
		case YoRigine::TutorialWaitType::Seconds: return "seconds";
		case YoRigine::TutorialWaitType::Event: return "event";
		default: return "confirm";
		}
	}

	YoRigine::TutorialWaitType ReadWaitType(const std::string& value) {
		if (value == "seconds") return YoRigine::TutorialWaitType::Seconds;
		if (value == "event") return YoRigine::TutorialWaitType::Event;
		return YoRigine::TutorialWaitType::Confirm;
	}
}

namespace YoRigine {
	void to_json(nlohmann::json& json, const TutorialStep& step) {
		json = {
			{ "name", step.name }, { "speaker", step.speaker }, { "text", step.text },
			{ "waitType", WaitTypeName(step.waitType) }, { "waitSeconds", step.waitSeconds },
			{ "eventName", step.eventName }, { "targetUIId", step.targetUIId },
			{ "pauseGameplay", step.pauseGameplay }, { "skippable", step.skippable },
		};
	}

	void from_json(const nlohmann::json& json, TutorialStep& step) {
		step.name = json.value("name", step.name);
		step.speaker = json.value("speaker", step.speaker);
		step.text = json.value("text", step.text);
		step.waitType = ReadWaitType(json.value("waitType", std::string("confirm")));
		step.waitSeconds = json.value("waitSeconds", step.waitSeconds);
		step.eventName = json.value("eventName", step.eventName);
		step.targetUIId = json.value("targetUIId", step.targetUIId);
		step.pauseGameplay = json.value("pauseGameplay", step.pauseGameplay);
		step.skippable = json.value("skippable", step.skippable);
	}

	namespace {
		class TutorialJsonSchema {
		public:
			explicit TutorialJsonSchema(TutorialData& data) {
				style_.Add("fontFamily", &data.style.fontFamily)
					.Add("fontFilePath", &data.style.fontFilePath)
					.Add("fontSize", &data.style.fontSize)
					.Add("textColor", &data.style.textColor)
					.Add("outlineWidth", &data.style.outlineWidth)
					.Add("outlineColor", &data.style.outlineColor)
					.Add("panelColor", &data.style.panelColor)
					.Add("panelTexturePath", &data.style.panelTexturePath)
					.Add("panelPosition", &data.style.panelPosition)
					.Add("panelSize", &data.style.panelSize)
					.Add("textOffset", &data.style.textOffset)
					.Add("textMaxWidth", &data.style.textMaxWidth)
					.Add("textPadding", &data.style.textPadding)
					.Add("textAlign", &data.style.textAlign)
					.Add("textShadow", &data.style.textShadow)
					.Add("shadowOffset", &data.style.shadowOffset)
					.Add("shadowColor", &data.style.shadowColor)
					.Add("showControlHint", &data.style.showControlHint)
					.Add("hintText", &data.style.hintText)
					.Add("skipHintText", &data.style.skipHintText)
					.Add("hintOffset", &data.style.hintOffset)
					.Add("hintPanelSize", &data.style.hintPanelSize)
					.Add("hintPanelTexturePath", &data.style.hintPanelTexturePath)
					.Add("hintPanelColor", &data.style.hintPanelColor)
					.Add("hintFontSize", &data.style.hintFontSize)
					.Add("hintTextColor", &data.style.hintTextColor)
					.Add("hintOutlineWidth", &data.style.hintOutlineWidth)
					.Add("hintOutlineColor", &data.style.hintOutlineColor)
					.Add("hintPadding", &data.style.hintPadding)
					.Add("layer", &data.style.layer);

				root_.Add("version", &version_)
					.Add("name", &data.name)
					.AddGroup("style", style_)
					.Add("steps", &data.steps);
			}

			AutoJson& Root() { return root_; }

		private:
			int version_ = 1;
			AutoJson root_;
			AutoJson style_;
		};
	}

	TutorialManager* TutorialManager::GetInstance() {
		static TutorialManager instance;
		return &instance;
	}

	bool TutorialManager::Save(const TutorialData& data, const std::string& path) const {
		if (path.empty()) return false;
		try {
			TutorialData saveData = data;
			TutorialJsonSchema schema(saveData);
			schema.Root().SaveToFile(path);
			return std::filesystem::exists(path);
		}
		catch (...) {
			return false;
		}
	}

	bool TutorialManager::Load(TutorialData& data, const std::string& path) {
		try {
			if (!std::filesystem::exists(path)) return false;
			TutorialData loaded;
			TutorialJsonSchema schema(loaded);
			schema.Root().LoadFromFile(path);
			for (const TutorialStep& step : loaded.steps) RegisterEventName(step.eventName);
			data = std::move(loaded);
			return true;
		}
		catch (...) {
			return false;
		}
	}

	bool TutorialManager::LoadAndStart(const std::string& path, std::size_t startStep) {
		TutorialData data;
		if (!Load(data, path)) return false;
		Start(data, startStep);
		return playing_;
	}

	void TutorialManager::Start(const TutorialData& data, std::size_t startStep) {
		Stop();
		if (data.steps.empty() || startStep >= data.steps.size()) return;
		currentData_ = data;
		playing_ = true;
		EnterStep(startStep);
	}

	void TutorialManager::Stop() {
		ClearTargetHighlight();
		ApplyGameplayPause(false);
		HideRuntimeUI();
		playing_ = false;
		runtimeUIDirty_ = false;
		currentStep_ = 0;
		stepElapsed_ = 0.0f;
		receivedEvents_.clear();
	}

	void TutorialManager::Update() {
		if (!playing_ || currentStep_ >= currentData_.steps.size()) return;
		if (runtimeUIDirty_ || !UIManager::GetInstance()->HasUI(kPanelUIId) || !UIManager::GetInstance()->HasUI(kTextUIId)) {
			RefreshRuntimeUI();
			runtimeUIDirty_ = false;
		}
		stepElapsed_ += GameTime::GetDeltaTime(TimeChannel::UI);

		const TutorialStep& step = currentData_.steps[currentStep_];
		bool completed = false;
		switch (step.waitType) {
		case TutorialWaitType::Seconds:
			completed = stepElapsed_ >= std::max(0.0f, step.waitSeconds);
			break;
		case TutorialWaitType::Event:
			completed = !step.eventName.empty() && receivedEvents_.contains(step.eventName);
			break;
		case TutorialWaitType::Confirm: {
			Input* input = Input::GetInstance();
			completed = input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
				input->IsPadTriggered(0, GamePadButton::A);
			break;
		}
		}

		Input* input = Input::GetInstance();
		if (step.skippable && (input->TriggerKey(DIK_ESCAPE) || input->IsPadTriggered(0, GamePadButton::B))) {
			completed = true;
		}
		if (completed) Advance();
	}

	void TutorialManager::NotifyEvent(const std::string& eventName) {
		RegisterEventName(eventName);
		if (playing_ && !eventName.empty()) receivedEvents_.insert(eventName);
	}

	void TutorialManager::RegisterEventName(const std::string& eventName) {
		if (eventName.empty()) return;
		if (std::find(knownEventNames_.begin(), knownEventNames_.end(), eventName) == knownEventNames_.end()) {
			knownEventNames_.push_back(eventName);
			std::sort(knownEventNames_.begin(), knownEventNames_.end());
		}
	}

	void TutorialManager::Advance() {
		if (!playing_) return;
		const std::size_t next = currentStep_ + 1;
		if (next >= currentData_.steps.size()) Stop();
		else EnterStep(next);
	}

	void TutorialManager::EnterStep(std::size_t index) {
		ClearTargetHighlight();
		currentStep_ = index;
		stepElapsed_ = 0.0f;
		receivedEvents_.clear();
		const TutorialStep& step = currentData_.steps[currentStep_];
		ApplyGameplayPause(step.pauseGameplay);
		// Editor::Draw 中に Start される場合があるため、GPUテクスチャの更新は
		// Framework::Update 後に呼ばれる TutorialManager::Update まで遅延する。
		runtimeUIDirty_ = true;
		ApplyTargetHighlight();
	}

	void TutorialManager::RefreshRuntimeUI() {
		if (!playing_ || currentStep_ >= currentData_.steps.size()) return;
		const TutorialStep& step = currentData_.steps[currentStep_];
		const TutorialStyle& style = currentData_.style;

		std::string displayText;
		if (!step.speaker.empty()) displayText = step.speaker + "\n";
		displayText += step.text;

		std::string hintText;
		if (style.showControlHint) {
			if (step.waitType == TutorialWaitType::Confirm) hintText = style.hintText;
			else if (step.skippable) hintText = style.skipHintText;
		}

		TextBakeParams params;
		params.text = displayText;
		params.fontFamily = style.fontFamily;
		params.fontFilePath = style.fontFilePath;
		params.fontSize = style.fontSize;
		params.fillColor = style.textColor;
		params.outlineWidth = style.outlineWidth;
		params.outlineColor = style.outlineColor;
		params.padding = style.textPadding;
		params.maxWidth = style.textMaxWidth;
		params.align = style.textAlign;
		params.shadow = style.textShadow;
		params.shadowOffset = style.shadowOffset;
		params.shadowColor = style.shadowColor;
		TextTextureBaker::Bake(params, kRuntimeTexturePath, false);

		if (!hintText.empty()) {
			TextBakeParams hintParams;
			hintParams.text = hintText;
			hintParams.fontFamily = style.fontFamily;
			hintParams.fontFilePath = style.fontFilePath;
			hintParams.fontSize = style.hintFontSize;
			hintParams.fillColor = style.hintTextColor;
			hintParams.outlineWidth = style.hintOutlineWidth;
			hintParams.outlineColor = style.hintOutlineColor;
			hintParams.padding = style.hintPadding;
			TextTextureBaker::Bake(hintParams, kRuntimeHintTexturePath, false);
		}

		UIManager* uiManager = UIManager::GetInstance();
		UIBase* panel = uiManager->GetUI(kPanelUIId);
		if (!panel) {
			auto created = std::make_unique<UIBase>(kPanelUIId);
			created->Initialize("");
			created->SetTransient(true);
			panel = created.get();
			uiManager->AddUI(kPanelUIId, std::move(created));
		}
		panel->SetAnchorPoint({ 0.5f, 0.5f });
		panel->SetTexture(style.panelTexturePath.empty() ? "./Resources/images/white.png" : style.panelTexturePath);
		panel->SetPosition({ style.panelPosition.x, style.panelPosition.y, 0.0f });
		panel->SetSize(style.panelSize);
		panel->SetColor(style.panelColor);
		panel->SetLayer(style.layer);
		panel->SetVisible(true);

		UIBase* text = uiManager->GetUI(kTextUIId);
		if (!text) {
			auto created = std::make_unique<UIBase>(kTextUIId);
			created->Initialize("");
			created->SetTransient(true);
			text = created.get();
			uiManager->AddUI(kTextUIId, std::move(created));
		}
		TextureManager::GetInstance()->ReloadTexture(kRuntimeTexturePath);
		text->SetTexture(kRuntimeTexturePath);
		text->SetAnchorPoint({ 0.5f, 0.5f });
		text->SetPosition({ style.panelPosition.x + style.textOffset.x,
			style.panelPosition.y + style.textOffset.y, 0.0f });
		text->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		text->SetLayer(style.layer + 1);
		text->SetVisible(true);
		text->StopAnimation(UIAnimationType::FadeIn);
		text->SetAlpha(1.0f);

		UIBase* hintPanel = uiManager->GetUI(kHintPanelUIId);
		if (!hintPanel) {
			auto created = std::make_unique<UIBase>(kHintPanelUIId);
			created->Initialize("");
			created->SetTransient(true);
			hintPanel = created.get();
			uiManager->AddUI(kHintPanelUIId, std::move(created));
		}
		const Vector2 hintPosition{
			style.panelPosition.x + style.hintOffset.x,
			style.panelPosition.y + style.hintOffset.y
		};
		hintPanel->SetAnchorPoint({ 0.5f, 0.5f });
		hintPanel->SetTexture(style.hintPanelTexturePath.empty() ?
			"./Resources/images/white.png" : style.hintPanelTexturePath);
		hintPanel->SetPosition({ hintPosition.x, hintPosition.y, 0.0f });
		hintPanel->SetSize(style.hintPanelSize);
		hintPanel->SetColor(style.hintPanelColor);
		hintPanel->SetLayer(style.layer + 2);
		hintPanel->SetVisible(!hintText.empty());

		UIBase* hint = uiManager->GetUI(kHintTextUIId);
		if (!hint) {
			auto created = std::make_unique<UIBase>(kHintTextUIId);
			created->Initialize("");
			created->SetTransient(true);
			hint = created.get();
			uiManager->AddUI(kHintTextUIId, std::move(created));
		}
		if (!hintText.empty()) {
			TextureManager::GetInstance()->ReloadTexture(kRuntimeHintTexturePath);
			hint->SetTexture(kRuntimeHintTexturePath);
		}
		hint->SetAnchorPoint({ 0.5f, 0.5f });
		hint->SetPosition({ hintPosition.x, hintPosition.y, 0.0f });
		hint->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		hint->SetLayer(style.layer + 3);
		hint->SetVisible(!hintText.empty());
		hint->StopAnimation(UIAnimationType::FadeIn);
		hint->SetAlpha(1.0f);

		uiManager->BringToFront(kPanelUIId);
		uiManager->BringToFront(kTextUIId);
		uiManager->BringToFront(kHintPanelUIId);
		uiManager->BringToFront(kHintTextUIId);
		uiManager->SortByLayer();
		// TutorialManager は通常の UIManager::UpdateAll より後に更新されるため、
		// 新規生成したスプライトの頂点をこのフレーム分だけ即時更新する。
		panel->Update();
		text->Update();
		hintPanel->Update();
		hint->Update();
	}

	void TutorialManager::HideRuntimeUI() {
		UIManager* manager = UIManager::GetInstance();
		if (UIBase* panel = manager->GetUI(kPanelUIId)) panel->SetVisible(false);
		if (UIBase* text = manager->GetUI(kTextUIId)) text->SetVisible(false);
		if (UIBase* hintPanel = manager->GetUI(kHintPanelUIId)) hintPanel->SetVisible(false);
		if (UIBase* hint = manager->GetUI(kHintTextUIId)) hint->SetVisible(false);
	}

	void TutorialManager::ApplyTargetHighlight() {
		const std::string& targetId = currentData_.steps[currentStep_].targetUIId;
		if (targetId.empty() || targetId == kPanelUIId || targetId == kTextUIId ||
			targetId == kHintPanelUIId || targetId == kHintTextUIId) return;
		if (UIBase* target = UIManager::GetInstance()->GetUI(targetId)) {
			highlightedUIId_ = targetId;
			highlightedOriginalScale_ = target->GetScale();
			target->PlayPulse(1.12f, 0.55f, true);
			UIManager::GetInstance()->BringToFront(targetId);
		}
	}

	void TutorialManager::ClearTargetHighlight() {
		if (!highlightedUIId_.empty()) {
			if (UIBase* target = UIManager::GetInstance()->GetUI(highlightedUIId_)) {
				target->StopAnimation(UIAnimationType::Pulse);
				target->SetScale(highlightedOriginalScale_);
			}
		}
		highlightedUIId_.clear();
	}

	void TutorialManager::ApplyGameplayPause(bool pause) {
		if (pause && !gameplayPauseOwned_) {
			gameplayWasPaused_ = GameTime::IsChannelPaused(TimeChannel::Gameplay);
			GameTime::SetChannelPaused(TimeChannel::Gameplay, true);
			gameplayPauseOwned_ = true;
		}
		else if (!pause && gameplayPauseOwned_) {
			GameTime::SetChannelPaused(TimeChannel::Gameplay, gameplayWasPaused_);
			gameplayPauseOwned_ = false;
		}
	}

} // namespace YoRigine
