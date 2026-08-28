#pragma once

// App
#include "../StateMachine.h"
#include "MovementTypes.h"

// Engine
#include "WorldTransform/WorldTransform.h"
#include <Loaders/Json/JsonManager.h>
// C++
#include <functional>

class Player;

// ============================================================
// プレイヤーの移動管理クラス
// 入力の取得、歩行/走行の速度計算、モデルの回転、カメラ基準移動やステップ補間を担当する
// ============================================================
class PlayerMovement {
public:
  // ============================================================
  // 初期化と更新処理
  // ============================================================
  PlayerMovement(Player *owner);

  void Update(float deltaTime);
  void InitJson(YoRigine::JsonManager *jsonManager);
  void ShowStateDebug();
  const char *GetStateString(MovementState state) const;

  // ============================================================
  // StateMachine関連
  // ============================================================
  void ChangeState(MovementState newState) {
    stateMachine_.ChangeState(newState);
  }
  bool CanTransitionTo([[maybe_unused]] MovementState newState) const;

  MovementState GetCurrentState() const {
    return stateMachine_.GetCurrentState();
  }
  MovementState GetPreviousState() const {
    return stateMachine_.GetPreviousState();
  }
  bool StateChanged() const { return stateMachine_.StateChanged(); }

  // 戦闘アクション終了時に呼ばれる。現在の移動ステートに応じたアニメーションを再生する
  void SyncAnimationToCurrentState();

  // ============================================================
  // コールバック設定
  // ============================================================
  void SetInputTypeChangeCallback(std::function<void(InputType)> callback) {
    onInputTypeChanged_ = callback;
  }

  // ============================================================
  // 移動と回転のアクセッサ・状態操作
  // ============================================================
  bool IsRotating() const { return isRotating_; }
  void SetIsRotating(bool isRotating) { isRotating_ = isRotating; }
  bool CanRotate() const { return canRotate_; }
  void SetCanRotate(bool canRotate) { canRotate_ = canRotate; }

  float GetCurrentRotate() const { return currentRotateY_; }
  Vector3 GetForwardDirection() const;

  Vector3 GetVelocity() const { return velocity_; }
  float GetSpeed() const;
  bool IsMoving() const;

  bool CanMove() const { return canMove_; }
  void SetCanMove(bool canMove) { canMove_ = canMove; }
  void ForceStop();

  const MovementConfig &GetConfig() const { return config_; }
  // 「右スティックでカメラを回すとプレイヤーも追従して回る」フラグのポインタ。
  // カメラエディタ（PlayerCamera）から ON/OFF を切り替えられるよう公開する。
  bool *GetCameraFollowEnabledPtr() { return &config_.enableCameraFollow; }
  InputType GetCurrentInputType() const { return lastInputType_; }
  Player *GetOwner() const { return owner_; }

  // ============================================================
  // 各Stateから呼ばれる処理
  // ============================================================
  InputState GetInputState() const;
  void DetectInputType(const InputState &input);

  void UpdateRotate(float deltaTime, const Vector3 &moveDirection);
  void ApplyRotate();

  void ApplyMovement(float deltaTime);

  // ============================================================
  // 攻撃時のステップ補間
  // ============================================================
  void RequestAttackStep(const Vector3 &targetPosition, float stepDistance);
  bool IsAttackStepping() const { return isAttackStepping_; }

  // ============================================================
  // オートホーミング（位置 + 向き Slerp）
  // 攻撃発動時に、指定ターゲット方向へ向きと位置をなめらかに補正する
  // ============================================================
  void RequestAutoHoming(const Vector3 &targetPosition, float duration,
                         float maxStep);
  bool IsHoming() const { return isHoming_; }

  // ============================================================
  // 内部データへの参照取得（State更新用）
  // ============================================================
  Vector3 &GetVelocityRef() { return velocity_; }
  Vector3 &GetTargetDirectionRef() { return targetDirection_; }
  float &GetStateTimerRef() { return stateTimer_; }

  bool IsRunning() const {
    return GetCurrentState() == MovementState::Moving &&
           velocity_.Length() > config_.walkSpeed + 0.01f;
  }

  // ============================================================
  // カメラ計算・追従処理
  // ============================================================
  Vector3 CameraMoveDir(const Vector3 &inputDirection,
                        const Vector3 &cameraRotation);
  Vector3 GetCameraRotation() const;
  void UpdateCameraFollow(float deltaTime);
  void UpdateWaypointFacing(float deltaTime);
  bool FaceCurrentWaypointNow();
  bool IsCameraMoving() const { return isCameraMoving_; }

private:
  // ============================================================
  // 内部処理
  // ============================================================
  void InitializeStateMachine();

  float CalculateTargetRotate(const Vector3 &direction) const;
  float LerpAngle(float from, float to, float t) const;

private:
  // ============================================================
  // メンバ変数
  // ============================================================

  // ------------------------------------------------------------
  // システム連携・オーナー参照
  // ------------------------------------------------------------
  Player *owner_; // このコンポーネントを所有するプレイヤー
  StateMachine<MovementState>
      stateMachine_; // 移動状態（待機、移動中など）を管理するステートマシン
  MovementConfig config_; // JSONから読み込む移動速度や回転速度などの設定データ

  // ------------------------------------------------------------
  // 移動用パラメータ
  // ------------------------------------------------------------
  Vector3 velocity_{0.0f, 0.0f, 0.0f}; // 現在の移動速度ベクトル
  Vector3 targetDirection_{
      0.0f, 0.0f, 0.0f}; // 入力などによって決定された目標の移動方向ベクトル

  // ------------------------------------------------------------
  // 回転用パラメータ
  // ------------------------------------------------------------
  float currentRotateY_ = 0.0f; // プレイヤーモデルの現在のY軸回転角（ラジアン）
  float targetRotateY_ = 0.0f;  // モデルが向くべき目標のY軸回転角（ラジアン）
  bool isRotating_ = false;     // 現在回転処理を実行中かどうか

  // ------------------------------------------------------------
  // 状態・制御フラグ
  // ------------------------------------------------------------
  float stateTimer_ = 0.0f; // 現在の移動ステートでの経過時間
  bool canMove_ = true; // 移動操作が許可されているか（攻撃中などはfalseになる）
  bool canRotate_ = true; // 回転操作が許可されているか
  bool enableCameraRelativeMovement_ =
      true; // カメラの向きを基準にした移動を行うかどうか

  // ------------------------------------------------------------
  // 入力デバイス管理
  // ------------------------------------------------------------
  InputType lastInputType_ = InputType::Keyboard; // 直前に使用された入力デバイス（キーボード
                                                  // or パッド）
  float inputSwitchCooldown_ =
      0.0f; // 入力デバイスが頻繁に切り替わるのを防ぐためのクールダウン時間
  std::function<void(InputType)>
      onInputTypeChanged_; // 入力デバイス切り替え時に呼ばれるコールバック

  // ------------------------------------------------------------
  // カメラ追従制御
  // ------------------------------------------------------------
  float previousCameraRotationY_ = 0.0f; // 1フレーム前のカメラのY軸回転角
  bool isCameraMoving_ = false;  // カメラが現在回転しているかどうかのフラグ
  float cameraStopTimer_ = 0.0f; // カメラの回転が停止してからの経過時間

  // ------------------------------------------------------------
  // 攻撃時ステップ（前進補間）用パラメータ
  // ------------------------------------------------------------
  Vector3 stepStartPos_{0.0f, 0.0f, 0.0f};  // ステップを開始した際の位置
  Vector3 stepTargetPos_{0.0f, 0.0f, 0.0f}; // ステップで到達する目標位置
  float stepProgress_ = 0.0f;               // ステップの進行度（0.0 ～ 1.0）
  bool isAttackStepping_ = false;           // 現在攻撃ステップ中かどうか

  // ------------------------------------------------------------
  // オートホーミング用パラメータ
  // 攻撃発動時にターゲット方向へ向きと位置を Slerp / SmoothStep で補正する
  // ------------------------------------------------------------
  bool isHoming_ = false;
  float homingDuration_ = 0.12f;
  float homingTimer_ = 0.0f;
  Vector3 homingStartPos_{};
  Vector3 homingTargetPos_{};
  float homingStartYaw_ = 0.0f;
  float homingTargetYaw_ = 0.0f;
};
