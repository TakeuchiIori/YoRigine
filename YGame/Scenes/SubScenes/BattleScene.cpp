#include "BattleScene.h"
#include "Enemy/Boss/BossEnemy.h"

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
constexpr const char *kCinematicCamName = CameraId::ClearCinematic;
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

  //------------------------------------------------------------
  // ボス出現演出の調整値（JSON 保存・エディタで調整）
  //------------------------------------------------------------
  {
    auto &s = bossAppearance_;
    bossAppearanceJson_ = std::make_unique<YoRigine::JsonManager>(
        "BossAppearance", "Resources/Json/Scenes/");
    bossAppearanceJson_->SetCategory("BossAppearance");
    bossAppearanceJson_->Register("ボスの敵ID", &s.bossEnemyId);
    bossAppearanceJson_->Register("出現距離", &s.spawnDistance);

    bossAppearanceJson_->SetTreePrefix("構図");
    bossAppearanceJson_->Register("プレイヤーの立ち位置", &s.playerStandDistance);
    bossAppearanceJson_->Register("カメラの肩ずらし", &s.cameraShoulderYaw);
    bossAppearanceJson_->Register("カメラの見下ろし", &s.cameraPitch);
    bossAppearanceJson_->Register("出現前の注視の高さ", &s.cameraLookHeight);
    bossAppearanceJson_->Register("カメラの寄る速さ", &s.cameraBlendSpeed);
    bossAppearanceJson_->ClearTreePrefix();

    bossAppearanceJson_->SetTreePrefix("タイムライン");
    bossAppearanceJson_->Register("霧が覆うまで", &s.fogInDuration);
    bossAppearanceJson_->Register("ボス生成時刻", &s.spawnTime);
    bossAppearanceJson_->Register("実体化の時間", &s.emergenceDuration);
    bossAppearanceJson_->Register("霧が晴れ始める時刻", &s.fogLiftStart);
    bossAppearanceJson_->Register("霧が晴れる時間", &s.fogLiftDuration);
    bossAppearanceJson_->Register("演出全体", &s.totalDuration);
    bossAppearanceJson_->ClearTreePrefix();

    bossAppearanceJson_->SetTreePrefix("最も濃い霧");
    bossAppearanceJson_->Register("色", &s.darkFogColor);
    bossAppearanceJson_->Register("濃度", &s.darkFogDensity);
    bossAppearanceJson_->Register("開始距離", &s.darkFogStart);
    bossAppearanceJson_->Register("高さフォグ", &s.darkHeightFogDensity);
    bossAppearanceJson_->Register("空のフォグ上限", &s.darkSkyFogClamp);
    bossAppearanceJson_->ClearTreePrefix();

    bossAppearanceJson_->SetTreePrefix("ボス戦中の霧");
    bossAppearanceJson_->Register("色", &s.battleFogColor);
    bossAppearanceJson_->Register("濃度", &s.battleFogDensity);
    bossAppearanceJson_->Register("開始距離", &s.battleFogStart);
    bossAppearanceJson_->Register("高さフォグ", &s.battleHeightFogDensity);
    bossAppearanceJson_->Register("空のフォグ上限", &s.battleSkyFogClamp);
    bossAppearanceJson_->ClearTreePrefix();
  }

  // 戦闘終了時のコールバック設定
  battleEnemyManager_->SetBattleEndCallback(
      [this](BattleResult result, const BattleStats &stats) {
        HandleBattleEnd(result, stats);
      });
  battleEnemyManager_->SetEnemyDefeatedCallback(
      [this](const BattleEnemyBase &) { FocusNearestEnemyAfterDefeat(); });

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
      [this]() { battleEnemyManager_->ShowDebugInfo(); }, SceneId::Game, "ゲームプレイ");

  // 攻撃の経路エディタ。制御点をギズモで置いて動きを作る。
  motionPathEditor_.Initialize(sceneCamera_);
  Editor::GetInstance()->RegisterGameUI(
      "攻撃経路エディタ", [this]() { motionPathEditor_.Draw(); }, SceneId::Game, "ゲームプレイ");

  // ImGuizmo はゲームビューの ImGui コンテキスト内でしか描けないので、
  // Editor 側のギズモ描画タイミングに乗せてもらう。
  motionGizmoCallbackId_ = Editor::GetInstance()->AddGizmoDrawCallback(
      [this]() { motionPathEditor_.DrawGizmo(); });

  // 攻撃エディタ。プレビューで実際の敵を動かすためマネージャを渡す。
  attackEditor_.SetManager(battleEnemyManager_.get());
  Editor::GetInstance()->RegisterGameUI(
      "攻撃エディタ", [this]() { attackEditor_.Draw(); }, SceneId::Game, "ゲームプレイ");

  // ボス戦エディタ。出現演出はこのシーンが持つので、必要な口だけ渡す。
  {
    BossEditor::SceneHooks hooks;
    hooks.appearance = &bossAppearance_;
    hooks.appearanceJson = bossAppearanceJson_.get();
    hooks.previewFog = [this](float t) { PreviewBossFog(t); };
    hooks.endFogPreview = [this]() { RestoreFogAfterBossAppearance(); };
    hooks.replayAppearance = [this]() { ReplayBossAppearance(); };
    hooks.spawnBossNow = [this]() { SpawnBossNow(); };
    bossEditor_.SetManager(battleEnemyManager_.get());
    bossEditor_.SetSceneHooks(hooks);
  }
  Editor::GetInstance()->RegisterGameUI(
      "ボス戦エディタ", [this]() { bossEditor_.Draw(); }, SceneId::Game, "ゲームプレイ");
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

#ifdef _DEBUG
  // 【一時的な確認用】YORIGINE_DEBUG_AUTOKILL=秒 で起動すると、開始演出明けから
  // その秒数後に取り巻きを全滅させる（ボス出現演出を手で戦わずに確認するため）
  {
    static float s_autoKillTimer = -1.0f;
    static bool s_autoKillRead = false;
    if (!s_autoKillRead) {
      s_autoKillRead = true;
      char *value = nullptr;
      size_t length = 0;
      if (_dupenv_s(&value, &length, "YORIGINE_DEBUG_AUTOKILL") == 0 && value) {
        s_autoKillTimer = static_cast<float>(std::atof(value));
        free(value);
      }
    }
    if (s_autoKillTimer >= 0.0f && !battleIntroActive_ && !bossPhaseStarted_) {
      s_autoKillTimer -= YoRigine::GameTime::GetDeltaTime();
      if (s_autoKillTimer < 0.0f) {
        battleEnemyManager_->DamageAllEnemies(999999);
      }
    }
  }
#endif

  // 最終戦の取り巻きを倒し切ったら、まず黒い霧の中からボスを出す。
  // ボスまで倒したら（2回目の全滅）、霧晴れ＋光芒のクリア演出を挟んで Clear へ。
  if (battleEnemyManager_->IsFinalBattleCleared() && !clearCinematicStarted_) {
    if (!bossPhaseStarted_ && StartBossAppearance()) {
      return;
    }
    StartClearCinematic();
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

    // ボス戦フレーミング：生存中のボスがいれば、胸の位置と身長をカメラへ渡す。
    // 雑魚戦ではボスがいないので何も渡さず、カメラは通常のまま。
    bool hasBoss = false;
    for (auto *enemy : battleEnemyManager_->GetActiveBattleEnemies()) {
      auto *boss = dynamic_cast<BossEnemy *>(enemy);
      if (!boss) {
        continue;
      }
      Vector3 focus{};
      float height = 0.0f;
      if (boss->GetCameraFraming(focus, height)) {
        player_->GetPlayerCamera()->SetBossFraming(focus, height,
                                                   boss->GetPrimaryCollider());
        hasBoss = true;
        break;
      }
    }
    if (!hasBoss) {
      player_->GetPlayerCamera()->ClearBossFraming();
    }
  }

  // ボス出現演出中：カメラの注視点とプレイヤーの向く先をボスへ追従させる
  UpdateBossAppearanceStaging();

  // 視覚効果・オブジェクト更新
  sprite_->Update();
  ground_->Update();

  // UI更新
  lockOnUI_->Update();
  DamageNumberManager::GetInstance()->Update(
      YoRigine::GameTime::GetDeltaTime(),
      sceneCamera_->GetViewProjectionMatrix());
}

/// <summary>
/// 最終戦勝利後のクリア演出
/// 黒帯・霧晴れ・光芒・カメラを 1 本のシーケンスにまとめて再生する。
/// </summary>
void BattleScene::StartClearCinematic() {
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

  // 演出終了 → GameScene へクリアを通知（Clear への遷移は親が判断する）
  seq->OnFinish([this]() { RaiseEvent(SubSceneEvent::GameClear); });

  YoRigine::CinematicManager::GetInstance()->Play(std::move(seq));
}

/// <summary>
/// ボス出現演出。
/// 黒い霧がステージを覆う → カメラ正面の霧の中からボスが実体化して咆哮
/// → 霧がボス戦用の暗さまで少し晴れ、操作が戻ってそのままボス戦。
/// </summary>
bool BattleScene::StartBossAppearance() {
  const auto &s = bossAppearance_;
  if (s.bossEnemyId.empty() || !battleEnemyManager_ || !player_) {
    return false;
  }
  // 敵データに無い ID だと SpawnBattleEnemy がエラーで止まるので、ここで確かめる
  if (!battleEnemyManager_->HasEnemyData(s.bossEnemyId)) {
    Logger("[BattleScene] ボスの敵ID が敵データにありません: " + s.bossEnemyId +
           "（ボス戦を飛ばしてクリアへ進みます）\n");
    return false;
  }

  bossPhaseStarted_ = true;
  bossAppearanceActive_ = true;

  // クリアフラグだけ先に下ろす（毎フレームこの演出を始め直さないため）。
  // クリア待ちの解除はボスを生成した瞬間に行う。先に解除すると、ボスが
  // 現れるまでの間「敵がいない＝全滅」の判定が毎フレーム走ってしまう。
  battleEnemyManager_->ResetFinalBattleClearFlag();

  const Vector3 spawnPos = ComputeBossSpawnPosition();
  bossAppearanceSpawnPos_ = spawnPos;

  //------------------------------------------------------------
  // プレイヤー：入力を止め、出現位置の手前まで歩かせてボスの方を向かせる。
  // 取り巻きを倒した直後の攻撃・剣の軌跡・判定は BeginScriptedMove が片付ける。
  //------------------------------------------------------------
  player_->SetInvincible(true);
  player_->BeginScriptedMove(ComputeBossAppearanceStandPosition(spawnPos), spawnPos);

  //------------------------------------------------------------
  // カメラ：プレイヤー越しに出現位置（生成後はボスの胸）を画面中央へ捉え続ける。
  // スティックでよそを向けないよう、演出中はカメラ操作も受け付けない。
  //------------------------------------------------------------
  if (auto *camera = player_->GetPlayerCamera()) {
    camera->BeginCinematicFocus(ComputeBossAppearanceFocus(), MakeBossAppearanceCameraParams());
  }

  // 霧：演出前の値を退避してから、最も濃い霧 → ボス戦中の霧 へ補間する
  auto *chain = PostEffectManager::GetInstance()->GetEffectChain();
  auto *fog = chain ? chain->GetFirstEffectByType(OffScreen::OffScreenEffectType::Fog)
                    : nullptr;
  if (fog && !hasSavedFog_) {
    savedFog_ = fog->params.fog;
    hasSavedFog_ = true;
  }

  auto seq = std::make_unique<YoRigine::CinematicSequencer>(s.totalDuration);

  // --- 黒帯 ---
  seq->Letterbox(true, 0.0f, kLetterboxInDuration, Easing::Function::EaseOutCubic);
  seq->Letterbox(false, s.totalDuration - kLetterboxOutDuration, s.totalDuration,
                 Easing::Function::EaseInCubic);

  // --- 霧 ---
  if (fog) {
    const OffScreen::FogParams from = fog->params.fog;
    // 1 本の進行度 t で全パラメータを動かす（項目ごとに Tween を並べるとズレやすい）
    auto blendFog = [fog](const OffScreen::FogParams &a, const OffScreen::FogParams &b,
                          float t) {
      auto &f = fog->params.fog;
      f.fogColor = Lerp(a.fogColor, b.fogColor, t);
      f.fogDensity = Lerp(a.fogDensity, b.fogDensity, t);
      f.fogStart = Lerp(a.fogStart, b.fogStart, t);
      f.heightFogDensity = Lerp(a.heightFogDensity, b.heightFogDensity, t);
      f.skyFogClamp = Lerp(a.skyFogClamp, b.skyFogClamp, t);
      f.sunInscatterStrength = Lerp(a.sunInscatterStrength, b.sunInscatterStrength, t);
    };

    const OffScreen::FogParams dark = MakeDarkFog(from);
    const OffScreen::FogParams battle = MakeBattleFog(from);

    seq->Tween([blendFog, from, dark](float t) { blendFog(from, dark, t); },
               0.0f, 1.0f, 0.0f, s.fogInDuration, Easing::Function::EaseInOutSine);
    seq->Tween([blendFog, dark, battle](float t) { blendFog(dark, battle, t); },
               0.0f, 1.0f, s.fogLiftStart, s.fogLiftStart + s.fogLiftDuration,
               Easing::Function::EaseOutCubic);
  }

  // --- 霧が濃くなった頃、カメラ正面にボスを生成して実体化させる ---
  const std::string bossId = s.bossEnemyId;
  const float emergence = s.emergenceDuration;
  seq->Callback(s.spawnTime, [this, bossId, spawnPos, emergence]() {
    battleEnemyManager_->SpawnBattleEnemy(bossId, spawnPos);
    battleEnemyManager_->BeginBossPhase(); // ここから最終戦のボス戦として続行
    if (auto *spawned = battleEnemyManager_->GetEnemyById(bossId)) {
      spawned->SetPlayer(player_);
      if (auto *boss = dynamic_cast<BossEnemy *>(spawned)) {
        boss->BeginEmergence(emergence);
      }
    }
  });

  // --- 終了：操作を返してボス戦開始 ---
  seq->OnFinish([this]() {
    bossAppearanceActive_ = false;
    EndBossAppearanceStaging();
  });

  YoRigine::CinematicManager::GetInstance()->Play(std::move(seq));
  Logger("[BattleScene] ボス出現演出を開始: " + bossId + "\n");
  return true;
}

Vector3 BattleScene::ComputeBossAppearanceStandPosition(const Vector3 &spawnPos) const {
  // 出現位置へ向かう直線上で、playerStandDistance だけ手前の点。
  // 既にそれより近ければ（または真上に居れば）今の位置のまま向きだけ変える。
  const Vector3 playerPos = player_->GetWT().translate_;
  Vector3 toSpawn = spawnPos - playerPos;
  toSpawn.y = 0.0f;
  const float dist = Length(toSpawn);
  const float stand = std::max(bossAppearance_.playerStandDistance, 0.0f);
  if (dist <= stand || dist < 0.01f) {
    return playerPos;
  }
  Vector3 standPos = spawnPos - toSpawn * (stand / dist);
  standPos.y = playerPos.y;
  return standPos;
}

Vector3 BattleScene::ComputeBossAppearanceFocus() const {
  // ボスが居ればその胸、まだ居なければ出現位置の少し上（霧の中の“何か”を見ている構図）
  if (battleEnemyManager_) {
    if (auto *boss = dynamic_cast<BossEnemy *>(
            battleEnemyManager_->GetEnemyById(bossAppearance_.bossEnemyId))) {
      Vector3 focus{};
      float height = 0.0f;
      if (boss->GetCameraFraming(focus, height)) {
        return focus;
      }
    }
  }
  return bossAppearanceSpawnPos_ + Vector3{0.0f, bossAppearance_.cameraLookHeight, 0.0f};
}

PlayerCamera::CinematicFocusParams BattleScene::MakeBossAppearanceCameraParams() const {
  const auto &s = bossAppearance_;
  PlayerCamera::CinematicFocusParams params;
  params.shoulderYaw = s.cameraShoulderYaw;
  params.pitch = s.cameraPitch;
  params.blendSpeed = s.cameraBlendSpeed;
  return params;
}

void BattleScene::UpdateBossAppearanceStaging() {
  if (!bossAppearanceActive_ || !player_) {
    return;
  }
  // 注視点はボスの生成・実体化に合わせて毎フレーム更新する（カメラ側で滑らかに追う）
  const Vector3 focus = ComputeBossAppearanceFocus();
  if (auto *camera = player_->GetPlayerCamera()) {
    camera->SetCinematicFocusPoint(focus);
  }
  // 立ち止まった後は、出現したボスの方へ体を向け続ける
  Vector3 face = focus;
  face.y = player_->GetWT().translate_.y;
  player_->SetScriptedFaceTarget(face);
}

void BattleScene::EndBossAppearanceStaging() {
  if (!player_) {
    return;
  }
  // 自動移動を解いてから操作を返す（順番が逆だと 1 フレーム入力が混ざる）
  player_->EndScriptedMove();
  if (auto *camera = player_->GetPlayerCamera()) {
    camera->EndCinematicFocus();
  }
  player_->SetControlEnabled(true);
  player_->SetInvincible(false);
}

Vector3 BattleScene::ComputeBossSpawnPosition() const {
  // カメラを振り向かせずに済み、ボスが画面中央に現れる
  const float cameraYaw = sceneCamera_ ? sceneCamera_->transform_.rotate.y : 0.0f;
  const Vector3 forward = {std::sin(cameraYaw), 0.0f, std::cos(cameraYaw)};
  Vector3 pos = player_ ? player_->GetWT().translate_ : Vector3{};
  pos += forward * bossAppearance_.spawnDistance;
  pos.y = 0.0f;
  return pos;
}

void BattleScene::RemoveBosses() {
  if (!battleEnemyManager_) {
    return;
  }
  for (auto *enemy : battleEnemyManager_->GetActiveBattleEnemies()) {
    if (dynamic_cast<BossEnemy *>(enemy)) {
      battleEnemyManager_->RemoveEnemy(enemy);
    }
  }
}

void BattleScene::SpawnBossNow() {
  const std::string &id = bossAppearance_.bossEnemyId;
  if (!battleEnemyManager_ || !battleEnemyManager_->IsBattleActive() ||
      !battleEnemyManager_->HasEnemyData(id)) {
    Logger("[BattleScene] ボスを出せません（バトル外か、敵IDが無い）: " + id + "\n");
    return;
  }
  RemoveBosses();
  battleEnemyManager_->SpawnBattleEnemy(id, ComputeBossSpawnPosition());
  if (auto *spawned = battleEnemyManager_->GetEnemyById(id)) {
    spawned->SetPlayer(player_);
  }
}

void BattleScene::ReplayBossAppearance() {
  if (!battleEnemyManager_ || !battleEnemyManager_->IsBattleActive()) {
    Logger("[BattleScene] バトル中でないため出現演出を再生できません\n");
    return;
  }
  // 既にいるボスを消し、霧を演出前へ戻してから流し直す
  RemoveBosses();
  if (bossAppearanceActive_) {
    YoRigine::CinematicManager::GetInstance()->Stop();
    bossAppearanceActive_ = false;
    // 演出の OnFinish は Stop では呼ばれないので、自動移動と注視をここで解く
    EndBossAppearanceStaging();
  }
  RestoreFogAfterBossAppearance();
  bossPhaseStarted_ = false;
  StartBossAppearance();
}

OffScreen::FogParams BattleScene::MakeDarkFog(const OffScreen::FogParams &base) const {
  const auto &s = bossAppearance_;
  OffScreen::FogParams dark = base;
  dark.fogColor = s.darkFogColor;
  dark.fogDensity = s.darkFogDensity;
  dark.fogStart = s.darkFogStart;
  dark.heightFogDensity = s.darkHeightFogDensity;
  dark.skyFogClamp = s.darkSkyFogClamp;
  dark.sunInscatterStrength = 0.0f; // 太陽のにじみを消して、光の無い闇にする
  return dark;
}

OffScreen::FogParams BattleScene::MakeBattleFog(const OffScreen::FogParams &base) const {
  const auto &s = bossAppearance_;
  OffScreen::FogParams battle = MakeDarkFog(base);
  battle.fogColor = s.battleFogColor;
  battle.fogDensity = s.battleFogDensity;
  battle.fogStart = s.battleFogStart;
  battle.heightFogDensity = s.battleHeightFogDensity;
  battle.skyFogClamp = s.battleSkyFogClamp;
  return battle;
}

void BattleScene::PreviewBossFog(float t) {
  auto *chain = PostEffectManager::GetInstance()->GetEffectChain();
  auto *fog = chain ? chain->GetFirstEffectByType(OffScreen::OffScreenEffectType::Fog)
                    : nullptr;
  if (!fog) {
    return;
  }
  // 初回だけ演出前の霧を退避する（「霧を元に戻す」で戻す先）
  if (!hasSavedFog_) {
    savedFog_ = fog->params.fog;
    hasSavedFog_ = true;
  }
  const OffScreen::FogParams dark = MakeDarkFog(savedFog_);
  const OffScreen::FogParams battle = MakeBattleFog(savedFog_);
  const OffScreen::FogParams &a = (t <= 1.0f) ? savedFog_ : dark;
  const OffScreen::FogParams &b = (t <= 1.0f) ? dark : battle;
  const float u = std::clamp(t <= 1.0f ? t : t - 1.0f, 0.0f, 1.0f);

  auto &f = fog->params.fog;
  f.fogColor = Lerp(a.fogColor, b.fogColor, u);
  f.fogDensity = Lerp(a.fogDensity, b.fogDensity, u);
  f.fogStart = Lerp(a.fogStart, b.fogStart, u);
  f.heightFogDensity = Lerp(a.heightFogDensity, b.heightFogDensity, u);
  f.skyFogClamp = Lerp(a.skyFogClamp, b.skyFogClamp, u);
  f.sunInscatterStrength = Lerp(a.sunInscatterStrength, b.sunInscatterStrength, u);
}

void BattleScene::RestoreFogAfterBossAppearance() {
  if (!hasSavedFog_) {
    return;
  }
  auto *chain = PostEffectManager::GetInstance()->GetEffectChain();
  if (auto *fog = chain ? chain->GetFirstEffectByType(OffScreen::OffScreenEffectType::Fog)
                        : nullptr) {
    fog->params.fog = savedFog_;
  }
  hasSavedFog_ = false;
}

void BattleScene::FocusNearestEnemyAfterDefeat() {
  if (!player_ || !player_->GetPlayerCamera() || !battleEnemyManager_)
    return;

  BattleEnemyBase *nearest =
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
void BattleScene::DrawVfx() {
  if (battleEnemyManager_) {
    battleEnemyManager_->DrawVfx();
  }
}

void BattleScene::DrawShadow() {
  player_->DrawShadow();
  battleEnemyManager_->DrawShadow();
}

/// <summary>
/// シーン遷移時に呼ばれる：状態をリセットしてから、遭遇データで戦闘を開始する。
/// </summary>
void BattleScene::OnEnter(const SubScenePayload &payload) {
  BaseSubScene::OnEnter(payload);
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
  bossPhaseStarted_ = false;
  bossAppearanceActive_ = false;

  // プレイヤー位置リセット
  player_->SetInitialPosition();

  // エンカウントからの入場なら、遭遇データで敵を配置して開始演出へ
  if (const auto *battleData = std::get_if<BattleTransitionData>(&payload)) {
    StartBattle(*battleData);
  }

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
    player_->GetPlayerCamera()->ClearBossFraming();
    player_->GetPlayerCamera()->SetThreatAwarenessAllowed(false);
  }

#ifdef USE_IMGUI
  bossEditor_.StopPreview(); // プレビューで止めた AI・姿勢を戻す
#endif

  // ボス出現演出で暗くした霧を戻す（敗北・リトライでフィールドへ戻る場合など）
  if (bossAppearanceActive_) {
    YoRigine::CinematicManager::GetInstance()->Stop();
    bossAppearanceActive_ = false;
    // 演出の OnFinish は Stop では呼ばれないので、自動移動と注視をここで解く
    EndBossAppearanceStaging();
  }
  RestoreFogAfterBossAppearance();

  // ロックオンUIを非表示にする
  lockOnUI_->SetIsVisible(false);
  // シーンから抜ける時に、強制的に敵を全削除する
  if (battleEnemyManager_) {
    battleEnemyManager_->RemoveAllBattleEnemies();
  }
  Logger("[BattleScene] ===== OnExit() END =====\n");
}

/// <summary>
/// バトル開始エントリポイント。
/// OnEnter で受け取った BattleTransitionData から敵を配置する。
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

  // クリア演出の終了コールバックは this を参照する。
  // 演出の途中でシーンが破棄された場合に備え、再生中なら止めておく。
  if (clearCinematicStarted_) {
    YoRigine::CinematicManager::GetInstance()->Stop();
  }

#ifdef USE_IMGUI
  // 解除しないと破棄済みの this を指したままコールバックが呼ばれる
  if (motionGizmoCallbackId_ != 0) {
    Editor::GetInstance()->RemoveGizmoDrawCallback(motionGizmoCallbackId_);
    motionGizmoCallbackId_ = 0;
  }
#endif
}
