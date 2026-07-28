#include "TutorialSignal.h"

#include <algorithm>

#include "Systems/Input/InputActionMap.h"

namespace {
// 入力アクション由来のシグナルに付ける接頭辞。
constexpr const char *kActionTriggeredPrefix = "action.triggered.";
} // namespace

namespace YoRigine {

///************************* 基本関数 *************************///

TutorialSignal *TutorialSignal::GetInstance() {
  static TutorialSignal instance;
  return &instance;
}

void TutorialSignal::Emit(const std::string &name) {
  if (name.empty())
    return;
  TutorialSignalData data;
  data.name = name;
  Emit(data);
}

void TutorialSignal::Emit(const TutorialSignalData &data) {
  if (data.name.empty())
    return;

  TutorialSignal *instance = GetInstance();
  instance->RegisterName(data.name);

  // 配信中に購読が追加・解除されても壊れないよう、スナップショットを回す。
  const std::vector<Entry> snapshot = instance->listeners_;
  for (const Entry &entry : snapshot) {
    if (entry.listener)
      entry.listener(data);
  }
}

///************************* 購読 *************************///

TutorialSignal::Handle TutorialSignal::Subscribe(Listener listener) {
  if (!listener)
    return 0;
  Entry entry;
  entry.handle = nextHandle_++;
  entry.listener = std::move(listener);
  listeners_.push_back(std::move(entry));
  return listeners_.back().handle;
}

void TutorialSignal::Unsubscribe(Handle handle) {
  if (handle == 0)
    return;
  listeners_.erase(std::remove_if(listeners_.begin(), listeners_.end(),
                                  [handle](const Entry &entry) {
                                    return entry.handle == handle;
                                  }),
                   listeners_.end());
}

///************************* 内蔵発火源 *************************///

void TutorialSignal::ConnectEngineSources() {
  if (engineSourcesConnected_)
    return;
  engineSourcesConnected_ = true;

  InputActionMap *actionMap = InputActionMap::GetInstance();

  // アクションが押された瞬間をシグナルへ変換する。
  // これだけで「攻撃ボタンを押す」チュートリアルがゲーム側の改修なしに作れる。
  // 注意: SetTriggerObserver
  // は1枠しかない。他の購読者が要る場合はここを分岐させること。
  actionMap->SetTriggerObserver([](const std::string &actionName) {
    Emit(ActionTriggeredName(actionName));
  });

  // 登録済みアクションの名前を、まだ押されていなくても候補として載せておく。
  // エディタで完了条件を選ぶときに一覧へ出したいため。
  for (const std::string &actionName : actionMap->GetActionNames()) {
    RegisterName(ActionTriggeredName(actionName));
  }
}

///************************* 名前の一覧 *************************///

void TutorialSignal::RegisterName(const std::string &name) {
  if (name.empty())
    return;
  const auto it =
      std::lower_bound(knownNames_.begin(), knownNames_.end(), name);
  if (it != knownNames_.end() && *it == name)
    return;
  knownNames_.insert(it, name);
}

std::string TutorialSignal::ActionTriggeredName(const std::string &actionName) {
  return std::string(kActionTriggeredPrefix) + actionName;
}

} // namespace YoRigine
