#include "TutorialCondition.h"

#include <algorithm>

namespace YoRigine {

///************************* 評価状態 *************************///

void TutorialConditionRuntime::Reset(const TutorialCondition &condition) {
  definition_ = &condition;
  receivedCount_ = 0;
  latched_ = false;

  children_.clear();
  children_.resize(condition.children.size());
  for (std::size_t i = 0; i < condition.children.size(); ++i) {
    children_[i].Reset(condition.children[i]);
  }
}

void TutorialConditionRuntime::Clear() {
  definition_ = nullptr;
  receivedCount_ = 0;
  latched_ = false;
  children_.clear();
}

void TutorialConditionRuntime::OnSignal(const TutorialSignalData &signal) {
  if (!definition_)
    return;

  if (definition_->type == TutorialConditionType::Signal &&
      !definition_->signalName.empty() &&
      definition_->signalName == signal.name) {
    ++receivedCount_;
  }

  for (TutorialConditionRuntime &child : children_) {
    child.OnSignal(signal);
  }
}

void TutorialConditionRuntime::Update(float elapsedSeconds,
                                      bool confirmTriggered) {
  if (!definition_)
    return;

  switch (definition_->type) {
  case TutorialConditionType::Elapsed:
    if (elapsedSeconds >= definition_->seconds)
      latched_ = true;
    break;
  case TutorialConditionType::Confirm:
    // 決定入力は押した瞬間しか来ないので、成立を保持しておく。
    if (confirmTriggered)
      latched_ = true;
    break;
  case TutorialConditionType::None:
  case TutorialConditionType::Signal:
  case TutorialConditionType::All:
  case TutorialConditionType::Any:
  case TutorialConditionType::Not:
    break;
  default:
    break;
  }

  for (TutorialConditionRuntime &child : children_) {
    child.Update(elapsedSeconds, confirmTriggered);
  }
}

bool TutorialConditionRuntime::IsSatisfied() const {
  if (!definition_)
    return false;

  switch (definition_->type) {
  case TutorialConditionType::Signal:
    if (definition_->signalName.empty())
      return false;
    return receivedCount_ >= (std::max)(1, definition_->requiredCount);

  case TutorialConditionType::Elapsed:
  case TutorialConditionType::Confirm:
    return latched_;

  case TutorialConditionType::All:
    // 子が空の All を「常に成立」にすると、設定漏れのステップが一瞬で
    // 飛んでしまう。空は不成立として扱う。
    if (children_.empty())
      return false;
    return std::all_of(children_.begin(), children_.end(),
                       [](const TutorialConditionRuntime &child) {
                         return child.IsSatisfied();
                       });

  case TutorialConditionType::Any:
    if (children_.empty())
      return false;
    return std::any_of(children_.begin(), children_.end(),
                       [](const TutorialConditionRuntime &child) {
                         return child.IsSatisfied();
                       });

  case TutorialConditionType::Not:
    if (children_.empty())
      return false;
    return !children_.front().IsSatisfied();

  case TutorialConditionType::None:
    return false;

  default:
    return false;
  }
}

bool TutorialConditionRuntime::HasDefinition() const {
  return definition_ != nullptr &&
         definition_->type != TutorialConditionType::None;
}

///************************* 名前変換 *************************///

const char *TutorialConditionTypeName(TutorialConditionType type) {
  switch (type) {
  case TutorialConditionType::Signal:
    return "signal";
  case TutorialConditionType::Elapsed:
    return "elapsed";
  case TutorialConditionType::Confirm:
    return "confirm";
  case TutorialConditionType::All:
    return "all";
  case TutorialConditionType::Any:
    return "any";
  case TutorialConditionType::Not:
    return "not";
  case TutorialConditionType::None:
    return "none";
  default:
    return "none";
  }
}

TutorialConditionType TutorialConditionTypeFromName(const std::string &name) {
  if (name == "signal")
    return TutorialConditionType::Signal;
  if (name == "elapsed")
    return TutorialConditionType::Elapsed;
  if (name == "confirm")
    return TutorialConditionType::Confirm;
  if (name == "all")
    return TutorialConditionType::All;
  if (name == "any")
    return TutorialConditionType::Any;
  if (name == "not")
    return TutorialConditionType::Not;
  return TutorialConditionType::None;
}

} // namespace YoRigine
