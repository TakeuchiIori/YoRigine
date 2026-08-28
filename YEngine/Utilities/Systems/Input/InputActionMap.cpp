#include "InputActionMap.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include <json.hpp>

namespace {

// スティック入力を「動いている」とみなす下限。
// PlayerMovement が従来使っていた閾値をそのまま引き継いでいる。
constexpr float kAxisEpsilon = 0.01f;

// 名前とキーコードの対応表。JSON とエディタの双方から引く。
struct KeyNameEntry {
  const char *name;
  BYTE code;
};

constexpr KeyNameEntry kKeyNames[] = {
    {"A", DIK_A},
    {"B", DIK_B},
    {"C", DIK_C},
    {"D", DIK_D},
    {"E", DIK_E},
    {"F", DIK_F},
    {"G", DIK_G},
    {"H", DIK_H},
    {"I", DIK_I},
    {"J", DIK_J},
    {"K", DIK_K},
    {"L", DIK_L},
    {"M", DIK_M},
    {"N", DIK_N},
    {"O", DIK_O},
    {"P", DIK_P},
    {"Q", DIK_Q},
    {"R", DIK_R},
    {"S", DIK_S},
    {"T", DIK_T},
    {"U", DIK_U},
    {"V", DIK_V},
    {"W", DIK_W},
    {"X", DIK_X},
    {"Y", DIK_Y},
    {"Z", DIK_Z},
    {"0", DIK_0},
    {"1", DIK_1},
    {"2", DIK_2},
    {"3", DIK_3},
    {"4", DIK_4},
    {"5", DIK_5},
    {"6", DIK_6},
    {"7", DIK_7},
    {"8", DIK_8},
    {"9", DIK_9},
    {"SPACE", DIK_SPACE},
    {"ENTER", DIK_RETURN},
    {"ESC", DIK_ESCAPE},
    {"TAB", DIK_TAB},
    {"BACKSPACE", DIK_BACK},
    {"LSHIFT", DIK_LSHIFT},
    {"RSHIFT", DIK_RSHIFT},
    {"LCTRL", DIK_LCONTROL},
    {"RCTRL", DIK_RCONTROL},
    {"LALT", DIK_LMENU},
    {"RALT", DIK_RMENU},
    {"UP", DIK_UP},
    {"DOWN", DIK_DOWN},
    {"LEFT", DIK_LEFT},
    {"RIGHT", DIK_RIGHT},
    {"F1", DIK_F1},
    {"F2", DIK_F2},
    {"F3", DIK_F3},
    {"F4", DIK_F4},
    {"F5", DIK_F5},
    {"F6", DIK_F6},
    {"F7", DIK_F7},
    {"F8", DIK_F8},
    {"F9", DIK_F9},
    {"F10", DIK_F10},
    {"F11", DIK_F11},
    {"F12", DIK_F12},
};

struct PadNameEntry {
  const char *name;
  GamePadButton button;
};

constexpr PadNameEntry kPadNames[] = {
    {"A", GamePadButton::A},
    {"B", GamePadButton::B},
    {"X", GamePadButton::X},
    {"Y", GamePadButton::Y},
    {"LB", GamePadButton::LB},
    {"RB", GamePadButton::RB},
    {"START", GamePadButton::Start},
    {"BACK", GamePadButton::Back},
    {"L_STICK", GamePadButton::L_Stick},
    {"R_STICK", GamePadButton::R_Stick},
    {"DPAD_UP", GamePadButton::DPad_Up},
    {"DPAD_DOWN", GamePadButton::DPad_Down},
    {"DPAD_LEFT", GamePadButton::DPad_Left},
    {"DPAD_RIGHT", GamePadButton::DPad_Right},
};

// PlayerMovement が従来使っていたデッドゾーン処理と同じ式。
// 閾値未満は 0 に落とし、超えた分を 0〜1 へ引き伸ばす。
float ApplyDeadzone(float value, float deadzone) {
  if (std::abs(value) < deadzone)
    return 0.0f;
  const float sign = (value > 0.0f) ? 1.0f : -1.0f;
  return sign * ((std::abs(value) - deadzone) / (1.0f - deadzone));
}

const char *StickName(YoRigine::InputStickKind stick) {
  switch (stick) {
  case YoRigine::InputStickKind::Left:
    return "Left";
  case YoRigine::InputStickKind::Right:
    return "Right";
  case YoRigine::InputStickKind::None:
    break;
  default:
    break;
  }
  return "None";
}

YoRigine::InputStickKind StickFromName(const std::string &name) {
  if (name == "Left")
    return YoRigine::InputStickKind::Left;
  if (name == "Right")
    return YoRigine::InputStickKind::Right;
  return YoRigine::InputStickKind::None;
}

} // namespace

namespace YoRigine {

///************************* 基本関数 *************************///

InputActionMap *InputActionMap::GetInstance() {
  static InputActionMap instance;
  return &instance;
}

void InputActionMap::Update() {
  bool gamepadUsed = false;
  bool keyboardUsed = false;

  // ------------------------------------------------------------
  // ボタン系アクション。押下状態をキャッシュし、立ち上がりを通知する。
  // ------------------------------------------------------------
  for (const std::string &name : actionNames_) {
    const auto bindingIt = actions_.find(name);
    if (bindingIt == actions_.end())
      continue;

    ActionState &state = actionStates_[name];
    state.previous = state.pressed;

    bool fromGamepad = false;
    state.pressed = EvaluateAction(bindingIt->second, fromGamepad);

    if (state.pressed) {
      if (fromGamepad)
        gamepadUsed = true;
      else
        keyboardUsed = true;
    }

    // ゲートで塞がれているアクションは、押されても外部へ通知しない。
    if (state.pressed && !state.previous && triggerObserver_ &&
        IsEnabled(name)) {
      triggerObserver_(name, EventKind::ActionTriggered);
    }
  }

  // ------------------------------------------------------------
  // 軸。こちらもフレーム内で値が揺れないようキャッシュする。
  // ------------------------------------------------------------
  for (const std::string &name : axisNames_) {
    const auto bindingIt = axes_.find(name);
    if (bindingIt == axes_.end())
      continue;

    const InputAxisValue value = EvaluateAxis(bindingIt->second);
    axisValues_[name] = value;

    if (value.magnitude > 0.0f) {
      if (value.device == InputDeviceKind::Gamepad)
        gamepadUsed = true;
      else
        keyboardUsed = true;
    }

    // 倒し量が閾値を跨いだ瞬間だけ「倒した」として通知する。
    // 押しっぱなしで毎フレーム流れないよう、前フレームの値と比較している。
    float &previousMagnitude = axisPreviousMagnitude_[name];
    const float threshold =
        std::clamp(bindingIt->second.signalThreshold, 0.01f, 1.0f);
    if (previousMagnitude <= threshold && value.magnitude > threshold &&
        triggerObserver_ && IsEnabled(name)) {
      triggerObserver_(name, EventKind::AxisEngaged);
    }
    previousMagnitude = value.magnitude;
  }

  // 同時に触られた場合はゲームパッドを優先する。
  // 入力が全く無いフレームでは直前のデバイスを保持する。
  if (gamepadUsed)
    lastDevice_ = InputDeviceKind::Gamepad;
  else if (keyboardUsed)
    lastDevice_ = InputDeviceKind::Keyboard;
}

///************************* バインド登録 *************************///

void InputActionMap::AddAction(const InputActionBinding &binding) {
  if (binding.name.empty())
    return;
  if (actions_.find(binding.name) == actions_.end()) {
    actionNames_.push_back(binding.name);
    actionStates_.emplace(binding.name, ActionState{});
  }
  actions_[binding.name] = binding;
}

void InputActionMap::AddAxis(const InputAxisBinding &binding) {
  if (binding.name.empty())
    return;
  if (axes_.find(binding.name) == axes_.end()) {
    axisNames_.push_back(binding.name);
    axisValues_.emplace(binding.name, InputAxisValue{});
    axisPreviousMagnitude_.emplace(binding.name, 0.0f);
  }
  axes_[binding.name] = binding;
}

void InputActionMap::Clear() {
  actions_.clear();
  axes_.clear();
  actionStates_.clear();
  axisValues_.clear();
  axisPreviousMagnitude_.clear();
  actionNames_.clear();
  axisNames_.clear();
  disabled_.clear();
}

void InputActionMap::SetAxisDeadzone(std::string_view axis, float deadzone) {
  const auto it = axes_.find(axis);
  if (it == axes_.end())
    return;
  it->second.deadzone = std::clamp(deadzone, 0.0f, 0.95f);
}

///************************* 問い合わせ *************************///

bool InputActionMap::IsPressed(std::string_view action) const {
  if (!IsEnabled(action))
    return false;
  const auto it = actionStates_.find(action);
  return it != actionStates_.end() && it->second.pressed;
}

bool InputActionMap::IsTriggered(std::string_view action) const {
  if (!IsEnabled(action))
    return false;
  const auto it = actionStates_.find(action);
  return it != actionStates_.end() && it->second.pressed &&
         !it->second.previous;
}

bool InputActionMap::IsReleased(std::string_view action) const {
  if (!IsEnabled(action))
    return false;
  const auto it = actionStates_.find(action);
  return it != actionStates_.end() && !it->second.pressed &&
         it->second.previous;
}

InputAxisValue InputActionMap::GetAxis(std::string_view axis) const {
  if (!IsEnabled(axis))
    return InputAxisValue{};
  const auto it = axisValues_.find(axis);
  if (it == axisValues_.end())
    return InputAxisValue{};
  return it->second;
}

///************************* ゲート *************************///

void InputActionMap::SetEnabled(std::string_view name, bool enabled) {
  if (enabled) {
    const auto it = disabled_.find(name);
    if (it != disabled_.end())
      disabled_.erase(it);
  } else {
    disabled_.emplace(name);
  }
}

bool InputActionMap::IsEnabled(std::string_view name) const {
  return disabled_.find(name) == disabled_.end();
}

void InputActionMap::EnableAll() { disabled_.clear(); }

void InputActionMap::SetExclusivelyEnabled(
    const std::vector<std::string> &allowed) {
  disabled_.clear();
  auto blockUnlisted = [&](const std::vector<std::string> &names) {
    for (const std::string &name : names) {
      const bool isAllowed =
          std::find(allowed.begin(), allowed.end(), name) != allowed.end();
      if (!isAllowed)
        disabled_.emplace(name);
    }
  };
  blockUnlisted(actionNames_);
  blockUnlisted(axisNames_);
}

///************************* 検索 *************************///

const InputActionBinding *
InputActionMap::FindAction(std::string_view name) const {
  const auto it = actions_.find(name);
  return it != actions_.end() ? &it->second : nullptr;
}

const InputAxisBinding *InputActionMap::FindAxis(std::string_view name) const {
  const auto it = axes_.find(name);
  return it != axes_.end() ? &it->second : nullptr;
}

///************************* 名前変換 *************************///

BYTE InputActionMap::KeyCodeFromName(std::string_view name) {
  for (const KeyNameEntry &entry : kKeyNames) {
    if (name == entry.name)
      return entry.code;
  }
  return 0;
}

const char *InputActionMap::KeyNameFromCode(BYTE code) {
  for (const KeyNameEntry &entry : kKeyNames) {
    if (entry.code == code)
      return entry.name;
  }
  return "";
}

bool InputActionMap::PadButtonFromName(std::string_view name,
                                       GamePadButton &out) {
  for (const PadNameEntry &entry : kPadNames) {
    if (name == entry.name) {
      out = entry.button;
      return true;
    }
  }
  return false;
}

const char *InputActionMap::PadButtonName(GamePadButton button) {
  for (const PadNameEntry &entry : kPadNames) {
    if (entry.button == button)
      return entry.name;
  }
  return "";
}

///************************* JSON *************************///

bool InputActionMap::LoadFromFile(const std::string &path) {
  if (path.empty() || !std::filesystem::exists(path))
    return false;

  try {
    std::ifstream file(path);
    if (!file.is_open())
      return false;

    nlohmann::json root;
    file >> root;

    for (const nlohmann::json &item :
         root.value("actions", nlohmann::json::array())) {
      InputActionBinding binding;
      binding.name = item.value("name", std::string());
      if (binding.name.empty())
        continue;

      for (const nlohmann::json &key :
           item.value("key", nlohmann::json::array())) {
        const BYTE code = KeyCodeFromName(key.get<std::string>());
        if (code != 0)
          binding.keys.push_back(code);
      }
      for (const nlohmann::json &pad :
           item.value("pad", nlohmann::json::array())) {
        const std::string padName = pad.get<std::string>();
        // LT / RT はボタンビットではなくアナログトリガーなので別扱いにする。
        if (padName == "LT") {
          binding.leftTrigger = true;
          continue;
        }
        if (padName == "RT") {
          binding.rightTrigger = true;
          continue;
        }

        GamePadButton button = GamePadButton::A;
        if (PadButtonFromName(padName, button))
          binding.padButtons.push_back(button);
      }
      AddAction(binding);
    }

    for (const nlohmann::json &item :
         root.value("axes", nlohmann::json::array())) {
      InputAxisBinding binding;
      binding.name = item.value("name", std::string());
      if (binding.name.empty())
        continue;

      binding.stick = StickFromName(item.value("stick", std::string("None")));
      binding.deadzone = item.value("deadzone", binding.deadzone);
      binding.signalThreshold =
          item.value("signalThreshold", binding.signalThreshold);
      binding.keyUp = KeyCodeFromName(item.value("keyUp", std::string()));
      binding.keyDown = KeyCodeFromName(item.value("keyDown", std::string()));
      binding.keyLeft = KeyCodeFromName(item.value("keyLeft", std::string()));
      binding.keyRight = KeyCodeFromName(item.value("keyRight", std::string()));
      AddAxis(binding);
    }
    return true;
  } catch (...) {
    // 壊れた設定ファイルで操作不能にならないよう、既定バインドを残したまま失敗を返す。
    return false;
  }
}

bool InputActionMap::SaveToFile(const std::string &path) const {
  if (path.empty())
    return false;

  try {
    nlohmann::json root;
    root["version"] = 1;
    root["actions"] = nlohmann::json::array();
    root["axes"] = nlohmann::json::array();

    for (const std::string &name : actionNames_) {
      const auto it = actions_.find(name);
      if (it == actions_.end())
        continue;
      const InputActionBinding &binding = it->second;

      nlohmann::json item;
      item["name"] = binding.name;
      item["key"] = nlohmann::json::array();
      item["pad"] = nlohmann::json::array();
      for (const BYTE code : binding.keys) {
        const char *keyName = KeyNameFromCode(code);
        if (keyName[0] != '\0')
          item["key"].push_back(keyName);
      }
      for (const GamePadButton button : binding.padButtons) {
        const char *padName = PadButtonName(button);
        if (padName[0] != '\0')
          item["pad"].push_back(padName);
      }
      if (binding.leftTrigger)
        item["pad"].push_back("LT");
      if (binding.rightTrigger)
        item["pad"].push_back("RT");
      root["actions"].push_back(item);
    }

    for (const std::string &name : axisNames_) {
      const auto it = axes_.find(name);
      if (it == axes_.end())
        continue;
      const InputAxisBinding &binding = it->second;

      nlohmann::json item;
      item["name"] = binding.name;
      item["stick"] = StickName(binding.stick);
      item["deadzone"] = binding.deadzone;
      item["signalThreshold"] = binding.signalThreshold;
      item["keyUp"] = KeyNameFromCode(binding.keyUp);
      item["keyDown"] = KeyNameFromCode(binding.keyDown);
      item["keyLeft"] = KeyNameFromCode(binding.keyLeft);
      item["keyRight"] = KeyNameFromCode(binding.keyRight);
      root["axes"].push_back(item);
    }

    std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
      std::filesystem::create_directories(filePath.parent_path());
    }
    std::ofstream file(path);
    if (!file.is_open())
      return false;
    file << root.dump(4);
    return true;
  } catch (...) {
    return false;
  }
}

///************************* 内部処理 *************************///

bool InputActionMap::EvaluateAction(const InputActionBinding &binding,
                                    bool &outFromGamepad) const {
  Input *input = Input::GetInstance();
  outFromGamepad = false;

  for (const BYTE key : binding.keys) {
    if (key != 0 && input->PushKey(key))
      return true;
  }

  if (!Input::IsControllerConnected())
    return false;

  for (const GamePadButton button : binding.padButtons) {
    if (input->IsPadPressed(playerIndex_, button)) {
      outFromGamepad = true;
      return true;
    }
  }
  if (binding.leftTrigger && input->IsLTPressed(playerIndex_)) {
    outFromGamepad = true;
    return true;
  }
  if (binding.rightTrigger && input->IsRTPressed(playerIndex_)) {
    outFromGamepad = true;
    return true;
  }
  return false;
}

InputAxisValue
InputActionMap::EvaluateAxis(const InputAxisBinding &binding) const {
  Input *input = Input::GetInstance();
  InputAxisValue result;

  // ------------------------------------------------------------
  // スティックを優先。デッドゾーンを超えていればその値を採用する。
  // ------------------------------------------------------------
  if (binding.stick != InputStickKind::None && Input::IsControllerConnected()) {
    const bool isLeft = (binding.stick == InputStickKind::Left);
    const float rawX = isLeft ? input->GetLeftStickX(playerIndex_)
                              : input->GetRightStickX(playerIndex_);
    const float rawY = isLeft ? input->GetLeftStickY(playerIndex_)
                              : input->GetRightStickY(playerIndex_);

    const float x = ApplyDeadzone(rawX, binding.deadzone);
    const float y = ApplyDeadzone(rawY, binding.deadzone);
    const float magnitude = std::min(std::sqrt(x * x + y * y), 1.0f);

    if (magnitude > kAxisEpsilon) {
      result.raw = {rawX, rawY};
      result.value = {x, y};
      result.magnitude = magnitude;
      result.device = InputDeviceKind::Gamepad;
      result.isAnalog = true;
      return result;
    }
  }

  // ------------------------------------------------------------
  // キーボード。押された方向を合成して正規化する（デッドゾーンは掛けない）。
  // ------------------------------------------------------------
  Vector2 direction{0.0f, 0.0f};
  if (binding.keyUp != 0 && input->PushKey(binding.keyUp))
    direction.y += 1.0f;
  if (binding.keyDown != 0 && input->PushKey(binding.keyDown))
    direction.y -= 1.0f;
  if (binding.keyLeft != 0 && input->PushKey(binding.keyLeft))
    direction.x -= 1.0f;
  if (binding.keyRight != 0 && input->PushKey(binding.keyRight))
    direction.x += 1.0f;

  const float length =
      std::sqrt(direction.x * direction.x + direction.y * direction.y);
  if (length > 0.0f) {
    direction.x /= length;
    direction.y /= length;
    result.magnitude = 1.0f;
  }
  result.raw = direction;
  result.value = direction;
  result.device = InputDeviceKind::Keyboard;
  return result;
}

} // namespace YoRigine
