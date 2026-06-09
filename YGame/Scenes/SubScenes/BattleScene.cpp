#include "BattleScene.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Systems./Input./Input.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Cinematic/CinematicManager.h"
#include "Systems/Cinematic/CinematicSequencer.h"
#include "Systems/Camera/CameraDirector.h"
#include "Systems/Camera/Virtuals/KeyframeCamera/KeyframeCamera.h"
#include "OffScreen/PostEffectManager.h"
#include "OffScreen/PostEffectChain.h"
#include <Editor/Editor.h>

#include <UI/Damage/DamageNumberManager.h>
#include <ModelManipulator/ModelManipulator.h>
#include <algorithm>
#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Debugger/Logger.h>

namespace {
	// ===== 霧晴れカットシーン演出のタイムライン定数 =====
	// 時間はすべて秒。Sequencer の全体時間 (duration) は KeyframeCamera のパス長から自動取得。
	constexpr float kLetterboxInDuration       = 1.0f;     // 黒帯がスライドインする時間
	constexpr float kLetterboxOutDuration      = 1.0f;     // 黒帯がスライドアウトする時間
	constexpr float kPostEffectFadeDelay       = 0.5f;     // 黒帯 IN 完了後にエフェクト開始までの余白
	constexpr float kPostEffectFadeEndMargin   = 0.5f;     // 黒帯 OUT 開始前にエフェクトを終わらせる余白
	constexpr float kGodRaysExposureMul        = 1.5f;     // 演出中の GodRays 露光倍率
	constexpr int   kCinematicCamPriority      = 1000;     // 演出中のカメラ優先度
	constexpr float kFallbackDuration          = 5.0f;     // ClearCinematic 不在時の保険
	constexpr const char* kCinematicCamName    = "ClearCinematic";
	constexpr const char* kClearSceneName      = "Clear";
}

/// <summary>
/// バトルシーン初期化
/// </summary>
void BattleScene::Initialize(Camera* camera, Player* player) {

	sceneCamera_ = camera;
	player_ = player;
	player->Reset();

	//------------------------------------------------------------
	// バトル敵管理システム初期化
	//------------------------------------------------------------
	battleEnemyManager_ = std::make_unique<BattleEnemyManager>();
	battleEnemyManager_->Initialize(sceneCamera_);
	battleEnemyManager_->SetPlayer(player_);

	// 戦闘終了時のコールバック設定
	battleEnemyManager_->SetBattleEndCallback([this](BattleResult result, const BattleStats& stats) {
		HandleBattleEnd(result, stats);
		});

	//------------------------------------------------------------
	// 環境オブジェクト初期化
	//------------------------------------------------------------
	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(sceneCamera_);

	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_);
	ground_->GetColor() = { 0.5f, 0.2f, 0.5f, 1.0f };

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize("Resources/Textures/GameScene/BattleScene.png");
	sprite_->SetTranslate({ 200.0f, 0.0f, 0.0f });


	auto manager = AreaManager::GetInstance();

	//------------------------------------------------------------
	// フィールドの設定
	//------------------------------------------------------------
	auto battleField = std::make_shared<CircleArea>();
	battleField->Initialize(Vector3(0, 0, 0), 50.0f);
	battleField->SetPurpose(AreaPurpose::Boundary);  // 境界制限として設定
	battleField->SetCamera(sceneCamera_);
	manager->AddArea("BattleArea", battleField);


	//------------------------------------------------------------
	// UI初期化
	//------------------------------------------------------------
	lockOnUI_ = std::make_unique<LockOnUI>();
	lockOnUI_->Initialize(player_);

	DamageNumberManager::GetInstance()->Initialize();

	//------------------------------------------------------------
	// プレイヤーをエリア制限対象として登録
	//------------------------------------------------------------
	manager->RegisterObject(&player_->GetWT(), "Player");

	// デバッグ描画を有効化
	manager->SetDebugDrawEnabled(true);

#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("バトルモード:デバッグ情報", [this]() { battleEnemyManager_->ShowDebugInfo(); }, "Game");
#endif
}

/// <summary>
/// 更新処理
/// </summary>
void BattleScene::Update() {

	// カメラ終了チェック
	if (currentCameraMode_ == CameraMode::BATTLE_START && battleCameraFinished_) {
		currentCameraMode_ = CameraMode::FOLLOW;
	}
	bool isBattleCameraActive = (currentCameraMode_ == CameraMode::BATTLE_START && !battleCameraFinished_);

	// 最終バトルクリア検知（BattleScene側で遷移したいならここ）
	// 直接 ChangeScene せず、霧晴れ＋光芒のカットシーン演出を挟んでから遷移
	if (!isBattleCameraActive && battleEnemyManager_->IsFinalBattleCleared() && !clearCinematicStarted_) {
		clearCinematicStarted_ = true;
		battleEnemyManager_->ResetFinalBattleClearFlag();

		// チェーンから Fog / GodRays を引っ張る（無ければ tween をスキップ）
		auto* chain = PostEffectManager::GetInstance()->GetEffectChain();
		auto* fog     = chain ? chain->GetFirstEffectByType(OffScreen::OffScreenEffectType::Fog)     : nullptr;
		auto* godRays = chain ? chain->GetFirstEffectByType(OffScreen::OffScreenEffectType::GodRays) : nullptr;

		// シーケンス全体時間 = KeyframeCamera "ClearCinematic" のパス長
		// 編集で長さを変えるだけで letterbox / エフェクトの時刻が自動で追従する
		auto cinemaCam = std::dynamic_pointer_cast<KeyframeCamera>(
			CameraDirector::GetInstance()->GetCamera(kCinematicCamName));
		const float duration = (cinemaCam && cinemaCam->GetMaxTime() > 0.0f)
			? cinemaCam->GetMaxTime()
			: kFallbackDuration;

		// エフェクトフェード時刻（黒帯 OUT 開始前に余白を確保して終わらせる）
		const float fadeStart = kPostEffectFadeDelay;
		const float fadeEnd   = std::max(
			fadeStart + 0.1f,
			duration - kLetterboxOutDuration - kPostEffectFadeEndMargin);

		auto seq = std::make_unique<YoRigine::CinematicSequencer>(duration);

		// --- レターボックス ---
		seq->Letterbox(true,  0.0f, kLetterboxInDuration,
			Easing::Function::EaseOutCubic);
		seq->Letterbox(false, duration - kLetterboxOutDuration, duration,
			Easing::Function::EaseInCubic);

		// --- Fog を 0 に補間（density / heightDensity / sunInscatter）---
		if (fog) {
			const auto& f = fog->params.fog;
			seq->Tween([fog](float v){ fog->params.fog.fogDensity = v; },
				f.fogDensity, 0.0f, fadeStart, fadeEnd, Easing::Function::EaseOutCubic);

			seq->Tween([fog](float v){ fog->params.fog.heightFogDensity = v; },
				f.heightFogDensity, 0.0f, fadeStart, fadeEnd, Easing::Function::EaseOutCubic);

			seq->Tween([fog](float v){ fog->params.fog.sunInscatterStrength = v; },
				f.sunInscatterStrength, 0.0f, fadeStart, fadeEnd, Easing::Function::EaseOutCubic);
		}

		// --- GodRays exposure を倍率倍へ（光が射す印象を強化）---
		if (godRays) {
			const float curExposure = godRays->params.godRays.exposure;
			seq->Tween([godRays](float v){ godRays->params.godRays.exposure = v; },
				curExposure, curExposure * kGodRaysExposureMul,
				fadeStart, fadeEnd, Easing::Function::EaseInOutSine);
		}

		// --- KeyframeCamera をシーン全体で再生 ---
		// 演出後は Clear シーンへ遷移するので、復帰先カメラは指定しない
		seq->Camera(kCinematicCamName, 0.0f, duration, kCinematicCamPriority);

		// 演出終了 → Clear へ遷移
		seq->OnFinish([](){ SceneManager::GetInstance()->ChangeScene(kClearSceneName); });

		YoRigine::CinematicManager::GetInstance()->Play(std::move(seq));
		return;
	}

	// 演出中は他のロジックを止める（プレイヤー操作・カメラ・敵更新ロック）
	if (clearCinematicStarted_) {
		return;
	}

	// 敵更新
	if (!isBattleCameraActive) {
		battleEnemyManager_->Update();
	}

	// プレイヤー更新
	if (!isBattleCameraActive && !battleEnemyManager_->IsFinalBattleCleared()) {
		player_->Update();
	}


	// 視覚効果・オブジェクト更新
	sprite_->Update();
	ground_->Update();

	// UI更新
	lockOnUI_->Update();
	DamageNumberManager::GetInstance()->Update(YoRigine::GameTime::GetDeltaTime(),sceneCamera_->GetViewProjectionMatrix());

	// エリア制限補正
	AreaManager::GetInstance()->UpdateSingleObject(&player_->GetWT());
}


/// <summary>
/// 3Dオブジェクト描画
/// </summary>
void BattleScene::DrawObject() {
	ground_->Draw();

	if (battleEnemyManager_) {
		battleEnemyManager_->Draw();
	}

	player_->Draw();
	player_->DrawAnimation();
}

/// <summary>
/// デバッグライン描画
/// </summary>
void BattleScene::DrawLine() {
#ifdef USE_IMGUI
	// フレーム冒頭の頂点・マテリアル CB スロットのリセット (複数の DrawLine() 呼出の干渉を避ける)
	line_->Reset();

	if (battleEnemyManager_) {
		battleEnemyManager_->DrawCollision();
	}

	player_->DrawCollision();
	player_->DrawBone(*line_.get());
	AreaManager::GetInstance()->DrawArea("BattleArea", line_.get());

#endif
}

/// <summary>
/// UI描画
/// </summary>
void BattleScene::DrawUI() {
	//sprite_->Draw();
	if (battleEnemyManager_) {
		battleEnemyManager_->DrawUI();
		DamageNumberManager::GetInstance()->Draw();
		
		if (player_->IsAlive()) {
			lockOnUI_->Draw();
		}
	}
}

void BattleScene::DrawNonOffscreen()
{
}

void BattleScene::DrawShadow()
{
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

	Logger("[BattleScene] ===== OnEnter() START =====\n");

	// バトル用 ModelManipulator シーンへ切替。
	// Battle.json が無い場合は空シーンになる。初回は GameScene.json から
	// バトル用に置くものだけ残して保存して Battle.json を作る。
	YoRigine::ModelManipulator::GetInstance()->LoadScene("Battle");

	// カメラリセット
	currentCameraMode_ = CameraMode::FOLLOW;
	battleCameraFinished_ = true;
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

	Logger("[BattleScene] ===== OnExit() START =====\n");

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
/// SubSceneManager がフェード演出の途中でこの関数に BattleTransitionData を渡す。
/// </summary>
void BattleScene::StartBattle(const BattleTransitionData& data) {
	Logger("[BattleScene] ===== StartBattle() START =====\n");

	originalTransitionData_ = data;
	currentEnemyGroup_ = data.enemyGroup;
	isFinalBattle_ = data.isFinalBattle;
	totalRemainingFieldEnemies_ = data.totalRemainingFieldEnemies;

	char debugBuffer[256];
	sprintf_s(debugBuffer, "[BattleScene] isFinalBattle: %s, Remaining groups: %zu\n",
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
	}
	else {
		encounter.enemyIds = { data.battleEnemyId };
		Logger("[BattleScene] 単体バトル開始\n");
	}

	// フォーメーション設定
	if (!data.battleFormation.empty() && battleEnemyManager_) {
		auto formation = battleEnemyManager_->GetFormation(data.battleFormation);
		if (!formation.positions.empty()) {
			encounter.formations = formation.positions;
		}
	}

	// デフォルト配置
	if (encounter.formations.empty() && battleEnemyManager_) {
		encounter.formations = battleEnemyManager_->GetFormationPositions(encounter.enemyIds.size());
	}

	// バトル開始演出
	player_->GetPlayerCamera()->PlayBattleStart();

	if (battleEnemyManager_) {
		battleEnemyManager_->SetFinalBattleMode(isFinalBattle_);
		battleEnemyManager_->StartBattle(encounter);
	}

	Logger("[BattleScene] ===== StartBattle() END =====\n");
}

/// <summary>
/// 戦闘終了時の処理
/// </summary>
void BattleScene::HandleBattleEnd(BattleResult result, const BattleStats& stats) {
	Logger("[BattleScene] ===== HandleBattleEnd() START =====\n");

	char debugBuffer[512];
	sprintf_s(debugBuffer, "[BattleScene] isFinalBattle_: %s, result: %d (Victory=1)\n",
		isFinalBattle_ ? "TRUE" : "FALSE", static_cast<int>(result));
	Logger(debugBuffer);

	//------------------------------------------------------------
	// 最終バトルで勝利した場合、クリアシーンへ直接遷移
	//------------------------------------------------------------
	if (isFinalBattle_ && result == BattleResult::Victory) {
		Logger("[BattleScene] ★★★ Final Battle Victory! Transitioning to Clear Scene ★★★\n");

		// 注意: BattleEnemyManagerのスロー演出処理で既にクリアシーンへ遷移済み
		// このコードは念のための保険（通常は到達しない）
		Logger("[BattleScene] Already handled by BattleEnemyManager slow motion\n");

		Logger("[BattleScene] ===== HandleBattleEnd() END (Clear Scene) =====\n");
		return;  // フィールドに戻るコールバックを実行しない
	}
	else {
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

	const char* resultStr =
		(result == BattleResult::Victory) ? "Victory" :
		(result == BattleResult::Defeat) ? "Defeat" : "Other";

	char buffer[256];
	sprintf_s(buffer, "[BattleScene] 結果: %s, EXP:%d, Gold:%d\n",
		resultStr, stats.totalExpGained, stats.totalGaldGained);
	Logger(buffer);

	battleEnemyManager_->ForceBattleEnd();

	// SubSceneManager 経由でフィールドへ戻る（in-memory コピーで FieldReturnData を渡す）
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
void BattleScene::SavePlayerState([[maybe_unused]] const BattleTransitionData& data) {
	if (player_) {
		Logger("[BattleScene] Player state saved\n");
	}
}

/// <summary>
/// 戦闘終了時の戻りデータを作成
/// </summary>
void BattleScene::CreateBattleReturnData(FieldReturnData& data, BattleResult result, const BattleStats& stats) {
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
}