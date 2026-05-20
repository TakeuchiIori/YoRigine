#include "BattleScene.h"
#include "SceneSyncData.h"

// Engine
#include <SceneSystems/SceneManager.h>
#include "Systems./Input./Input.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include "Particle/ParticleManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif
#include <Debugger/Logger.h>

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
	manager->AddArea("BattleField", battleField);


	//------------------------------------------------------------
	// UI初期化
	//------------------------------------------------------------
	lockOnUI_ = std::make_unique<LockOnUI>();
	lockOnUI_->Initialize(player_);

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
	if (!isBattleCameraActive && battleEnemyManager_->IsFinalBattleCleared()) {
		SceneManager::GetInstance()->ChangeScene("Clear");
		battleEnemyManager_->ResetFinalBattleClearFlag();
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
	if (battleEnemyManager_) {
		battleEnemyManager_->DrawCollision();
	}

	player_->DrawCollision();
	player_->DrawBone(*line_.get());
	//AreaManager::GetInstance()->Draw(line_.get());
#endif
}

/// <summary>
/// UI描画
/// </summary>
void BattleScene::DrawUI() {
	//sprite_->Draw();
	if (battleEnemyManager_) {
		battleEnemyManager_->DrawUI();
		lockOnUI_->Draw();
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
/// シーン遷移時に呼ばれる：バトル開始処理
/// </summary>
void BattleScene::OnEnter() {
	BaseSubScene::OnEnter();

	Logger("[BattleScene] ===== OnEnter() START =====\n");

	// カメラリセット
	//currentCameraMode_ = CameraMode::BATTLE_START;
	currentCameraMode_ = CameraMode::FOLLOW;
	battleCameraFinished_ = true;
	shouldResetBattleCamera_ = true;

	// プレイヤー位置リセット
	player_->SetInitialPosition();

	//------------------------------------------------------------
	// バトル遷移データの読み込み
	//------------------------------------------------------------
	auto* syncData = SceneSyncData::GetInstance();
	if (syncData->HasBattleTransitionData()) {
		BattleTransitionData transitionData = syncData->LoadBattleTransitionData();
		originalTransitionData_ = transitionData;
		currentEnemyGroup_ = transitionData.enemyGroup;

		// 最終バトルフラグを設定
		isFinalBattle_ = transitionData.isFinalBattle;
		totalRemainingFieldEnemies_ = transitionData.totalRemainingFieldEnemies;  // ★追加

		char debugBuffer[256];
		sprintf_s(debugBuffer, "[BattleScene] isFinalBattle: %s, Remaining groups: %zu\n",
			isFinalBattle_ ? "TRUE" : "FALSE", totalRemainingFieldEnemies_);
		Logger(debugBuffer);

		if (isFinalBattle_) {
			Logger("[BattleScene] ★★★ FINAL BATTLE FLAG SET! ★★★\n");
		}

		// 敵出現設定
		EnemyEncounterData encounter;
		encounter.encounterName = transitionData.enemyGroup + "_Battle";

		// 複数体対応
		if (!transitionData.battleEnemyIds.empty()) {
			encounter.enemyIds = transitionData.battleEnemyIds;
			Logger("[BattleScene] 複数体バトル開始\n");
		}
		else {
			encounter.enemyIds = { transitionData.battleEnemyId };
			Logger("[BattleScene] 単体バトル開始\n");
		}

		// フォーメーション設定
		if (!transitionData.battleFormation.empty() && battleEnemyManager_) {
			auto formation = battleEnemyManager_->GetFormation(transitionData.battleFormation);
			if (!formation.positions.empty()) {
				encounter.formations = formation.positions;
			}
		}

		// デフォルト配置
		if (encounter.formations.empty()) {
			encounter.formations = battleEnemyManager_->GetFormationPositions(encounter.enemyIds.size());
		}

		// バトル開始時のスタート演出カメラの再生
		player_->GetFollowCamera()->PlayBattleStart();
		// 戦闘開始
		if (battleEnemyManager_) {
			// BattleEnemyManagerに最終バトル情報を渡す
			battleEnemyManager_->SetFinalBattleMode(isFinalBattle_);
			battleEnemyManager_->StartBattle(encounter);
		}

		syncData->ClearBattleTransitionData();
		Logger("[BattleScene] バトルデータ設定完了\n");
	}
	else {
		Logger("[BattleScene] エラー: バトル遷移データが存在しません\n");
	}

	Logger("[BattleScene] ===== OnEnter() END =====\n");
}

/// <summary>
/// シーンを抜ける時に呼ばれる：結果の保存処理
/// </summary>
void BattleScene::OnExit() {
	BaseSubScene::OnExit();

	Logger("[BattleScene] ===== OnExit() START =====\n");

	auto* syncData = SceneSyncData::GetInstance();
	nlohmann::json battleState;

	battleState["currentEnemyGroup"] = currentEnemyGroup_;

	if (battleEnemyManager_) {
		const BattleStats& stats = battleEnemyManager_->GetBattleStats();
		battleState["stats"]["expGained"] = stats.totalExpGained;
		battleState["stats"]["goldGained"] = stats.totalGaldGained;
		battleState["stats"]["enemiesDefeated"] = stats.enemiesDefeated;
		battleState["stats"]["battleDuration"] = stats.battleDuration;

		nlohmann::json itemsArray = nlohmann::json::array();
		for (const auto& item : stats.droppedItems) {
			itemsArray.push_back(item);
		}
		battleState["stats"]["droppedItems"] = itemsArray;
	}
	battleState["playerHpRatio"] = 1.0f;
	syncData->SaveCurrentSceneState("Battle", battleState);


	// シーンから抜ける時に、強制的に敵を全削除する
	if (battleEnemyManager_) {
		battleEnemyManager_->RemoveAllBattleEnemies();
	}
	currentCameraMode_ = CameraMode::FOLLOW;

	Logger("[BattleScene] ===== OnExit() END =====\n");
}

/// <summary>
/// 即時バトル開始（外部から直接呼ばれる）
/// </summary>
void BattleScene::StartBattle(const BattleTransitionData& data) {
	Logger("[BattleScene] ===== StartBattle() START =====\n");

	originalTransitionData_ = data;
	currentEnemyGroup_ = data.enemyGroup;
	SavePlayerState(data);

	EnemyEncounterData encounter;
	encounter.encounterName = data.enemyGroup + "_Individual";
	encounter.enemyIds = { data.battleEnemyId };
	encounter.formations = { Vector3(0.0f, 0.0f, 5.0f) };

	if (battleEnemyManager_) {
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

	SceneSyncData::GetInstance()->SaveFieldReturnData(returnData);

	const char* resultStr =
		(result == BattleResult::Victory) ? "Victory" :
		(result == BattleResult::Defeat) ? "Defeat" : "Other";

	char buffer[256];
	sprintf_s(buffer, "[BattleScene] 結果: %s, EXP:%d, Gold:%d\n",
		resultStr, stats.totalExpGained, stats.totalGaldGained);
	Logger(buffer);

	if (battleEndCallback_) {
		battleEndCallback_(returnData, result, stats);
	}

	battleEnemyManager_->ForceBattleEnd();

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