#include "PlayerInput.h"

namespace {
// キーコンフィグの保存先。存在しない場合は既定バインドで動作する。
constexpr const char *kBindingPath = "Resources/Json/Input/PlayerActions.json";
} // namespace

// ============================================================
// 初期化
// ============================================================
void PlayerInput::Initialize() {
  map_ = YoRigine::InputActionMap::GetInstance();

  RegisterDefaultBindings();

  // JSON があればユーザー設定として上書きする。
  // 失敗しても既定バインドが残るので、操作不能にはならない。
  map_->LoadFromFile(kBindingPath);
}

// ============================================================
// 既定バインド登録
// ============================================================
void PlayerInput::RegisterDefaultBindings() {
  using YoRigine::InputActionBinding;
  using YoRigine::InputAxisBinding;
  using YoRigine::InputStickKind;

  // ------------------------------------------------------------
  // ボタン
  // ------------------------------------------------------------
  InputActionBinding attackLight;
  attackLight.name = PlayerAction::kAttackLight;
  attackLight.padButtons = {GamePadButton::A};
  map_->AddAction(attackLight);

  InputActionBinding attackHeavy;
  attackHeavy.name = PlayerAction::kAttackHeavy;
  attackHeavy.padButtons = {GamePadButton::B};
  map_->AddAction(attackHeavy);

  InputActionBinding guard;
  guard.name = PlayerAction::kGuard;
  guard.padButtons = {GamePadButton::X};
  guard.keys = {DIK_N};
  map_->AddAction(guard);

  InputActionBinding styleToggle;
  styleToggle.name = PlayerAction::kStyleToggle;
  styleToggle.padButtons = {GamePadButton::Y};
  map_->AddAction(styleToggle);

  InputActionBinding lockOn;
  lockOn.name = PlayerAction::kLockOn;
  lockOn.padButtons = {GamePadButton::R_Stick};
  map_->AddAction(lockOn);

  // 走りはキーボードのみ。ゲームパッドはスティックの倒し量で判定するため
  // ボタンを割り当てない（PlayerMovement の analogRunThreshold が担当）。
  InputActionBinding run;
  run.name = PlayerAction::kRun;
  run.keys = {DIK_LSHIFT};
  map_->AddAction(run);

  // ------------------------------------------------------------
  // 軸
  // ------------------------------------------------------------
  InputAxisBinding move;
  move.name = PlayerAction::kMove;
  move.stick = InputStickKind::Left;
  move.keyUp = DIK_W;
  move.keyDown = DIK_S;
  move.keyLeft = DIK_A;
  move.keyRight = DIK_D;
  map_->AddAxis(move);

  InputAxisBinding look;
  look.name = PlayerAction::kLook;
  look.stick = InputStickKind::Right;
  map_->AddAxis(look);
}

// ============================================================
// ボタン入力
// ============================================================
bool PlayerInput::AttackLightTriggered() const {
  return map_ && map_->IsTriggered(PlayerAction::kAttackLight);
}

bool PlayerInput::AttackLightHeld() const {
  return map_ && map_->IsPressed(PlayerAction::kAttackLight);
}

bool PlayerInput::AttackHeavyTriggered() const {
  return map_ && map_->IsTriggered(PlayerAction::kAttackHeavy);
}

bool PlayerInput::AttackHeavyHeld() const {
  return map_ && map_->IsPressed(PlayerAction::kAttackHeavy);
}

bool PlayerInput::GuardTriggered() const {
  return map_ && map_->IsTriggered(PlayerAction::kGuard);
}

bool PlayerInput::GuardHeld() const {
  return map_ && map_->IsPressed(PlayerAction::kGuard);
}

bool PlayerInput::StyleToggleTriggered() const {
  return map_ && map_->IsTriggered(PlayerAction::kStyleToggle);
}

bool PlayerInput::LockOnTriggered() const {
  return map_ && map_->IsTriggered(PlayerAction::kLockOn);
}

bool PlayerInput::RunHeld() const {
  return map_ && map_->IsPressed(PlayerAction::kRun);
}

// ============================================================
// 軸入力
// ============================================================
YoRigine::InputAxisValue PlayerInput::MoveAxis() const {
  if (!map_)
    return YoRigine::InputAxisValue{};
  return map_->GetAxis(PlayerAction::kMove);
}

YoRigine::InputAxisValue PlayerInput::LookAxis() const {
  if (!map_)
    return YoRigine::InputAxisValue{};
  return map_->GetAxis(PlayerAction::kLook);
}

YoRigine::InputDeviceKind PlayerInput::LastDevice() const {
  if (!map_)
    return YoRigine::InputDeviceKind::Keyboard;
  return map_->LastDevice();
}

// ============================================================
// 設定
// ============================================================
void PlayerInput::SetMoveDeadzone(float deadzone) {
  if (!map_)
    return;
  map_->SetAxisDeadzone(PlayerAction::kMove, deadzone);
}

void PlayerInput::SetAllowedActions(const std::vector<std::string> &allowed) {
  if (!map_)
    return;
  map_->SetExclusivelyEnabled(allowed);
}

void PlayerInput::ClearActionGate() {
  if (!map_)
    return;
  map_->EnableAll();
}
