#pragma once

// App
#include "GuardConfig.h"

// Engine
#include "Vector3.h"
#include <functional>
#include <string>

class Player;

// ============================================================
// プレイヤーのガード状態管理クラス
//
// 「発生 → 持続（内側にパリィ区間）→ 硬直」というタイムラインを進め、
// 攻撃を受けた瞬間に ResolveHit() で防御の成否を判定する。
//
// 判定はこのクラスの ResolveHit() ただ1箇所で行う。
// 以前は盾のコライダー側と本体の被弾側の2経路から呼ばれていて、
// どちらが先に処理されるかで結果が変わる状態だった。
// ============================================================
class PlayerGuard {
public:
  // ガード判定の結果
  enum class GuardResult {
    GuardFail,    // 防御不成立（ダメージをそのまま受ける）
    GuardSuccess, // 通常ガード成立（軽減されるが押し込まれる）
    ParrySuccess  // パリィ成立（無効化して相手を突き放す）
  };

  // ガードの進行フェーズ
  enum class State {
    Idle,    // 待機中
    StartUp, // 発生前（まだ防げない）
    Active,  // 防御が成立する期間（内側にパリィ区間を含む）
    Recovery // 解除後の硬直
  };

  // ============================================================
  // 初期化と更新処理
  // ============================================================
  explicit PlayerGuard(Player *owner);

  // 設定ファイルを読み込む（存在しなければ既定値のまま）
  void LoadConfig(const std::string &path = GuardConfigIO::kDefaultPath);
  bool SaveConfig(const std::string &path = GuardConfigIO::kDefaultPath) const;

  bool StartGuard();
  void Update(float deltaTime);
  void Reset();

  // ============================================================
  // 防御判定
  // ============================================================

  /// <summary>
  /// 攻撃を受けた瞬間の防御判定。ダメージ処理から呼ぶ唯一の判断点。
  /// </summary>
  /// <param
  /// name="attackerPosition">攻撃してきた相手の位置（正面判定に使う）</param>
  GuardResult ResolveHit(const Vector3 &attackerPosition);

  /// <summary>
  /// 判定結果に対応する結果パラメータを取得する。
  /// GuardFail の場合は nullptr。
  /// </summary>
  const GuardOutcome *GetOutcome(GuardResult result) const;

  // ============================================================
  // 状態確認
  // ============================================================
  bool IsGuarding() const {
    return state_ == State::StartUp || state_ == State::Active;
  }

  // 今この瞬間がパリィ受付中か
  bool IsParryWindow() const {
    return state_ == State::Active &&
           frame_ >= config_.timeline.parryStartFrame &&
           frame_ <= config_.timeline.parryEndFrame;
  }

  State GetState() const { return state_; }
  int GetFrame() const { return frame_; }

  // タイムライン先頭から数えた現在フレーム（エディタの再生ヘッド表示用）
  int GetTimelineFrame() const;

  // ============================================================
  // 設定へのアクセス（エディタ用）
  // ============================================================
  GuardConfig &GetConfig() { return config_; }
  const GuardConfig &GetConfig() const { return config_; }

  // ============================================================
  // コールバック設定
  // ============================================================
  void SetOnGuardSuccess(std::function<void()> cb) {
    onGuardSuccess_ = std::move(cb);
  }
  void SetOnParrySuccess(std::function<void()> cb) {
    onParrySuccess_ = std::move(cb);
  }
  void SetOnGuardFail(std::function<void()> cb) {
    onGuardFail_ = std::move(cb);
  }

  using StateCallback = std::function<void(State /*from*/, State /*to*/)>;
  void SetStateChangeCallback(StateCallback cb) {
    onStateChanged_ = std::move(cb);
  }

  // ============================================================
  // デバッグ
  // ============================================================
  void ShowDebugImGui();

private:
  void ChangeState(State s);

  // 攻撃が正面から来たか（背後からの攻撃はガードできない）
  bool IsAttackFromFront(const Vector3 &attackerPosition) const;

private:
  // ------------------------------------------------------------
  // システム連携・設定
  // ------------------------------------------------------------
  Player *owner_ = nullptr; // このガードシステムを所持するプレイヤー
  GuardConfig config_;      // タイムラインと結果パラメータ

  // ------------------------------------------------------------
  // 状態管理
  // ------------------------------------------------------------
  State state_ = State::Idle; // 現在の進行フェーズ
  int frame_ = 0;             // 現在フェーズでの経過フレーム数

  // ------------------------------------------------------------
  // コールバック
  // ------------------------------------------------------------
  std::function<void()> onGuardSuccess_;
  std::function<void()> onParrySuccess_;
  std::function<void()> onGuardFail_;
  StateCallback onStateChanged_;
};
