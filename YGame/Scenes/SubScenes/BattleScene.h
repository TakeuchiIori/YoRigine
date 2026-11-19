#pragma once

// C++
#include <functional>
#include <memory>
#include <string>

// Engine
#include "Collision/AreaCollision/Base/AreaManager.h"
#include "Collision/AreaCollision/CircleArea.h"
#include "Drawer/LineManager/Line.h"
#include "Object3D/Object3d.h"
#include "Player/Player.h"
#include "Systems/Camera/Camera.h"
#include "WorldTransform./WorldTransform.h"

// App
#include "BaseSubScene.h"
#include "Enemy/Attack/Editor/EnemyAttackEditor.h"
#include "Enemy/Attack/Motion/MotionPathEditor.h"
#include "Enemy/BattleEnemy/BattleEnemyManager.h"
#include "Enemy/Boss/BossAppearanceSettings.h"
#include "GameIds.h"
#include "Ground/Ground.h"
#include "OffScreen/OffScreen.h"
#include "SceneDataStructures.h"
#ifdef USE_IMGUI
#include "Enemy/Boss/Editor/BossEditor.h"
#endif
#include <Loaders/Json/JsonManager.h>
#include <UI/LockOn/LockOnUI.h>

///************************* バトルシーン *************************///
class BattleScene : public BaseSubScene {
public:
  ///************************* 基本関数 *************************///

  // コンストラクタ
  BattleScene() : BaseSubScene(SubSceneId::Battle) {}

  // 初期化処理
  void Initialize(YoRigine::Camera *camera, Player *player) override;

  // 更新処理
  void Update() override;

  // オブジェクト描画処理
  void DrawObject() override;

  // ライン描画処理
  void DrawLine() override;

  // UI描画処理
  void DrawUI() override;

  // シャドウ描画処理
  void DrawShadow() override;

  // 終了処理
  void Finalize() override;

  ///************************* ライフサイクル *************************///

  // シーンに入ったときの処理（エンカウントなら BattleTransitionData が来る）
  void OnEnter(const SubScenePayload &payload) override;

  // シーンから出るときの処理
  void OnExit() override;

  ///************************* シーン固有処理 *************************///

  // 戦闘を強制終了
  void ForceBattleEnd();

  // バトルカメラ終了フラグを設定
  void SetBattleCameraFinished(bool finished);

  // ゲームクリアフラグを設定
  void SetGameClearFlag(bool isFinalBattle) { isFinalBattle_ = isFinalBattle; }

  bool IsAllEnemysDefeated() {
    return battleEnemyManager_->AreAllEnemiesDefeated();
  };

  // BattleEnemyManagerへのアクセサ
  BattleEnemyManager *GetBattleEnemyManager() const {
    return battleEnemyManager_.get();
  }

  ///************************* アクセッサ *************************///

  // 現在の敵グループ名を取得
  std::string GetCurrentEnemyGroup() const { return currentEnemyGroup_; }

  // 戦闘中か確認
  bool IsBattleActive() const;

  // バトルカメラをリセットすべきか確認
  bool ShouldResetBattleCamera() const { return shouldResetBattleCamera_; }

  // バトルカメラリセットフラグをクリア
  void ClearBattleCameraResetFlag() { shouldResetBattleCamera_ = false; }

private:
  ///************************* 内部処理 *************************///

  // 戦闘を開始（OnEnter で受け取った遭遇データから敵を配置する）
  void StartBattle(const BattleTransitionData &data);

  // 最終戦勝利後のクリア演出を開始する
  void StartClearCinematic();

  // 最終戦の取り巻きを倒し切った後、黒い霧の中からボスを出現させる演出を開始する。
  // 出現させるボスが設定されていなければ false（そのままクリア演出へ進む）。
  bool StartBossAppearance();

  // ボス出現で暗くした霧を、演出前の状態へ戻す（シーンを抜ける時など）
  void RestoreFogAfterBossAppearance();

  // 霧だけのプレビュー（ボス戦エディタ用）。
  // t: 0=演出前 → 1=最も濃い霧 → 2=ボス戦中の霧。演出前の霧は退避しておき、
  // RestoreFogAfterBossAppearance で戻す。
  void PreviewBossFog(float t);

  // 出現演出を最初から再生し直す（ボス戦エディタ用）。既にいるボスは消してから出す。
  void ReplayBossAppearance();

  // ボスを出す位置（プレイヤーから、今カメラが向いている方向へ spawnDistance
  // 先）
  Vector3 ComputeBossSpawnPosition() const;

  // ── 出現演出の構図（プレイヤーの立ち位置とカメラの注視）──
  // 出現位置へ向かう直線上で、playerStandDistance だけ手前の立ち位置
  Vector3 ComputeBossAppearanceStandPosition(const Vector3 &spawnPos) const;
  // カメラの注視点（ボスが居れば胸、居なければ出現位置の上）
  Vector3 ComputeBossAppearanceFocus() const;
  PlayerCamera::CinematicFocusParams MakeBossAppearanceCameraParams() const;
  // 演出中に毎フレーム：注視点とプレイヤーの向く先を更新する
  void UpdateBossAppearanceStaging();
  // 演出の終了・中断時：自動移動と注視を解いて操作を返す
  void EndBossAppearanceStaging();

  // 生成中のボスをすべて取り除く（エディタで出し直す前に）
  void RemoveBosses();

  // 演出なしで、目の前にボスを出す（ボス戦エディタ用）
  void SpawnBossNow();

  // 演出前の霧から「最も濃い霧」「ボス戦中の霧」を作る
  OffScreen::FogParams MakeDarkFog(const OffScreen::FogParams &base) const;
  OffScreen::FogParams MakeBattleFog(const OffScreen::FogParams &base) const;

  // 戦闘終了時の処理
  void HandleBattleEnd(BattleResult result, const BattleStats &stats);

  // プレイヤー状態を保存
  void SavePlayerState(const BattleTransitionData &data);

  // 戦闘終了後の戻りデータを作成
  void CreateBattleReturnData(FieldReturnData &data, BattleResult result,
                              const BattleStats &stats);

  void FocusNearestEnemyAfterDefeat();

private:
  ///************************* メンバ変数 *************************///

  std::unique_ptr<BattleEnemyManager> battleEnemyManager_;
  std::unique_ptr<YoRigine::Line> line_;

#ifdef USE_IMGUI
  // 攻撃の経路を制御点で編集するエディタ（Debugのみ）。
  // ギズモはゲームビューの ImGui コンテキスト内で描く必要があるため、
  // Editor のギズモコールバックへ登録して呼んでもらう。
  MotionPathEditor motionPathEditor_;
  int motionGizmoCallbackId_ = 0;

  // 攻撃をカーブで作るエディタ。プレビューは戦闘中の敵で再生する。
  EnemyAttackEditor attackEditor_;
#endif
  std::unique_ptr<YoRigine::Sprite> sprite_;
  std::unique_ptr<Ground> ground_;
  std::unique_ptr<YoRigine::BaseObject> enemy_;
  std::unique_ptr<LockOnUI> lockOnUI_;

  std::string currentEnemyGroup_;
  BattleTransitionData originalTransitionData_;
  bool battleCameraFinished_ = false;
  bool shouldResetBattleCamera_ = false;
  bool battleIntroActive_ = false;
  bool battleIntroSawPerformance_ = false;
  bool isFinalBattle_ = false; // 最終バトルかどうか
  size_t totalRemainingFieldEnemies_ =
      0; // ★追加: フィールドに残っているエンカウントグループ数

  // 最終バトルクリア演出が起動済みかどうか（毎フレーム再発火するのを防ぐ）
  bool clearCinematicStarted_ = false;

  ///************************* ボス出現演出 *************************///
  // 最終戦の取り巻きを全滅させると、クリアへ進む前に黒い霧がステージを覆い、
  // カメラ正面の霧の中からボスが実体化してそのままボス戦になる。
  // ボスを倒すと既存のクリア演出（霧晴れ）でクリアシーンへ進む。
  BossAppearanceSettings bossAppearance_;
  std::unique_ptr<YoRigine::JsonManager> bossAppearanceJson_;
#ifdef USE_IMGUI
  BossEditor bossEditor_; // ボス戦エディタ（当たり判定・攻撃・出現演出の調整）
#endif

  bool bossPhaseStarted_ =
      false; // この戦闘でボスを出したか（2回目の全滅でクリアへ）
  bool bossAppearanceActive_ = false; // 出現演出の再生中か
  Vector3 bossAppearanceSpawnPos_{};  // 今回の出現位置（演出開始時に確定）
  bool hasSavedFog_ = false;          // 演出前の霧を退避しているか
  OffScreen::FogParams savedFog_{};   // 演出前の霧（シーンを抜ける時に戻す）
};
