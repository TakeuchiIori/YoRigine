#include "TutorialManager.h"

#include <algorithm>
#include <filesystem>

#include "Loaders/Json/Use/AutoJson.h"
#include "Loaders/Texture/TextureManager.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Input/Input.h"
#include "Systems/Input/InputActionMap.h"
#include "Systems/Text/TextTextureBaker.h"
#include "Systems/UI/UIBase.h"
#include "Systems/UI/UIManager.h"

#ifdef USE_IMGUI
#include "Editor/Widgets/YEditorWidget.h"
#include <imgui.h>
#endif

namespace {
constexpr const char *kPanelUIId = "__TutorialRuntimePanel";
constexpr const char *kTextUIId = "__TutorialRuntimeText";
constexpr const char *kHintPanelUIId = "__TutorialRuntimeHintPanel";
constexpr const char *kHintTextUIId = "__TutorialRuntimeHintText";
constexpr const char *kRuntimeTexturePath =
    "Resources/UITex/__tutorial_runtime.png";
constexpr const char *kRuntimeHintTexturePath =
    "Resources/UITex/__tutorial_runtime_hint.png";
constexpr const char *kAdditionalUIPrefix = "__TutorialRuntimeAdditionalUI_";

std::string AdditionalUIId(std::size_t index) {
  return std::string(kAdditionalUIPrefix) + std::to_string(index);
}

BYTE KeyboardKeyCode(const std::string &keyName) {
  if (keyName == "ENTER")
    return DIK_RETURN;
  if (keyName == "E")
    return DIK_E;
  if (keyName == "F")
    return DIK_F;
  if (keyName == "TAB")
    return DIK_TAB;
  if (keyName == "ESC")
    return DIK_ESCAPE;
  return DIK_SPACE;
}

GamePadButton GamepadButtonCode(const std::string &buttonName) {
  if (buttonName == "B")
    return GamePadButton::B;
  if (buttonName == "X")
    return GamePadButton::X;
  if (buttonName == "Y")
    return GamePadButton::Y;
  if (buttonName == "START")
    return GamePadButton::Start;
  if (buttonName == "BACK")
    return GamePadButton::Back;
  return GamePadButton::A;
}

const char *WaitTypeName(YoRigine::TutorialWaitType type) {
  switch (type) {
  case YoRigine::TutorialWaitType::Seconds:
    return "seconds";
  case YoRigine::TutorialWaitType::Event:
    return "event";
  default:
    return "confirm";
  }
}

const char *SpotlightKindName(YoRigine::TutorialSpotlightTargetKind kind) {
  switch (kind) {
  case YoRigine::TutorialSpotlightTargetKind::Rect:
    return "rect";
  case YoRigine::TutorialSpotlightTargetKind::World:
    return "world";
  case YoRigine::TutorialSpotlightTargetKind::Ui:
    break;
  default:
    break;
  }
  return "ui";
}

YoRigine::TutorialSpotlightTargetKind
SpotlightKindFromName(const std::string &value) {
  if (value == "rect")
    return YoRigine::TutorialSpotlightTargetKind::Rect;
  if (value == "world")
    return YoRigine::TutorialSpotlightTargetKind::World;
  return YoRigine::TutorialSpotlightTargetKind::Ui;
}

YoRigine::TutorialWaitType ReadWaitType(const std::string &value) {
  if (value == "seconds")
    return YoRigine::TutorialWaitType::Seconds;
  if (value == "event")
    return YoRigine::TutorialWaitType::Event;
  return YoRigine::TutorialWaitType::Confirm;
}
} // namespace

namespace YoRigine {
void to_json(nlohmann::json &json, const TutorialStepLayout &layout) {
  json = {
      {"panelPosition", layout.panelPosition},
      {"panelSize", layout.panelSize},
      {"textOffset", layout.textOffset},
      {"textMaxWidth", layout.textMaxWidth},
      {"hintOffset", layout.hintOffset},
      {"hintPanelSize", layout.hintPanelSize},
  };
}

void from_json(const nlohmann::json &json, TutorialStepLayout &layout) {
  layout.panelPosition = json.value("panelPosition", layout.panelPosition);
  layout.panelSize = json.value("panelSize", layout.panelSize);
  layout.textOffset = json.value("textOffset", layout.textOffset);
  layout.textMaxWidth = json.value("textMaxWidth", layout.textMaxWidth);
  layout.hintOffset = json.value("hintOffset", layout.hintOffset);
  layout.hintPanelSize = json.value("hintPanelSize", layout.hintPanelSize);
}

void to_json(nlohmann::json &json, const TutorialStepUI &ui) {
  json = {
      {"name", ui.name},
      {"texturePath", ui.texturePath},
      {"position", ui.position},
      {"size", ui.size},
      {"anchorPoint", ui.anchorPoint},
      {"color", ui.color},
      {"layerOffset", ui.layerOffset},
  };
}

void from_json(const nlohmann::json &json, TutorialStepUI &ui) {
  ui.name = json.value("name", ui.name);
  ui.texturePath = json.value("texturePath", ui.texturePath);
  ui.position = json.value("position", ui.position);
  ui.size = json.value("size", ui.size);
  ui.anchorPoint = json.value("anchorPoint", ui.anchorPoint);
  ui.color = json.value("color", ui.color);
  ui.layerOffset = json.value("layerOffset", ui.layerOffset);
}

void to_json(nlohmann::json &json, const TutorialGate &gate) {
  json = {
      {"enabled", gate.enabled},
      {"allow", gate.allow},
  };
}

void from_json(const nlohmann::json &json, TutorialGate &gate) {
  gate.enabled = json.value("enabled", gate.enabled);
  gate.allow = json.value("allow", std::vector<std::string>{});
}

void to_json(nlohmann::json &json, const TutorialSpotlightTarget &target) {
  json = {
      {"kind", SpotlightKindName(target.kind)},
      {"id", target.id},
      {"center", target.center},
      {"size", target.size},
      {"radius", target.radius},
  };
}

void from_json(const nlohmann::json &json, TutorialSpotlightTarget &target) {
  target.kind = SpotlightKindFromName(json.value("kind", std::string("ui")));
  target.id = json.value("id", target.id);
  target.center = json.value("center", target.center);
  target.size = json.value("size", target.size);
  target.radius = json.value("radius", target.radius);
}

void to_json(nlohmann::json &json, const TutorialSpotlightConfig &spotlight) {
  json = {
      {"enabled", spotlight.enabled},         {"targets", spotlight.targets},
      {"dimColor", spotlight.dimColor},       {"padding", spotlight.padding},
      {"fadeSeconds", spotlight.fadeSeconds},
  };
}

void from_json(const nlohmann::json &json, TutorialSpotlightConfig &spotlight) {
  spotlight.enabled = json.value("enabled", spotlight.enabled);
  spotlight.targets =
      json.value("targets", std::vector<TutorialSpotlightTarget>{});
  spotlight.dimColor = json.value("dimColor", spotlight.dimColor);
  spotlight.padding = json.value("padding", spotlight.padding);
  spotlight.fadeSeconds = json.value("fadeSeconds", spotlight.fadeSeconds);
}

// 条件ツリーは入れ子になるため、vector<TutorialCondition>
// の変換より先に宣言しておく。
void to_json(nlohmann::json &json, const TutorialCondition &condition);
void from_json(const nlohmann::json &json, TutorialCondition &condition);

void to_json(nlohmann::json &json, const TutorialCondition &condition) {
  json = nlohmann::json::object();
  json["type"] = TutorialConditionTypeName(condition.type);

  // 種別に関係の無いキーを書かないことで、手書きしたときに読みやすくする。
  if (condition.type == TutorialConditionType::Signal) {
    json["name"] = condition.signalName;
    json["count"] = condition.requiredCount;
  }
  if (condition.type == TutorialConditionType::Elapsed) {
    json["seconds"] = condition.seconds;
  }
  if (!condition.children.empty()) {
    json["children"] = condition.children;
  }
}

void from_json(const nlohmann::json &json, TutorialCondition &condition) {
  condition.type =
      TutorialConditionTypeFromName(json.value("type", std::string("none")));
  condition.signalName = json.value("name", condition.signalName);
  condition.requiredCount = json.value("count", condition.requiredCount);
  condition.seconds = json.value("seconds", condition.seconds);
  condition.children = json.value("children", std::vector<TutorialCondition>{});
}

void to_json(nlohmann::json &json, const TutorialStep &step) {
  json = {
      {"name", step.name},
      {"speaker", step.speaker},
      {"text", step.text},
      {"waitType", WaitTypeName(step.waitType)},
      {"waitSeconds", step.waitSeconds},
      {"eventName", step.eventName},
      {"targetUIId", step.targetUIId},
      {"pauseGameplay", step.pauseGameplay},
      {"skippable", step.skippable},
      {"layout", step.layout},
      {"additionalUIs", step.additionalUIs},
      {"complete", step.complete},
      {"trigger", step.trigger},
      {"spotlight", step.spotlight},
      {"gate", step.gate},
      {"once", step.once},
  };
}

void from_json(const nlohmann::json &json, TutorialStep &step) {
  step.name = json.value("name", step.name);
  step.speaker = json.value("speaker", step.speaker);
  step.text = json.value("text", step.text);
  step.waitType = ReadWaitType(json.value("waitType", std::string("confirm")));
  step.waitSeconds = json.value("waitSeconds", step.waitSeconds);
  step.eventName = json.value("eventName", step.eventName);
  step.targetUIId = json.value("targetUIId", step.targetUIId);
  step.pauseGameplay = json.value("pauseGameplay", step.pauseGameplay);
  step.skippable = json.value("skippable", step.skippable);
  step.layout = json.value("layout", step.layout);
  step.additionalUIs = json.value("additionalUIs", step.additionalUIs);
  // complete が無い既存ファイルは type=None のままになり、waitType で進む。
  step.complete = json.value("complete", TutorialCondition{});
  step.spotlight = json.value("spotlight", TutorialSpotlightConfig{});
  // trigger が無い既存ファイルは type=None のままとなり、従来の線形進行になる。
  step.trigger = json.value("trigger", TutorialCondition{});
  step.gate = json.value("gate", TutorialGate{});
  step.once = json.value("once", step.once);
}

namespace {
class TutorialJsonSchema {
public:
  explicit TutorialJsonSchema(TutorialData &data) {
    style_.Add("fontFamily", &data.style.fontFamily)
        .Add("fontFilePath", &data.style.fontFilePath)
        .Add("fontSize", &data.style.fontSize)
        .Add("textColor", &data.style.textColor)
        .Add("outlineWidth", &data.style.outlineWidth)
        .Add("outlineColor", &data.style.outlineColor)
        .Add("panelColor", &data.style.panelColor)
        .Add("panelTexturePath", &data.style.panelTexturePath)
        .Add("textPadding", &data.style.textPadding)
        .Add("textAlign", &data.style.textAlign)
        .Add("textShadow", &data.style.textShadow)
        .Add("shadowOffset", &data.style.shadowOffset)
        .Add("shadowColor", &data.style.shadowColor)
        .Add("showControlHint", &data.style.showControlHint)
        .Add("hintText", &data.style.hintText)
        .Add("skipHintText", &data.style.skipHintText)
        .Add("hintPanelTexturePath", &data.style.hintPanelTexturePath)
        .Add("hintPanelColor", &data.style.hintPanelColor)
        .Add("hintFontSize", &data.style.hintFontSize)
        .Add("hintTextColor", &data.style.hintTextColor)
        .Add("hintOutlineWidth", &data.style.hintOutlineWidth)
        .Add("hintOutlineColor", &data.style.hintOutlineColor)
        .Add("hintPadding", &data.style.hintPadding)
        .Add("fadeInSeconds", &data.style.fadeInSeconds)
        .Add("fadeOutSeconds", &data.style.fadeOutSeconds)
        .Add("confirmKeyboardKey", &data.style.confirmKeyboardKey)
        .Add("confirmGamepadButton", &data.style.confirmGamepadButton)
        .Add("skipKeyboardKey", &data.style.skipKeyboardKey)
        .Add("skipGamepadButton", &data.style.skipGamepadButton)
        .Add("autoBuildControlHint", &data.style.autoBuildControlHint)
        .Add("layer", &data.style.layer);

    root_.Add("version", &version_)
        .Add("name", &data.name)
        .AddGroup("style", style_)
        .Add("steps", &data.steps);
  }

  AutoJson &Root() { return root_; }

private:
  int version_ = 1;
  AutoJson root_;
  AutoJson style_;
};
} // namespace

TutorialManager *TutorialManager::GetInstance() {
  static TutorialManager instance;
  return &instance;
}

bool TutorialManager::Save(const TutorialData &data,
                           const std::string &path) const {
  if (path.empty())
    return false;
  try {
    TutorialData saveData = data;
    TutorialJsonSchema schema(saveData);
    schema.Root().SaveToFile(path);
    return std::filesystem::exists(path);
  } catch (...) {
    return false;
  }
}

bool TutorialManager::Load(TutorialData &data, const std::string &path) {
  try {
    if (!std::filesystem::exists(path))
      return false;
    TutorialData loaded;
    TutorialJsonSchema schema(loaded);
    schema.Root().LoadFromFile(path);
    for (const TutorialStep &step : loaded.steps)
      RegisterEventName(step.eventName);
    data = std::move(loaded);
    return true;
  } catch (...) {
    return false;
  }
}

bool TutorialManager::LoadAndStart(const std::string &path,
                                   std::size_t startStep) {
  TutorialData data;
  if (!Load(data, path))
    return false;
  Start(data, startStep);
  return playing_;
}

void TutorialManager::Start(const TutorialData &data, std::size_t startStep) {
  Stop();
  if (data.steps.empty() || startStep >= data.steps.size())
    return;
  currentData_ = data;
  playing_ = true;
  SubscribeSignals();
  ResetStepStates(startStep);

  // 開始条件を持たないステップがあれば即座に始まる。
  // 全ステップが条件待ちの場合は、何も表示せず成立を待つ状態になる。
  if (!TryActivateNext() && AllStepsDone())
    Stop();
}

void TutorialManager::ResetStepStates(std::size_t startStep) {
  const std::size_t count = currentData_.steps.size();
  stepStates_.assign(count, StepState::Waiting);
  triggerRuntimes_.clear();
  triggerRuntimes_.resize(count);
  pendingQueue_.clear();
  autoCursor_ = startStep;
  totalElapsed_ = 0.0f;
  hasActiveStep_ = false;

  TutorialProgress *progress = TutorialProgress::GetInstance();
  for (std::size_t i = 0; i < count; ++i) {
    const TutorialStep &step = currentData_.steps[i];

    // startStep より前は「もう見た扱い」にして飛ばす。
    if (i < startStep) {
      stepStates_[i] = StepState::Done;
      continue;
    }
    // once 指定のステップは、既読なら最初から完了扱いにする。
    if (step.once && progress->IsSeen(currentData_.name, step.name)) {
      stepStates_[i] = StepState::Done;
      continue;
    }
    triggerRuntimes_[i].Reset(step.trigger);
  }
}

void TutorialManager::SubscribeSignals() {
  // 入力アクションなどの内蔵発火源を繋ぐ。二重接続は内部で弾かれる。
  TutorialSignal::GetInstance()->ConnectEngineSources();

  if (signalHandle_ != 0)
    return;
  signalHandle_ = TutorialSignal::GetInstance()->Subscribe(
      [this](const TutorialSignalData &signal) {
        if (!playing_)
          return;
        // 旧 waitType=Event との互換のため、名前を受信済みイベントにも残す。
        receivedEvents_.insert(signal.name);
        completeRuntime_.OnSignal(signal);

        // 待機中ステップの開始条件へも配る。表示中でも裏で成立を数える。
        for (std::size_t i = 0; i < triggerRuntimes_.size(); ++i) {
          if (stepStates_[i] != StepState::Waiting)
            continue;
          triggerRuntimes_[i].OnSignal(signal);
        }
      });
}

void TutorialManager::UnsubscribeSignals() {
  if (signalHandle_ == 0)
    return;
  TutorialSignal::GetInstance()->Unsubscribe(signalHandle_);
  signalHandle_ = 0;
}

void TutorialManager::Stop() {
  ClearTargetHighlight();
  ApplyGameplayPause(false);
  HideRuntimeUI();
  // 条件の評価状態は currentData_ 内の定義を指しているため、
  // データが差し替わる前に必ず切り離す。
  UnsubscribeSignals();
  completeRuntime_.Clear();
  triggerRuntimes_.clear();
  stepStates_.clear();
  pendingQueue_.clear();
  TutorialSpotlight::GetInstance()->Clear();
  ReleaseGate();
  playing_ = false;
  hasActiveStep_ = false;
  runtimeUIDirty_ = false;
  currentStep_ = 0;
  autoCursor_ = 0;
  stepElapsed_ = 0.0f;
  totalElapsed_ = 0.0f;
  transitionElapsed_ = 0.0f;
  transitionOpacity_ = 1.0f;
  transitionStartOpacity_ = 1.0f;
  transitionPhase_ = TransitionPhase::Showing;
  receivedEvents_.clear();
}

///************************* 開始条件 *************************///

void TutorialManager::UpdateTriggers() {
  for (std::size_t i = 0; i < triggerRuntimes_.size(); ++i) {
    if (stepStates_[i] != StepState::Waiting)
      continue;

    TutorialConditionRuntime &runtime = triggerRuntimes_[i];
    if (!runtime.HasDefinition())
      continue;

    // 開始条件の elapsed はチュートリアル開始からの経過秒を見る。
    // 決定入力で始まる開始条件は紛らわしいので受け付けない。
    runtime.Update(totalElapsed_, false);
    if (!runtime.IsSatisfied())
      continue;

    stepStates_[i] = StepState::Queued;
    pendingQueue_.push_back(i);
  }
}

bool TutorialManager::TryActivateNext() {
  // 条件が成立したステップを優先する。割り込みで教えたい内容のため。
  while (!pendingQueue_.empty()) {
    const std::size_t index = pendingQueue_.front();
    pendingQueue_.erase(pendingQueue_.begin());
    if (index >= stepStates_.size() || stepStates_[index] == StepState::Done)
      continue;
    EnterStep(index);
    return true;
  }

  // 次は開始条件を持たないステップを順番に。これが従来の線形進行にあたる。
  while (autoCursor_ < currentData_.steps.size()) {
    const std::size_t index = autoCursor_;
    if (stepStates_[index] == StepState::Waiting &&
        currentData_.steps[index].trigger.type == TutorialConditionType::None) {
      ++autoCursor_;
      EnterStep(index);
      return true;
    }
    // 条件待ちのステップは飛ばして先へ進む（起動はキュー経由で行われる）。
    if (stepStates_[index] == StepState::Done ||
        currentData_.steps[index].trigger.type != TutorialConditionType::None) {
      ++autoCursor_;
      continue;
    }
    break;
  }
  return false;
}

void TutorialManager::FinishCurrentStep() {
  if (currentStep_ < currentData_.steps.size() &&
      currentStep_ < stepStates_.size()) {
    const TutorialStep &step = currentData_.steps[currentStep_];
    stepStates_[currentStep_] = StepState::Done;
    if (step.once) {
      TutorialProgress::GetInstance()->MarkSeen(currentData_.name, step.name);
    }
  }
  hasActiveStep_ = false;
  ClearTargetHighlight();
  HideRuntimeUI();
  TutorialSpotlight::GetInstance()->Clear();
  ApplyGameplayPause(false);
  ReleaseGate();
  completeRuntime_.Clear();
}

bool TutorialManager::AllStepsDone() const {
  for (const StepState state : stepStates_) {
    if (state != StepState::Done)
      return false;
  }
  return true;
}

///************************* 入力ゲート *************************///

void TutorialManager::ApplyGate(const TutorialGate &gate) {
  if (!gate.enabled) {
    ReleaseGate();
    return;
  }
  InputActionMap::GetInstance()->SetExclusivelyEnabled(gate.allow);
  gateOwned_ = true;
}

void TutorialManager::ReleaseGate() {
  if (!gateOwned_)
    return;
  InputActionMap::GetInstance()->EnableAll();
  gateOwned_ = false;
}

void TutorialManager::Update() {
  if (!playing_)
    return;

  const float deltaTime = GameTime::GetDeltaTime(TimeChannel::UI);
  totalElapsed_ += deltaTime;

  // 待機中ステップの開始条件は、説明を表示しているかに関わらず毎フレーム評価する。
  UpdateTriggers();

  if (!hasActiveStep_) {
    if (!TryActivateNext()) {
      // 起動待ちのステップが残っている間は、何も表示せずチュートリアルを生かしておく。
      // 全て終わって初めて終了する。
      if (AllStepsDone())
        Stop();
      return;
    }
  }
  if (currentStep_ >= currentData_.steps.size())
    return;

  if (runtimeUIDirty_ || !UIManager::GetInstance()->HasUI(kPanelUIId) ||
      !UIManager::GetInstance()->HasUI(kTextUIId)) {
    RefreshRuntimeUI();
    runtimeUIDirty_ = false;
  }
  const TutorialStyle &style = currentData_.style;

  // 暗幕はページ送りのフェード中も動かし続ける（下の早期 return
  // より前に置く）。
  TutorialSpotlight::GetInstance()->Update(deltaTime);

  if (transitionPhase_ == TransitionPhase::FadeIn) {
    transitionElapsed_ += deltaTime;
    const float duration = std::max(0.0f, style.fadeInSeconds);
    transitionOpacity_ =
        duration > 0.0f ? std::clamp(transitionElapsed_ / duration, 0.0f, 1.0f)
                        : 1.0f;
    ApplyRuntimeOpacity();
    if (transitionOpacity_ >= 1.0f) {
      transitionPhase_ = TransitionPhase::Showing;
      transitionElapsed_ = 0.0f;
    }
    return;
  }

  if (transitionPhase_ == TransitionPhase::FadeOut) {
    transitionElapsed_ += deltaTime;
    const float duration = std::max(0.0f, style.fadeOutSeconds);
    const float t = duration > 0.0f
                        ? std::clamp(transitionElapsed_ / duration, 0.0f, 1.0f)
                        : 1.0f;
    transitionOpacity_ = transitionStartOpacity_ * (1.0f - t);
    ApplyRuntimeOpacity();
    if (t >= 1.0f)
      CompleteAdvance();
    return;
  }

  stepElapsed_ += deltaTime;

  const TutorialStep &step = currentData_.steps[currentStep_];
  const bool confirmTriggered = IsConfirmTriggered();

  // 完了条件（条件ツリー）が設定されていればそちらを優先する。
  // 未設定のステップは従来の waitType で進むため、既存の JSON はそのまま動く。
  bool completed = false;
  if (completeRuntime_.HasDefinition()) {
    completeRuntime_.Update(stepElapsed_, confirmTriggered);
    completed = completeRuntime_.IsSatisfied();
  } else {
    switch (step.waitType) {
    case TutorialWaitType::Seconds:
      completed = stepElapsed_ >= std::max(0.0f, step.waitSeconds);
      break;
    case TutorialWaitType::Event:
      completed =
          !step.eventName.empty() && receivedEvents_.contains(step.eventName);
      break;
    case TutorialWaitType::Confirm:
      completed = confirmTriggered;
      break;
    }
  }

  if (step.skippable && IsSkipTriggered()) {
    // 「スキップ」は現在の1ページ送りではなく、説明全体を閉じる。
    Stop();
    return;
  }
  if (completed)
    Advance();
}

void TutorialManager::NotifyEvent(const std::string &eventName) {
  RegisterEventName(eventName);
  if (eventName.empty())
    return;
  // 旧 API もシグナルとして流し、条件ツリーからも同じ名前で拾えるようにする。
  // 購読側で receivedEvents_ へも積まれるため、waitType=Event も従来通り動く。
  TutorialSignal::Emit(eventName);
  if (playing_)
    receivedEvents_.insert(eventName);
}

void TutorialManager::RegisterEventName(const std::string &eventName) {
  if (eventName.empty())
    return;
  if (std::find(knownEventNames_.begin(), knownEventNames_.end(), eventName) ==
      knownEventNames_.end()) {
    knownEventNames_.push_back(eventName);
    std::sort(knownEventNames_.begin(), knownEventNames_.end());
  }
}

void TutorialManager::Advance() {
  if (!playing_ || transitionPhase_ == TransitionPhase::FadeOut)
    return;
  const float fadeOutSeconds =
      std::max(0.0f, currentData_.style.fadeOutSeconds);
  if (fadeOutSeconds <= 0.0f) {
    CompleteAdvance();
    return;
  }
  transitionPhase_ = TransitionPhase::FadeOut;
  transitionElapsed_ = 0.0f;
  transitionStartOpacity_ = transitionOpacity_;
}

void TutorialManager::CompleteAdvance() {
  if (!playing_)
    return;
  FinishCurrentStep();

  // 次に出すものが無くても、開始条件待ちが残っていれば終了しない。
  if (!TryActivateNext() && AllStepsDone())
    Stop();
}

void TutorialManager::EnterStep(std::size_t index) {
  ClearTargetHighlight();
  currentStep_ = index;
  stepElapsed_ = 0.0f;
  transitionElapsed_ = 0.0f;
  transitionOpacity_ = currentData_.style.fadeInSeconds > 0.0f ? 0.0f : 1.0f;
  transitionStartOpacity_ = transitionOpacity_;
  transitionPhase_ = currentData_.style.fadeInSeconds > 0.0f
                         ? TransitionPhase::FadeIn
                         : TransitionPhase::Showing;
  receivedEvents_.clear();
  hasActiveStep_ = true;
  if (index < stepStates_.size())
    stepStates_[index] = StepState::Queued;

  const TutorialStep &step = currentData_.steps[currentStep_];
  // 完了条件の受信回数をこのステップ用に張り直す。
  completeRuntime_.Reset(step.complete);
  // 暗幕は説明パネルの1つ下に敷く。穴の下にあるUIや3Dの画をそのまま見せたいため。
  TutorialSpotlight::GetInstance()->Apply(step.spotlight,
                                          currentData_.style.layer - 1);
  ApplyGate(step.gate);
  ApplyGameplayPause(step.pauseGameplay);
  // Editor::Draw 中に Start される場合があるため、GPUテクスチャの更新は
  // Framework::Update 後に呼ばれる TutorialManager::Update まで遅延する。
  runtimeUIDirty_ = true;
  ApplyTargetHighlight();
}

void TutorialManager::RefreshRuntimeUI() {
  if (!playing_ || currentStep_ >= currentData_.steps.size())
    return;
  const TutorialStep &step = currentData_.steps[currentStep_];
  const TutorialStyle &style = currentData_.style;
  const TutorialStepLayout &layout = step.layout;

  std::string displayText;
  if (!step.speaker.empty())
    displayText = step.speaker + "\n";
  displayText += step.text;

  std::string hintText;
  if (style.showControlHint) {
    if (step.waitType == TutorialWaitType::Confirm)
      hintText = BuildConfirmHintText();
    else if (step.skippable)
      hintText = BuildSkipHintText();
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
  params.maxWidth = layout.textMaxWidth;
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

  UIManager *uiManager = UIManager::GetInstance();
  UIBase *panel = uiManager->GetUI(kPanelUIId);
  if (!panel) {
    auto created = std::make_unique<UIBase>(kPanelUIId);
    created->Initialize("");
    created->SetTransient(true);
    panel = created.get();
    uiManager->AddUI(kPanelUIId, std::move(created));
  }
  panel->SetAnchorPoint({0.5f, 0.5f});
  panel->SetTexture(style.panelTexturePath.empty()
                        ? "./Resources/images/white.png"
                        : style.panelTexturePath);
  panel->SetPosition({layout.panelPosition.x, layout.panelPosition.y, 0.0f});
  panel->SetSize(layout.panelSize);
  panel->SetColor(style.panelColor);
  panel->SetLayer(style.layer);
  panel->SetVisible(true);

  UIBase *text = uiManager->GetUI(kTextUIId);
  if (!text) {
    auto created = std::make_unique<UIBase>(kTextUIId);
    created->Initialize("");
    created->SetTransient(true);
    text = created.get();
    uiManager->AddUI(kTextUIId, std::move(created));
  }
  TextureManager::GetInstance()->ReloadTexture(kRuntimeTexturePath);
  text->SetTexture(kRuntimeTexturePath);
  text->SetAnchorPoint({0.5f, 0.5f});
  text->SetPosition({layout.panelPosition.x + layout.textOffset.x,
                     layout.panelPosition.y + layout.textOffset.y, 0.0f});
  text->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
  text->SetLayer(style.layer + 1);
  text->SetVisible(true);
  text->StopAnimation(UIAnimationType::FadeIn);

  UIBase *hintPanel = uiManager->GetUI(kHintPanelUIId);
  if (!hintPanel) {
    auto created = std::make_unique<UIBase>(kHintPanelUIId);
    created->Initialize("");
    created->SetTransient(true);
    hintPanel = created.get();
    uiManager->AddUI(kHintPanelUIId, std::move(created));
  }
  const Vector2 hintPosition{layout.panelPosition.x + layout.hintOffset.x,
                             layout.panelPosition.y + layout.hintOffset.y};
  hintPanel->SetAnchorPoint({0.5f, 0.5f});
  hintPanel->SetTexture(style.hintPanelTexturePath.empty()
                            ? "./Resources/images/white.png"
                            : style.hintPanelTexturePath);
  hintPanel->SetPosition({hintPosition.x, hintPosition.y, 0.0f});
  hintPanel->SetSize(layout.hintPanelSize);
  hintPanel->SetColor(style.hintPanelColor);
  hintPanel->SetLayer(style.layer + 2);
  hintPanel->SetVisible(!hintText.empty());

  UIBase *hint = uiManager->GetUI(kHintTextUIId);
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
  hint->SetAnchorPoint({0.5f, 0.5f});
  hint->SetPosition({hintPosition.x, hintPosition.y, 0.0f});
  hint->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
  hint->SetLayer(style.layer + 3);
  hint->SetVisible(!hintText.empty());
  hint->StopAnimation(UIAnimationType::FadeIn);

  // 前のページで使っていた追加UIをいったん隠し、現在のページ分だけ再設定する。
  for (std::size_t i = 0; i < activeAdditionalUICount_; ++i) {
    if (UIBase *additional = uiManager->GetUI(AdditionalUIId(i))) {
      additional->SetVisible(false);
    }
  }
  activeAdditionalUICount_ = step.additionalUIs.size();
  for (std::size_t i = 0; i < step.additionalUIs.size(); ++i) {
    const TutorialStepUI &data = step.additionalUIs[i];
    const std::string id = AdditionalUIId(i);
    UIBase *additional = uiManager->GetUI(id);
    if (!additional) {
      auto created = std::make_unique<UIBase>(id);
      created->Initialize("");
      created->SetTransient(true);
      additional = created.get();
      uiManager->AddUI(id, std::move(created));
    }
    if (!data.texturePath.empty()) {
      TextureManager::GetInstance()->LoadTexture(data.texturePath);
      additional->SetTexture(data.texturePath);
    }
    additional->SetName(data.name);
    additional->SetAnchorPoint(data.anchorPoint);
    additional->SetPosition({data.position.x, data.position.y, 0.0f});
    additional->SetSize(data.size);
    additional->SetColor(data.color);
    additional->SetLayer(style.layer + data.layerOffset);
    additional->SetVisible(!data.texturePath.empty());
    uiManager->BringToFront(id);
  }

  uiManager->BringToFront(kPanelUIId);
  uiManager->BringToFront(kTextUIId);
  uiManager->BringToFront(kHintPanelUIId);
  uiManager->BringToFront(kHintTextUIId);
  uiManager->SortByLayer();
  ApplyRuntimeOpacity();
  // TutorialManager は通常の UIManager::UpdateAll より後に更新されるため、
  // 新規生成したスプライトの頂点をこのフレーム分だけ即時更新する。
  panel->Update();
  text->Update();
  hintPanel->Update();
  hint->Update();
  for (std::size_t i = 0; i < activeAdditionalUICount_; ++i) {
    if (UIBase *additional = uiManager->GetUI(AdditionalUIId(i)))
      additional->Update();
  }
}

void TutorialManager::HideRuntimeUI() {
  UIManager *manager = UIManager::GetInstance();
  if (UIBase *panel = manager->GetUI(kPanelUIId))
    panel->SetVisible(false);
  if (UIBase *text = manager->GetUI(kTextUIId))
    text->SetVisible(false);
  if (UIBase *hintPanel = manager->GetUI(kHintPanelUIId))
    hintPanel->SetVisible(false);
  if (UIBase *hint = manager->GetUI(kHintTextUIId))
    hint->SetVisible(false);
  for (std::size_t i = 0; i < activeAdditionalUICount_; ++i) {
    if (UIBase *additional = manager->GetUI(AdditionalUIId(i)))
      additional->SetVisible(false);
  }
  activeAdditionalUICount_ = 0;
}

void TutorialManager::ApplyRuntimeOpacity() {
  if (!playing_ || currentStep_ >= currentData_.steps.size())
    return;
  UIManager *manager = UIManager::GetInstance();
  const TutorialStyle &style = currentData_.style;
  const TutorialStep &step = currentData_.steps[currentStep_];
  const float opacity = std::clamp(transitionOpacity_, 0.0f, 1.0f);

  auto applyColor = [](UIBase *ui, Vector4 color, float alpha) {
    if (!ui)
      return;
    color.w *= alpha;
    ui->SetColor(color);
    ui->Update();
  };
  applyColor(manager->GetUI(kPanelUIId), style.panelColor, opacity);
  applyColor(manager->GetUI(kTextUIId), {1.0f, 1.0f, 1.0f, 1.0f}, opacity);
  applyColor(manager->GetUI(kHintPanelUIId), style.hintPanelColor, opacity);
  applyColor(manager->GetUI(kHintTextUIId), {1.0f, 1.0f, 1.0f, 1.0f}, opacity);
  for (std::size_t i = 0; i < step.additionalUIs.size(); ++i) {
    applyColor(manager->GetUI(AdditionalUIId(i)), step.additionalUIs[i].color,
               opacity);
  }
}

void TutorialManager::ApplyTargetHighlight() {
  const std::string &targetId = currentData_.steps[currentStep_].targetUIId;
  if (targetId.empty() || targetId == kPanelUIId || targetId == kTextUIId ||
      targetId == kHintPanelUIId || targetId == kHintTextUIId)
    return;
  if (UIBase *target = UIManager::GetInstance()->GetUI(targetId)) {
    highlightedUIId_ = targetId;
    highlightedOriginalScale_ = target->GetScale();
    target->PlayPulse(1.12f, 0.55f, true);
    UIManager::GetInstance()->BringToFront(targetId);
  }
}

void TutorialManager::ClearTargetHighlight() {
  if (!highlightedUIId_.empty()) {
    if (UIBase *target = UIManager::GetInstance()->GetUI(highlightedUIId_)) {
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
  } else if (!pause && gameplayPauseOwned_) {
    GameTime::SetChannelPaused(TimeChannel::Gameplay, gameplayWasPaused_);
    gameplayPauseOwned_ = false;
  }
}

bool TutorialManager::IsConfirmTriggered() const {
  Input *input = Input::GetInstance();
  return input->TriggerKey(
             KeyboardKeyCode(currentData_.style.confirmKeyboardKey)) ||
         input->IsPadTriggered(
             0, GamepadButtonCode(currentData_.style.confirmGamepadButton));
}

bool TutorialManager::IsSkipTriggered() const {
  Input *input = Input::GetInstance();
  return input->TriggerKey(
             KeyboardKeyCode(currentData_.style.skipKeyboardKey)) ||
         input->IsPadTriggered(
             0, GamepadButtonCode(currentData_.style.skipGamepadButton));
}

std::string TutorialManager::BuildConfirmHintText() const {
  const TutorialStyle &style = currentData_.style;
  if (!style.autoBuildControlHint)
    return style.hintText;
  std::string text = "[" + style.confirmKeyboardKey + " / " +
                     style.confirmGamepadButton + "] 次へ";
  if (currentStep_ < currentData_.steps.size() &&
      currentData_.steps[currentStep_].skippable) {
    text += "    [" + style.skipKeyboardKey + " / " + style.skipGamepadButton +
            "] チュートリアルを閉じる";
  }
  return text;
}

std::string TutorialManager::BuildSkipHintText() const {
  const TutorialStyle &style = currentData_.style;
  if (!style.autoBuildControlHint)
    return style.skipHintText;
  return "[" + style.skipKeyboardKey + " / " + style.skipGamepadButton +
         "] チュートリアルを閉じる";
}

} // namespace YoRigine
