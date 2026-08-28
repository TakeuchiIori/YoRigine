#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>

#include "Vector2.h"
#include "Vector4.h"

#include "TutorialCondition.h"
#include "TutorialProgress.h"
#include "TutorialSignal.h"
#include "TutorialSpotlight.h"

namespace YoRigine {

	enum class TutorialWaitType {
		Confirm,
		Seconds,
		Event,
	};

	// 注目させたいUIを揺らして目立たせる設定。
	// 暗幕（スポットライト）が「周りを暗くする」のに対し、こちらは「対象を動かす」。
	// 併用すると分かりやすい。
	struct TutorialHighlight {
		bool enabled = false;
		std::vector<std::string> uiIds;
		// UIに保存済みのアニメーションクリップ名。指定するとこちらを再生し、
		// 下のプリセット（pulse / blink）は使わない。
		// クリップは UIアニメーションエディタで作り、UIのconfig JSONへ保存される。
		// 新しい動きが欲しいたびにここへフラグを増やさずに済む。
		std::string clipName;
		// 拡大縮小の脈動。scaleAmount は最大倍率、seconds は1往復にかける時間。
		bool pulse = true;
		float scaleAmount = 1.2f;
		float pulseSeconds = 0.5f;
		// 点滅（アルファの明滅）。
		bool blink = true;
		float blinkSeconds = 0.8f;
		// 他のUIに隠れないよう最前面へ持ち上げる。
		bool bringToFront = true;
	};

	// 説明中に受け付ける操作の制限。
	// allow に挙げたアクションだけを有効にし、それ以外を一時的に封じる。
	// 「回避だけ練習させる」といった誘導に使う。
	struct TutorialGate {
		bool enabled = false;
		std::vector<std::string> allow;
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

	// 説明UIを構成する要素1つぶんの見た目。
	// 4要素（パネル / 本文 / ヒント背景 / ヒント文字）が同じ形の設定を持ち、
	// それぞれ独立に配置・着色・アニメーションできる。
	struct TutorialElementLayout {
		bool visible = true;
		Vector2 position{ 640.0f, 570.0f };   // 画面座標
		Vector2 size{ 100.0f, 100.0f };       // パネル系のみ有効（文字は実寸で描く）
		Vector2 anchorPoint{ 0.5f, 0.5f };
		// style の色へ掛ける倍率。ページ単位で濃さを変えたいときに使う。
		Vector4 colorTint{ 1.0f, 1.0f, 1.0f, 1.0f };
		// style.layer からの相対。要素同士の重なり順を決める。
		int layerOffset = 0;
		// 表示時に再生するアニメーションクリップ名。
		// スライドイン等の凝った動きは、UIアニメーションエディタで作ってここで指定する。
		std::string clipName;
	};

	// 説明ページごとのレイアウト。
	// ページによって本文量や補足画像の位置が異なるため、共通Styleとは分離して保存する。
	struct TutorialStepLayout {
		// 既定値は旧形式の既定レイアウトと同じ見た目になるよう揃えてある。
		TutorialElementLayout panel{
			.position = { 640.0f, 570.0f }, .size = { 1120.0f, 240.0f }, .layerOffset = 0 };
		TutorialElementLayout text{
			.position = { 640.0f, 535.0f }, .layerOffset = 1 };
		TutorialElementLayout hintPanel{
			.position = { 640.0f, 655.0f }, .size = { 540.0f, 54.0f }, .layerOffset = 2 };
		TutorialElementLayout hintText{
			.position = { 640.0f, 655.0f }, .layerOffset = 3 };
		// 本文の折り返し幅（テキストをベイクするときに使う。配置ではない）
		float textMaxWidth = 1020.0f;
	};

	// 旧形式（panelPosition / textOffset / hintOffset …）のレイアウトを
	// 要素ごとの形式へ変換する。読み込み時の互換のためだけに使う。
	TutorialStepLayout MakeLegacyStepLayout(const Vector2& panelPosition,
		const Vector2& panelSize, const Vector2& textOffset, float textMaxWidth,
		const Vector2& hintOffset, const Vector2& hintPanelSize);

	struct TutorialStep {
		std::string name = "新しいステップ";
		std::string speaker;
		std::string text = "説明文を入力してください";
		// 完了条件。type が None のときだけ、従来の waitType へフォールバックする。
		// 既存の JSON には complete が無いため、そのまま読み込めば従来動作になる。
		TutorialCondition complete;
		// 開始条件。type が None なら「前のステップが終わったら順番に開始」となり、
		// 従来の線形進行と同じ挙動になる。設定するとその条件が成立するまで待機する。
		TutorialCondition trigger;
		// 注目させたい場所以外を暗幕で覆う設定。enabled が false なら何もしない。
		TutorialSpotlightConfig spotlight;
		// 注目させたいUIを揺らす設定。enabled が false なら targetUIId の従来動作。
		TutorialHighlight highlight;
		// 表示中に受け付ける操作の制限。
		TutorialGate gate;
		// 一度見たら二度と出さない。既読は TutorialProgress へ保存される。
		bool once = false;
		// 表示中のゲーム速度（1.0=等速 / 0.3=スロー）。pauseGameplay が true なら無視される。
		// hitstop とは掛け算で合成されるので、演出を潰さずに共存する。
		float gameplaySpeed = 1.0f;
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

		// 説明パネルと暗幕を描画する。
		// シーンごとに描画レイヤーの扱いが違い（GameUI は特定レイヤーしか描かない）、
		// UIManager の一括描画に任せると出るシーンと出ないシーンができるため、
		// どのシーンでも同じ見え方になるよう自前で描く。
		void Draw();
		void NotifyEvent(const std::string& eventName);
		void RegisterEventName(const std::string& eventName);
		const std::vector<std::string>& GetKnownEventNames() const { return knownEventNames_; }
		void Advance();

		bool IsPlaying() const { return playing_; }
		// 説明を表示中か。開始条件の成立待ちで何も出していない間は false になる。
		bool HasActiveStep() const { return hasActiveStep_; }
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

		// 開始条件まわり。
		void ResetStepStates(std::size_t startStep);
		void UpdateTriggers();
		bool TryActivateNext();
		void FinishCurrentStep();
		bool AllStepsDone() const;

		// 入力ゲート。
		void ApplyGate(const TutorialGate& gate);
		void ReleaseGate();

		// 表示中のゲーム速度。解除まで維持される持続スケールを使う。
		void ApplyGameplaySpeed(float speed);
		void ReleaseGameplaySpeed();

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

		// ステップの進行状態。
		// 線形カーソル1本ではなく「待機 → 起動 → 完了」の集合として持つ。
		enum class StepState {
			Waiting,   // 開始条件の成立待ち
			Queued,    // 条件が成立し、表示の順番待ち
			Done,      // 完了済み（既読スキップを含む）
		};
		std::vector<StepState> stepStates_;
		// 待機中ステップの開始条件。stepStates_ と同じ添字で対応する。
		std::vector<TutorialConditionRuntime> triggerRuntimes_;
		// 条件が成立して表示待ちになっているステップの添字。
		std::vector<std::size_t> pendingQueue_;
		// 開始条件を持たないステップを順番に起動していくための位置。
		std::size_t autoCursor_ = 0;
		// チュートリアル開始からの経過秒。開始条件の elapsed はこれを見る。
		float totalElapsed_ = 0.0f;
		bool hasActiveStep_ = false;
		bool gateOwned_ = false;
		bool gameplaySpeedOwned_ = false;

		// 強調を掛けたUIの元の見た目。解除時にここへ戻す。
		struct HighlightRestore {
			std::string uiId;
			std::string clipName;   // クリップ再生の場合のみ。停止に使う。
			Vector2 scale{ 1.0f, 1.0f };
			Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		};
		std::vector<HighlightRestore> highlightRestores_;

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
