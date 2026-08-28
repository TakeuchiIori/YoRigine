#include "BattleScene.h"

// Engine
#include "LightManager/LightManager.h"
#include "Object3D/Object3dCommon.h"
#include "OffScreen/PostEffectChain.h"
#include "OffScreen/PostEffectManager.h"
#include "Systems./Input./Input.h"
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/Virtuals/KeyframeCamera/KeyframeCamera.h"
#include "Systems/Cinematic/CinematicManager.h"
#include "Systems/Cinematic/CinematicSequencer.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include <SceneSystems/SceneManager.h>

#include "Object3D/BaseObjectManager.h"
#include <SceneEditor/SceneEditor.h>
#include <UI/Damage/DamageNumberManager.h>
#include <algorithm>
#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Debugger/Logger.h>

namespace {
// ===== 霧晴れカットシーン演出のタイムライン定数 =====
// 時間はすべて秒。Sequencer の全体時間 (duration) は KeyframeCamera
// のパス長から自動取得。
constexpr float kLetterboxInDuration = 1.0f;  // 黒帯がスライドインする時間
constexpr float kLetterboxOutDuration = 1.0f; // 黒帯がスライドアウトする時間
constexpr float kPostEffectFadeDelay =
    0.5f; // 黒帯 IN 完了後にエフェクト開始までの余白
constexpr float kPostEffectFadeEndMargin =
    0.5f; // 黒帯 OUT 開始前にエフェクトを終わらせる余白
constexpr float kGodRaysExposureMul = 1.5f; // 演出中の GodRays 露光倍率
constexpr int kCinematicCamPriority = 1000; // 演出中のカメラ優先度
constexpr float kFallbackDuration = 5.0f;   // ClearCinematic 不在時の保険
constexpr const char *kCinematicCamName = "ClearCinematic";
constexpr const char *kClearSceneName = "Clear";
} // namespace

/// <summary>
/// バトルシーン初期化
/// </summary>
void BattleScene::Initialize(YoRigine::Camera *camera, Player *player) {

  sceneCamera_ = camera;
  player_ = player;
  player->Reset();

  // BaseObjectManager へ登録（描画は従来どおり直接。一覧・名前検索用）
  BaseObjectManager::GetInstance()->Register(player_, "Player");

  //------------------------------------------------------------
  // バトル敵管理システム初期化
  //------------------------------------------------------------
  battleEnemyManager_ = std::make_unique<BattleEnemyManager>();
  battleEnemyManager_->Initialize(sceneCamera_);
  battleEnemyManager_->SetPlayer(player_);

  // 戦闘終了時のコールバック設定
  battleEnemyManager_->SetBattleEndCallback(
      [this](BattleResult result, const BattleStats &stats) {
        HandleBattleEnd(result, stats);
      });
  battleEnemyManager_->SetEnemyDefeatedCallback(
      [this](const BattleEnemy &) { FocusNearestEnemyAfterDefeat(); });

  //------------------------------------------------------------
  // 環境オブジェクト初期化
  //------------------------------------------------------------
  line_ = std::make_unique<YoRigine::Line>();
  line_->Initialize();
  line_->SetCamera(sceneCamera_);

  ground_ = std::make_unique<Ground>();
  ground_->Initialize(sceneCamera_);
  ground_->GetColor() = {0.5f, 0.2f, 0.5f, 1.0f};

  sprite_ = std::make_unique<YoRigine::Sprite>();
  sprite_->Initialize("Resources/Textures/GameScene/BattleScene.png");
  sprite_->SetTranslate({200.0f, 0.0f, 0.0f});

  //------------------------------------------------------------
  // UI初期化
  //------------------------------------------------------------
  lockOnUI_ = std::make_unique<LockOnUI>();
  lockOnUI_->Initialize(player_);

  DamageNumberManager::GetInstance()->Initialize();

  //------------------------------------------------------------
  // プレイヤーをエリア制限対象として登録
  //------------------------------------------------------------
  auto manager = AreaManager::GetInstance();
  manager->RegisterObject(&player_->GetWT(), "Player");

  // デバッグ描画を有効化
  manager->SetDebugDrawEnabled(true);

#ifdef USE_IMGUI
  Editor::GetInstance()->RegisterGameUI(
      "バトルモード:デバッグ情報",
      [this]() { battleEnemyManager_->ShowDebugInfo(); }, "Game", "ゲームプレイ");

  // 攻撃の経路エディタ。制御点をギズモで置いて動きを作る。
  motionPathEditor_.Initialize(sceneCamera_);
  Editor::GetInstance()->RegisterGameUI(
      "攻撃経路エディタ", [this]() { motionPathEditor_.Draw(); }, "Game", "ゲームプレイ");

  // ImGuizmo はゲームビューの ImGui コンテキスト内でしか描けないので、
  // Editor 側のギズモ描画タイミングに乗せてもらう。
  motionGizmoCallbackId_ = Editor::GetInstance()->AddGizmoDrawCallback(
      [this]() { motionPathEditor_.DrawGizmo(); });

  // 攻撃エディタ。プレビューで実際の敵を動かすためマネージャを渡す。
  attackEditor_.SetManager(battleEnemyManager_.get());
  Editor::GetInstance()->RegisterGameUI(
      "攻撃エディタ", [this]() { attackEditor_.Draw(); }, "Game", "ゲームプレイ");
#endif

  //------------------------------------------------------------
  // オブジェクトの先行ロード
  //------------------------------------------------------------
  YoRigine::SceneEditor::GetInstance()->LoadScene("Battle");
}

/// <summary>
/// 更新処理
/// </summary>
void BattleScene::Update() {

  // 開始演出はCameraModeと独立して管理する。演出中はこの場でゲーム更新を止める。
  if (battleIntroActive_) {
    const bool inPerformance = player_ && player_->GetPlayerCamera() &&
                               player_->GetPlayerCamera()->IsInPerformance();
    if (inPerformance) {
      battleIntroSawPerformance_ = true;
    } else if (battleIntroSawPerformance_) {
      battleIntroActive_ = false;
      battleCameraFinished_ = true;
      if (player_) {
        player_->SetControlEnabled(true);
        player_->SetInvincible(false);
      }
    }
  }

  // 最終バトルクリア検知（BattleScene側で遷移したいならここ）
  // 直接 ChangeScene せず、霧晴れ＋光芒のカットシーン演出を挟んでから遷移
  if (battleEnemyManager_->IsFinalBattleCleared() && !clearCinematicStarted_) {
    clearCinematicStarted_ = true;
    battleEnemyManager_->ResetFinalBattleClearFlag();

    // ゲーム更新を止める前に、攻撃中の剣軌跡と攻撃判定を確実に終了する。
    // TrailMeshEmitter::Stop() は保持頂点もClearするため、静止した軌跡が
    // クリア演出中に残り続けることを防げる。
    if (player_ && player_->GetSword()) {
      player_->GetSword()->ResetRuntimeState();
    }

    // チェーンから Fog / GodRays を引っ張る（無ければ tween をスキップ）
    auto *chain = PostEffectManager::GetInstance()->GetEffectChain();
    auto *fog =
        chain ? chain->GetFirstEffectByType(OffScreen::OffScreenEffectType::Fog)
              : nullptr;
    auto *godRays = chain ? chain->GetFirstEffectByType(
                                OffScreen::OffScreenEffectType::GodRays)
                          : nullptr;

    // シーケンス全体時間 = KeyframeCamera "ClearCinematic" のパス長
    // 編集で長さを変えるだけで letterbox / エフェクトの時刻が自動で追従する
    auto cinemaCam = std::dynamic_pointer_cast<KeyframeCamera>(
        CameraDirector::GetInstance()->GetCamera(kCinematicCamName));
    const float duration = (cinemaCam && cinemaCam->GetMaxTime() > 0.0f)
                               ? cinemaCam->GetMaxTime()
                               : kFallbackDuration;

    // エフェクトフェード時刻（黒帯 OUT 開始前に余白を確保して終わらせる）
    const float fadeStart = kPostEffectFadeDelay;
    const float fadeEnd =
        std::max(fadeStart + 0.1f,
                 duration - kLetterboxOutDuration - kPostEffectFadeEndMargin);

    auto seq = std::make_unique<YoRigine::CinematicSequencer>(duration);

    // --- レターボックス ---
    seq->Letterbox(true, 0.0f, kLetterboxInDuration,
                   Easing::Function::EaseOutCubic);
    seq->Letterbox(false, duration - kLetterboxOutDuration, duration,
                   Easing::Function::EaseInCubic);

    // --- Fog を 0 に補間（density / heightDensity / sunInscatter）---
    if (fog) {
      const auto &f = fog->params.fog;
      seq->Tween([fog](float v) { fog->params.fog.fogDensity = v; },
                 f.fogDensity, 0.0f, fadeStart, fadeEnd,
                 Easing::Function::EaseOutCubic);

      seq->Tween([fog](float v) { fog->params.fog.heightFogDensity = v; },
                 f.heightFogDensity, 0.0f, fadeStart, fadeEnd,
                 Easing::Function::EaseOutCubic);

      seq->Tween([fog](float v) { fog->params.fog.sunInscatterStrength = v; },
                 f.sunInscatterStrength, 0.0f, fadeStart, fadeEnd,
                 Easing::Function::EaseOutCubic);
    }

    // --- GodRays exposure を倍率倍へ（光が射す印象を強化）---
    if (godRays) {
      const float curExposure = godRays->params.godRays.exposure;
      seq->Tween([godRays](float v) { godRays->params.godRays.exposure = v; },
                 curExposure, curExposure * kGodRaysExposureMul, fadeStart,
                 fadeEnd, Easing::Function::EaseInOutSine);
    }

    // --- KeyframeCamera をシーン全体で再生 ---
    // 演出後は Clear シーンへ遷移するので、復帰先カメラは指定しない
    seq->Camera(kCinematicCamName, 0.0f, duration, kCinematicCamPriority);

    // 演出終了 → Clear へ遷移
    seq->OnFinish(
        []() { SceneManager::GetInstance()->ChangeScene(kClearSceneName); });

    YoRigine::CinematicManager::GetInstance()->Play(std::move(seq));
    return;
  }

  // 演出中は他のロジックを止める（プレイヤー操作・カメラ・敵更新ロック）
  if (clearCinematicStarted_) {
    return;
  }

  // プレイヤー更新
  // 敵より先に動かすことで、敵AIが今フレームのプレイヤー位置を参照できる
  if (!battleEnemyManager_->IsFinalBattleCleared()) {
    player_->Update();
  }

  // エリア制限補正（プレイヤー移動直後に境界クランプ →
  // 敵がクランプ済みの位置を参照）
  AreaManager::GetInstance()->UpdateSingleObject(&player_->GetWT());

  // 開始演出中は敵のAI・移動・攻撃・アニメーション進行だけを停止する。
  // GameTime自体は止めないため、カメラやPlayerの表示更新には影響しない。
  if (!battleIntroActive_) {
    battleEnemyManager_->Update();
  }

  // 敵削除と同フレームで lockedTarget_ がダングリングポインタになるのを防ぐ。
  // battleEnemyManager_->Update() 内で敵コライダーが破棄された直後に検証する。
  if (player_ && player_->GetPlayerCamera()) {
    player_->GetPlayerCamera()->ValidateLockOnTarget();
  }

  if (player_ && player_->GetPlayerCamera() && battleEnemyManager_) {
    std::vector<Vector3> enemyPositions;
    for (auto *enemy : battleEnemyManager_->GetActiveBattleEnemies()) {
      if (enemy) {
        enemyPositions.push_back(enemy->GetTranslate());
      }
    }
    player_->GetPlayerCamera()->SetThreatTargetPositions(enemyPositions);
  }

  // 視覚効果・オブジェクト更新
  sprite_->Update();
  ground_->Update();

  // UI更新
  lockOnUI_->Update();
  DamageNumberManager::GetInstance()->Update(
      YoRigine::GameTime::GetDeltaTime(),
      sceneCamera_->GetViewProjectionMatrix());
}

void BattleScene::FocusNearestEnemyAfterDefeat() {
  if (!player_ || !player_->GetPlayerCamera() || !battleEnemyManager_)
    return;

  BattleEnemy *nearest =
      battleEnemyManager_->GetNearestEnemy(player_->GetWT().translate_);
  if (!nearest)
    return;

  player_->GetPlayerCamera()->FaceDefeatNextEnemy(nearest->GetTranslate());
}

/// <summary>
/// 3Dオブジェクト描画
/// </summary>
void BattleScene::DrawObject() {
  // インスタンシング描画(敵)は最後に。DrawAll が ObjectInstanced
  // のルートシグネチャに 切り替えるため、その後に Object 系の個別描画を置くと
  // RS 不一致でエラーになる。
  ground_->Draw();
  player_->Draw();
  player_->DrawAnimation();

  if (battleEnemyManager_) {
    battleEnemyManager_->Draw();
  }
}

/// <summary>
/// デバッグライン描画
/// </summary>
void BattleScene::DrawLine() {
#ifdef USE_IMGUI
  // フレーム冒頭の頂点・マテリアル CB スロットのリセット (複数の DrawLine()
  // 呼出の干渉を避ける)
  line_->Reset();

  if (battleEnemyManager_) {
    battleEnemyManager_->DrawCollision();
  }

  player_->DrawCollision();
  player_->DrawBone(*line_.get());
  AreaManager::GetInstance()->DrawArea("BattleArea", line_.get());
  // AreaEditor で追加した他エリアもまとめて描画(BattleArea
  // は重複描画を避けて除外)
  AreaManager::GetInstance()->Draw(line_.get(), {"BattleArea"});
  line_->DrawLine();

  // 編集中の攻撃経路を線と制御点で表示する。
  // 専用の Line インスタンスを持つので、上の line_ とは干渉しない。
  motionPathEditor_.DrawPreview();

#endif
}

/// <summary>
/// UI描画
/// </summary>
void BattleScene::DrawUI() {
  // sprite_->Draw();

  // 演出中（クリアカットシーン等）はゲームプレイ用UIを一切描画しない
  if (YoRigine::CinematicManager::GetInstance()->IsActive())
    return;

  if (battleEnemyManager_) {
    battleEnemyManager_->DrawUI();
    DamageNumberManager::GetInstance()->Draw();

    // ポーズ中はロックオンUIだけ隠す
    // （ポーズUIは GameUI
    // 側でこの後に描画されるため、ここで出すと前面に被ってしまう）
    if (player_->IsAlive() && !YoRigine::GameTime::IsPause()) {
      lockOnUI_->Draw();
    }
  }
}

/*==========================================================================
        VFXの描画
//========================================================================*/
void BattleScene::DrawVFX() {}

void BattleScene::DrawNonOffscreen() {}

void BattleScene::DrawShadow() {
  player_->DrawShadow();
  battleEnemyManager_->DrawShadow();
}

/// <summary>
/// シーン遷移時に呼ばれる：状態リセットだけ行う。
/// バトル開始本体は SubSceneManager::ApplyTransitionData → StartBattle(data) で
/// 型付きデータと共に呼ばれるので、ここではデータを触らない。
/// </summary>
void BattleScene::OnEnter() {
  BaseSubScene::OnEnter();
  if (player_)
    player_->ResetForBattleStart();

  // 脅威察知（気配）はバトル中だけ有効化（フィールドでは背景・マップ見渡しを優先）
  if (player_ && player_->GetPlayerCamera()) {
    player_->GetPlayerCamera()->SetThreatAwarenessAllowed(true);
  }

  Logger("[BattleScene] ===== OnEnter() START =====\n");

  // バトル用 SceneEditor シーンへ切替。
  // Battle.json が無い場合は空シーンになる。初回は GameScene.json から
  // バトル用に置くものだけ残して保存して Battle.json を作る。
  YoRigine::SceneEditor::GetInstance()->LoadScene("Battle");

  // 境界エリア(BattleArea) を入場のたびに登録し直す。
  // FieldScene::OnEnter が RemoveArea("BattleArea")
  // するため、ここで再生成しないと バトル中の境界制限 (UpdateSingleObject)
  // が効かなくなる。
  {
    auto *mgr = AreaManager::GetInstance();
    mgr->RemoveArea("BattleArea"); // 念のため重複除去
    auto battleField = std::make_shared<CircleArea>();
    battleField->Initialize(Vector3(0, 0, 0), 50.0f);
    battleField->SetPurpose(AreaPurpose::Boundary); // 境界制限として設定
    battleField->SetCamera(sceneCamera_);
    mgr->AddArea("BattleArea", battleField);
  }

  // 開始演出の停止状態だけ初期化する。CameraModeは変更しない。
  battleCameraFinished_ = true;
  battleIntroActive_ = false;
  battleIntroSawPerformance_ = false;
  if (player_) {
    player_->SetControlEnabled(true);
    player_->SetInvincible(false);
  }
  shouldResetBattleCamera_ = true;

  // クリア演出フラグをリセット（再戦・別バトル開始時のため）
  clearCinematicStarted_ = false;

  // プレイヤー位置リセット
  player_->SetInitialPosition();

  Logger("[BattleScene] ===== OnEnter() END =====\n");
}

/// <summary>
/// シーンを抜ける時に呼ばれる：敵削除と UI 後始末のみ。
/// FieldReturnData は HandleBattleEnd() が SubSceneManager 経由で直接渡す。
/// </summary>
void BattleScene::OnExit() {
  BaseSubScene::OnExit();
  battleIntroActive_ = false;
  battleIntroSawPerformance_ = false;
  if (player_) {
    player_->SetControlEnabled(true);
    player_->SetInvincible(false);
  }

  Logger("[BattleScene] ===== OnExit() START =====\n");

  // 境界エリアを掃除（FieldScene 側でも除去しているが対称性のため明示）
  AreaManager::GetInstance()->RemoveArea("BattleArea");
  if (player_ && player_->GetPlayerCamera()) {
    player_->GetPlayerCamera()->ClearThreatTargetPositions();
    player_->GetPlayerCamera()->SetThreatAwarenessAllowed(false);
  }

  // ロックオンUIを非表示にする
  lockOnUI_->SetIsVisible(false);
  // シーンから抜ける時に、強制的に敵を全削除する
  if (battleEnemyManager_) {
    battleEnemyManager_->RemoveAllBattleEnemies();
  }
  currentCameraMode_ = CameraMode::FOLLOW;

  Logger("[BattleScene] ===== OnExit() END =====\n");
}

/// <summary>
/// バトル開始エントリポイント。
/// SubSceneManager がフェード演出の途中でこの関数に BattleTransitionData
/// を渡す。
/// </summary>
void BattleScene::StartBattle(const BattleTransitionData &data) {
  Logger("[BattleScene] ===== StartBattle() START =====\n");

  originalTransitionData_ = data;
  currentEnemyGroup_ = data.enemyGroup;
  isFinalBattle_ = data.isFinalBattle;
  totalRemainingFieldEnemies_ = data.totalRemainingFieldEnemies;

  char debugBuffer[256];
  sprintf_s(debugBuffer,
            "[BattleScene] isFinalBattle: %s, Remaining groups: %zu\n",
            isFinalBattle_ ? "TRUE" : "FALSE", totalRemainingFieldEnemies_);
  Logger(debugBuffer);

  if (isFinalBattle_) {
    Logger("[BattleScene] ★★★ FINAL BATTLE FLAG SET! ★★★\n");
  }

  // エンカウントデータ構築
  EnemyEncounterData encounter;
  encounter.encounterName = data.enemyGroup + "_Battle";
  encounter.enemyScale = data.battleEnemyScale;

  if (!data.battleEnemyIds.empty()) {
    encounter.enemyIds = data.battleEnemyIds;
    Logger("[BattleScene] 複数体バトル開始\n");
  } else {
    encounter.enemyIds = {data.battleEnemyId};
    Logger("[BattleScene] 単体バトル開始\n");
  }

  // フォーメーション設定
  if (!data.battleFormation.empty() && data.battleFormation != "default" &&
      battleEnemyManager_) {
    auto formation = battleEnemyManager_->GetFormation(data.battleFormation);
    if (!formation.positions.empty()) {
      encounter.formations = formation.positions;
    }
  }

  // デフォルト配置
  if (encounter.formations.empty() && battleEnemyManager_) {
    encounter.formations =
        battleEnemyManager_->GetFormationPositions(encounter.enemyIds.size());
  }

  if (battleEnemyManager_) {
    battleEnemyManager_->SetFinalBattleMode(isFinalBattle_);
    battleEnemyManager_->StartBattle(encounter);
  }

  // 敵生成後、最も近い敵の方向へ向いた待機姿勢を確定する。
  if (player_ && battleEnemyManager_) {
    if (auto *nearest =
            battleEnemyManager_->GetNearestEnemy(player_->GetWorldPosition())) {
      player_->FacePosition(nearest->GetTranslate());
    }
  }

  // CameraModeは変更せず、開始演出中はプレイヤー操作だけを停止する。
  battleIntroActive_ = true;
  battleIntroSawPerformance_ = false;
  battleCameraFinished_ = false;
  player_->SetControlEnabled(false);
  player_->SetInvincible(true);
  player_->GetPlayerCamera()->PlayBattleStart();
  battleIntroSawPerformance_ = player_->GetPlayerCamera()->IsInPerformance();

  Logger("[BattleScene] ===== StartBattle() END =====\n");
}

/// <summary>
/// 戦闘終了時の処理
/// </summary>
void BattleScene::HandleBattleEnd(BattleResult result,
                                  const BattleStats &stats) {
  Logger("[BattleScene] ===== HandleBattleEnd() START =====\n");

  char debugBuffer[512];
  sprintf_s(debugBuffer,
            "[BattleScene] isFinalBattle_: %s, result: %d (Victory=1)\n",
            isFinalBattle_ ? "TRUE" : "FALSE", static_cast<int>(result));
  Logger(debugBuffer);

  //------------------------------------------------------------
  // 最終バトルで勝利した場合、クリアシーンへ直接遷移
  //------------------------------------------------------------
  if (isFinalBattle_ && result == BattleResult::Victory) {
    Logger("[BattleScene] ★★★ Final Battle Victory! Transitioning to Clear "
           "Scene ★★★\n");

    // 注意: BattleEnemyManagerのスロー演出処理で既にクリアシーンへ遷移済み
    // このコードは念のための保険（通常は到達しない）
    Logger("[BattleScene] Already handled by BattleEnemyManager slow motion\n");

    Logger("[BattleScene] ===== HandleBattleEnd() END (Clear Scene) =====\n");
    return; // フィールドに戻るコールバックを実行しない
  } else {
    if (isFinalBattle_) {
      Logger("[BattleScene] Final battle but not victory (Defeat?)\n");
    }
    if (result == BattleResult::Victory) {
      Logger("[BattleScene] Victory but not final battle\n");
    }
  }

  //------------------------------------------------------------
  // 通常のバトル終了処理
  //------------------------------------------------------------
  FieldReturnData returnData;
  CreateBattleReturnData(returnData, result, stats);

  const char *resultStr = (result == BattleResult::Victory)  ? "Victory"
                          : (result == BattleResult::Defeat) ? "Defeat"
                                                             : "Other";

  char buffer[256];
  sprintf_s(buffer, "[BattleScene] 結果: %s, EXP:%d, Gold:%d\n", resultStr,
            stats.totalExpGained, stats.totalGaldGained);
  Logger(buffer);

  battleEnemyManager_->ForceBattleEnd();

  // SubSceneManager 経由でフィールドへ戻る（in-memory コピーで FieldReturnData
  // を渡す）
  RequestFieldTransition(returnData);

  Logger("[BattleScene] ===== HandleBattleEnd() END =====\n");
}

/// <summary>
/// 強制的に戦闘を終了
/// </summary>
void BattleScene::ForceBattleEnd() {
  if (battleEnemyManager_) {
    battleEnemyManager_->ForceBattleEnd();
  }
  Logger("[BattleScene] Battle force ended\n");
}

/// <summary>
/// プレイヤー状態の保存
/// </summary>
void BattleScene::SavePlayerState(
    [[maybe_unused]] const BattleTransitionData &data) {
  if (player_) {
    Logger("[BattleScene] Player state saved\n");
  }
}

/// <summary>
/// 戦闘終了時の戻りデータを作成
/// </summary>
void BattleScene::CreateBattleReturnData(FieldReturnData &data,
                                         BattleResult result,
                                         const BattleStats &stats) {
  data.playerPosition = originalTransitionData_.playerPosition;
  data.cameraPosition = originalTransitionData_.cameraPosition;
  data.cameraMode = originalTransitionData_.cameraMode;
  data.defeatedEnemyGroup = currentEnemyGroup_;
  data.playerWon = (result == BattleResult::Victory);
  data.expGained = stats.totalExpGained;
  data.goldGained = stats.totalGaldGained;
  data.itemsGained = stats.droppedItems;
  data.playerHpRatio = 1.0f;

  Logger("[BattleScene] Field return data created\n");
}

/// <summary>
/// バトルが進行中かを確認
/// </summary>
bool BattleScene::IsBattleActive() const {
  return battleEnemyManager_ ? battleEnemyManager_->IsBattleActive() : false;
}

/// <summary>
/// バトルカメラ終了フラグ設定
/// </summary>
void BattleScene::SetBattleCameraFinished(bool finished) {
  battleCameraFinished_ = finished;
}

/// <summary>
/// 終了処理
/// </summary>
void BattleScene::Finalize() {
  if (battleEnemyManager_) {
    battleEnemyManager_->Finalize();
  }

#ifdef USE_IMGUI
  // 解除しないと破棄済みの this を指したままコールバックが呼ばれる
  if (motionGizmoCallbackId_ != 0) {
    Editor::GetInstance()->RemoveGizmoDrawCallback(motionGizmoCallbackId_);
    motionGizmoCallbackId_ = 0;
  }
#endif
}
