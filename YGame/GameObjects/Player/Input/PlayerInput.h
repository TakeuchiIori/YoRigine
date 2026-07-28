#pragma once

// Engine
#include "Systems/Input/InputActionMap.h"

// C++
#include <string>
#include <vector>

// ============================================================
// プレイヤー操作のアクション名
// 綴り間違いを防ぐため、文字列の定義はここへ集約する
// ============================================================
namespace PlayerAction {
inline constexpr const char *kMove = "Move";
inline constexpr const char *kLook = "Look";
inline constexpr const char *kRun = "Run";
inline constexpr const char *kAttackLight = "AttackLight";
inline constexpr const char *kAttackHeavy = "AttackHeavy";
inline constexpr const char *kGuard = "Guard";
inline constexpr const char *kStyleToggle = "StyleToggle";
inline constexpr const char *kLockOn = "LockOn";
} // namespace PlayerAction

// ============================================================
// プレイヤー用の入力窓口
//
// 生のキーコードやパッドボタンを直接触るのはこのクラスだけに閉じ込め、
// ゲーム側は「攻撃」「ガード」といった意味のある名前だけを扱う。
// 実体は InputActionMap（エンジン側の汎用マップ）への薄いラッパで、
// キーコンフィグやチュートリアルのゲートはそちらの仕組みに乗る。
// ============================================================
class PlayerInput {
public:
  // ============================================================
  // 基本関数
  // ============================================================

  // 既定バインドを登録したうえで、JSON があればそれで上書きする。
  // JSON が無い・壊れている場合は既定バインドのまま動作する。
  void Initialize();

  // ============================================================
  // ボタン入力（Triggered = 押した瞬間 / Held = 押しっぱなし）
  // ============================================================
  bool AttackLightTriggered() const;
  bool AttackLightHeld() const;
  bool AttackHeavyTriggered() const;
  bool AttackHeavyHeld() const;
  bool GuardTriggered() const;
  bool GuardHeld() const;
  bool StyleToggleTriggered() const;
  bool LockOnTriggered() const;
  bool RunHeld() const;

  // ============================================================
  // 軸入力
  // ============================================================
  YoRigine::InputAxisValue MoveAxis() const;
  YoRigine::InputAxisValue LookAxis() const;

  // 最後に操作されたデバイス。UIのボタン表示切り替えに使う。
  YoRigine::InputDeviceKind LastDevice() const;

  // ============================================================
  // 設定
  // ============================================================

  // 移動のデッドゾーンは MovementConfig を唯一の調整元にする。
  // 設定値が変わったらこれを呼んでバインド側へ同期する。
  void SetMoveDeadzone(float deadzone);

  // チュートリアルやカットシーン用のゲート。
  // allowed に挙げたアクションだけを受け付ける状態にする。
  void SetAllowedActions(const std::vector<std::string> &allowed);

  // ゲートを解除して全アクションを受け付ける状態へ戻す。
  void ClearActionGate();

private:
  // ============================================================
  // 内部処理関数
  // ============================================================

  // 現行の操作方法をそのままコードで表現した既定バインド。
  // JSON が無い環境でも操作不能にならないための保険を兼ねる。
  void RegisterDefaultBindings();

private:
  // ============================================================
  // メンバ変数
  // ============================================================
  YoRigine::InputActionMap *map_ = nullptr;
};
