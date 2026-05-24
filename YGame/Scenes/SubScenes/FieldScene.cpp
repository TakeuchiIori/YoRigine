#include "FieldScene.h"
#include "SceneSyncData.h"

// Engine
#include "Systems./Input./Input.h"
#include "Particle./ParticleManager.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Systems/GameTime/GameTime.h"
#include <Editor/Editor.h>
#include "Debugger/Logger.h"
#include <Object3D/ObjectManager.h>
#include "Collision/AreaCollision/Base/AreaManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

/*==========================================================================
	初期化
//========================================================================*/
void FieldScene::Initialize(Camera* camera, Player* player) {

	sceneCamera_ = camera;
	player_ = player;
	player_->Reset();

	//------------------------------------------------------------
	// オブジェクト初期化
	//------------------------------------------------------------
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_);

	emitter_ = std::make_unique<ParticleEmitter>("TestParticle", Vector3{ 0.0f, 0.0f, 0.0f }, 5);

	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(sceneCamera_);

	//------------------------------------------------------------
	// フィールド敵管理システム初期化
	//------------------------------------------------------------
	fieldEnemyManager_ = std::make_unique<FieldEnemyManager>();
	fieldEnemyManager_->Initialize(sceneCamera_);
	fieldEnemyManager_->SetPlayer(player_);

	// NavGridConfig をJSONから読み込み（なければデフォルト値を使用）
	navGridConfig_.aj_.LoadFromFile(navGridConfig_.kDefaultPath); // AutoJson の SaveToFile/LoadFromFile を直接利用

	// NavGrid 初期化＆ベイク（全設定値がJSONから取得される）
	navGrid_.Initialize(
		navGridConfig_.worldMinX, navGridConfig_.worldMaxX,
		navGridConfig_.worldMinZ, navGridConfig_.worldMaxZ,
		navGridConfig_.cellSize);
	navGrid_.SetAgentRadius(navGridConfig_.agentRadius);
	navGrid_.Bake(ObjectManager::GetInstance());
	navPathfinder_.SetNavGrid(&navGrid_);

	// FieldEnemyManager に NavPathfinder を渡す
	fieldEnemyManager_->SetNavPathfinder(&navPathfinder_);

	// 敵エンカウント時の詳細コールバック登録
	fieldEnemyManager_->SetEncounterDetailCallback([this](const EncountInfo& encounterInfo) {
		HandleDetailedEncounter(encounterInfo);
		});

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize("Resources/Textures/GameScene/FieldScene.png");



	battleFieldArea_ = std::make_unique<CircleArea>();
	battleFieldArea_->Initialize(Vector3(0, 0, 0), 50.0f);
	battleFieldArea_->SetCamera(sceneCamera_);
	AreaManager::GetInstance()->AddArea("BattleArea", battleFieldArea_);

#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("フィールドモード:デバッグ情報",
		[this]() { fieldEnemyManager_->ShowDebugInfo(); }, "Game");

	// NavGridConfig エディター（Editor::RegisterGameUI で登録）
	Editor::GetInstance()->RegisterGameUI("NavGrid Config", [this]() {
		ImGui::Checkbox("NavGrid デバッグ表示", &showNavGridDebug_);
		ImGui::SameLine();
		ImGui::TextDisabled("赤=通行不可  白=範囲境界");
		ImGui::Separator();
		if (ImGui::CollapsingHeader("NavGrid Config")) {
			navGridConfig_.aj_.ShowImGui("NavGridConfig");
			ImGui::Separator();
			if (ImGui::Button("Rebake NavGrid")) {
				// 設定を変更したらここでベイクし直す
				navGrid_.Initialize(
					navGridConfig_.worldMinX, navGridConfig_.worldMaxX,
					navGridConfig_.worldMinZ, navGridConfig_.worldMaxZ,
					navGridConfig_.cellSize);
				navGrid_.SetAgentRadius(navGridConfig_.agentRadius);
				navGrid_.Bake(ObjectManager::GetInstance());
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Config")) {
				navGridConfig_.aj_.SaveToFile(navGridConfig_.kDefaultPath);
			}
		}
		}, "Game");
#endif
}

/*==========================================================================
	更新処理
//========================================================================*/
void FieldScene::Update() {

	player_->Update();
	ground_->Update();
	fieldEnemyManager_->Update();
	//sprite_->Update();
	AreaManager::GetInstance()->UpdateSingleObject(&player_->GetWT());


	//------------------------------------------------------------
	// ログの出力用
	//------------------------------------------------------------
#ifdef _DEBUG
	// 変化検出用
	static bool prevFinalBattle = false;
	size_t remainingGroups = fieldEnemyManager_->GetActiveEncounterGroupCount();
	// 現在の判定
	bool isFinalBattle = (remainingGroups <= 1);
	// 状態が変わった時だけログを出す
	if (isFinalBattle != prevFinalBattle) {
		if (isFinalBattle) {
			Logger("[FieldScene] これは最後のバトルです！\n");
		}
		else {
			Logger("[FieldScene] まだ最後のバトルではありません。\n");
		}
	}
	prevFinalBattle = isFinalBattle;
#endif // _DEBUG
}

/*==========================================================================
	オブジェクトの描画
//========================================================================*/
void FieldScene::DrawObject() {
	player_->Draw();
	ground_->Draw();
	fieldEnemyManager_->Draw();
	player_->DrawAnimation();
}

/*==========================================================================
	線の描画
//========================================================================*/
void FieldScene::DrawLine() {
#ifdef USE_IMGUI
	player_->DrawCollision();
	fieldEnemyManager_->DrawCollision();
	fieldEnemyManager_->DrawLine(line_.get());
	player_->DrawBone(*line_.get());
	battleFieldArea_->Draw(line_.get());

	// ── NavGrid デバッグ描画 ─────────────────────────────────────────────
	// showNavGridDebug_ が true のときだけグリッドを可視化する。
	// 通行不可セル（障害物）は赤の薄いAABB、通行可能は緑の薄い点で描く。
	if (showNavGridDebug_ && navGrid_.IsInitialized()) {
		const int W = navGrid_.GetWidthCells();
		const int D = navGrid_.GetDepthCells();
		const float cs = navGrid_.GetCellSize();
		const float yBase = 0.0f; // 地面のY座標

		for (int gz = 0; gz < D; ++gz) {
			for (int gx = 0; gx < W; ++gx) {
				if (!navGrid_.IsWalkable(gx, gz)) {
					// 通行不可セル → 赤のAABB
					NavGrid::GridPos gp{ gx, gz };
					Vector3 center = navGrid_.GridToWorld(gp);
					Vector3 mn = { center.x - cs * 0.5f, yBase,        center.z - cs * 0.5f };
					Vector3 mx = { center.x + cs * 0.5f, yBase + 0.1f, center.z + cs * 0.5f };
					line_->SetColor({ 1.0f, 0.15f, 0.15f, 0.7f });
					line_->DrawAABB(mn, mx);
					line_->DrawLine();
				}
			}
		}

		// グリッド全体の外枠（白）
		const float minX = navGrid_.GetWorldMinX();
		const float minZ = navGrid_.GetWorldMinZ();
		const float maxX = minX + W * cs;
		const float maxZ = minZ + D * cs;
		line_->SetColor({ 1.0f, 1.0f, 1.0f, 0.4f });
		line_->DrawAABB({ minX, yBase, minZ }, { maxX, yBase + 0.05f, maxZ });
		line_->DrawLine();
	}
#endif
}

/*==========================================================================
	UIの描画
//========================================================================*/
void FieldScene::DrawUI() {
	//sprite_->Draw();
	fieldEnemyManager_->DrawUI();
}

/*==========================================================================
	ポストエフェクトに描画したくないものをここに描画
//========================================================================*/
void FieldScene::DrawNonOffscreen()
{
}

/*==========================================================================
	影の描画
//========================================================================*/
void FieldScene::DrawShadow()
{
	player_->DrawShadow();
	fieldEnemyManager_->DrawShadow();
}

/*==========================================================================
	シーンに入った時の処理
//========================================================================*/
void FieldScene::OnEnter() {
	BaseSubScene::OnEnter();

	Logger("[FieldScene] ===== OnEnter() START =====\n");

	// フィールドの敵を再開
	if (fieldEnemyManager_) {
		fieldEnemyManager_->SetAllEnemiesActive(true);
		fieldEnemyManager_->ResetEnCount();
		currentCameraMode_ = CameraMode::FOLLOW;
	}

	//------------------------------------------------------------
	// バトル終了後の復帰処理
	//------------------------------------------------------------
	auto* syncData = SceneSyncData::GetInstance();
	if (syncData->HasFieldReturnData()) {
		FieldReturnData returnData = syncData->LoadFieldReturnData();
		HandleBattleReturn(returnData);
		syncData->ClearFieldReturnData();
		Logger("[FieldScene] Field return data applied and cleared\n");
	}
	else {
		Logger("[FieldScene] No field return data (first time or fresh start)\n");
	}

	Logger("[FieldScene] ===== OnEnter() END =====\n");
}

/*==========================================================================
	シーンを抜けるときの処理
//========================================================================*/
void FieldScene::OnExit() {
	BaseSubScene::OnExit();

	Logger("[FieldScene] ===== OnExit() START =====\n");

	auto* syncData = SceneSyncData::GetInstance();
	nlohmann::json fieldState;

	//------------------------------------------------------------
	// プレイヤー・カメラ状態の保存
	//------------------------------------------------------------
	if (player_) {
		Vector3 playerPos = player_->GetWorldPosition();
		fieldState["playerPosition"] = { {"x", playerPos.x}, {"y", playerPos.y}, {"z", playerPos.z} };
	}

	if (sceneCamera_) {
		Vector3 cameraPos = sceneCamera_->transform_.translate;
		fieldState["cameraPosition"] = { {"x", cameraPos.x}, {"y", cameraPos.y}, {"z", cameraPos.z} };
	}

	fieldState["cameraMode"] = static_cast<int>(currentCameraMode_);

	//------------------------------------------------------------
	// 現在アクティブな敵情報を保存
	//------------------------------------------------------------
	if (fieldEnemyManager_) {
		auto activeEnemies = fieldEnemyManager_->GetActiveFieldEnemies();
		nlohmann::json enemyList = nlohmann::json::array();
		for (auto* enemy : activeEnemies) {
			Vector3 pos = enemy->GetPosition();
			enemyList.push_back({
				{"id", enemy->GetEnemyGroupName()},
				{"position", {{"x", pos.x}, {"y", pos.y}, {"z", pos.z}}}
				});
		}
		fieldState["activeEnemies"] = enemyList;
		fieldEnemyManager_->SetAllEnemiesActive(false);
	}

	syncData->SaveCurrentSceneState("Field", fieldState);
	Logger("[FieldScene] Field state saved to JSON\n");
	Logger("[FieldScene] ===== OnExit() END =====\n");
}

/*==========================================================================
	カメラモードの切り替え
//========================================================================*/
void FieldScene::UpdateCameraMode() {
#ifdef USE_IMGUI
	if (ImGui::Button("Follow Camera")) { currentCameraMode_ = CameraMode::FOLLOW; }
	if (ImGui::Button("Debug Camera")) { currentCameraMode_ = CameraMode::DEBUG; }
#endif
}

/*==========================================================================
	エンカウント発生時の処理
//========================================================================*/
void FieldScene::HandleDetailedEncounter(const EncountInfo& encounterInfo) {
	Logger("[FieldScene] ===== 詳細エンカウント処理 START =====\n");

	//------------------------------------------------------------
	// バトル遷移データ作成
	//------------------------------------------------------------
	BattleTransitionData transitionData;
	transitionData.enemyGroup = encounterInfo.enemyGroup;
	transitionData.battleEnemyId = encounterInfo.battleEnemyId;
	transitionData.battleEnemyIds = encounterInfo.battleEnemyIds;
	transitionData.playerPosition = GetPlayerPosition();
	transitionData.playerHitDamage = encounterInfo.encounteredEnemy->GetTakeDamage();
	SaveCameraState(transitionData);

	// これが最後の敵かどうかをチェック
	bool isFinalBattle = false;
	size_t remainingGroups = 0;

	if (fieldEnemyManager_) {

		// エンカウント「グループ数」で判定（敵の個体数ではなく）
		remainingGroups = fieldEnemyManager_->GetActiveEncounterGroupCount();

		char debugBuffer[256];
		sprintf_s(debugBuffer, "[FieldScene] 残りのエンカウントグループ数: %zu\n", remainingGroups);
		Logger(debugBuffer);

		// 残りのグループが 1（=今戦うグループのみ）なら最終戦
		if (remainingGroups <= 1) {
			isFinalBattle = true;
			Logger("[FieldScene] ★★★ 最終エンカウントグループです！ ★★★\n");
		}
		else {
			sprintf_s(debugBuffer, "[FieldScene] まだ最終戦ではありません。残りグループ数: %zu\n", remainingGroups - 1);
			Logger(debugBuffer);
		}
	}
	else {
		Logger("[FieldScene] エラー: fieldEnemyManager_ が null です！\n");
	}

	transitionData.isFinalBattle = isFinalBattle;
	transitionData.totalRemainingFieldEnemies = remainingGroups;

	auto* syncData = SceneSyncData::GetInstance();
	syncData->SaveBattleTransitionData(transitionData);

#ifdef _DEBUG
	char buffer[512];
	sprintf_s(buffer,
		"[FieldScene] バトル遷移データを保存しました - EnemyGroup: %s, BattleEnemyId: %s, 最終戦: %s\n",
		encounterInfo.enemyGroup.c_str(),
		encounterInfo.battleEnemyId.c_str(),
		isFinalBattle ? "はい" : "いいえ"
	);
	Logger(buffer);
#endif // _DEBUG

	// SubSceneManager へバトル遷移リクエスト
	RequestBattleTransition(transitionData);

#ifdef _DEBUG
	Logger("[FieldScene] ===== 詳細エンカウント処理 END =====\n");
	// 残りの敵グループ数をログ出力
	{
		size_t groups = fieldEnemyManager_->GetActiveEncounterGroupCount();
		char debugBuffer[256];
		sprintf_s(debugBuffer,
			"[FieldScene] 現在の残りエンカウントグループ数: %zu\n",
			groups);
		Logger(debugBuffer);
	}
#endif // _DEBUG
}


/*==========================================================================
	カメラ状態の保存（戦闘復帰用）
//========================================================================*/
void FieldScene::SaveCameraState(BattleTransitionData& data) {
	data.cameraPosition = sceneCamera_->transform_.translate;
	data.cameraMode = currentCameraMode_;
}

/*==========================================================================
	バトル復帰時の処理（勝敗によって変わる）
//========================================================================*/
void FieldScene::HandleBattleReturn(const FieldReturnData& data) {
	Logger("[FieldScene] ===== HandleBattleReturn() START =====\n");

	// 念のためここでもエンカウントリセット関数を呼ぶ
	fieldEnemyManager_->ResetEnCount();
	Vector3 returnPos = data.playerPosition;

	if (data.playerWon) {
		fieldEnemyManager_->RegisterDefeatedEnemy(data.defeatedEnemyGroup);
		char buffer[256];
		sprintf_s(buffer, "[FieldScene] Victory! Defeated enemy: %s\n", data.defeatedEnemyGroup.c_str());
		Logger(buffer);
	}
	else {
		returnPos += Vector3(0, 0, -2.0f);
		Logger("[FieldScene] Defeat! Player moved back\n");
	}

	player_->SetPosition(returnPos);

	if (data.expGained > 0 || data.goldGained > 0) {
		char buffer[256];
		sprintf_s(buffer, "[FieldScene] Battle rewards - EXP: %d, Gold: %d\n",
			data.expGained, data.goldGained);
		Logger(buffer);
	}

	fieldEnemyManager_->HandleBattleEnd(data.defeatedEnemyGroup, data.playerWon);

	Logger("[FieldScene] ===== HandleBattleReturn() END =====\n");
}

/*==========================================================================
	カメラ状態の復元
//========================================================================*/
void FieldScene::RestoreCameraState(const FieldReturnData& data) {
	currentCameraMode_ = data.cameraMode;
}

/*==========================================================================
	プレイヤーの位置取得
//========================================================================*/
Vector3 FieldScene::GetPlayerPosition() const {
	return player_ ? player_->GetWorldPosition() : Vector3(0.0f, 0.0f, 0.0f);
}

/*==========================================================================
	全ての敵が激はされたかチェック
//========================================================================*/
bool FieldScene::AreAllEnemiesDefeated() const {
	if (!fieldEnemyManager_) {
		return false;
	}

	// アクティブな敵を取得
	auto activeEnemies = fieldEnemyManager_->GetActiveFieldEnemies();

	// アクティブな敵がいる場合、まだクリアではない
	if (!activeEnemies.empty()) {
		return false;
	}

	// アクティブな敵がいない場合、
	// 撃破済み敵が1体以上いればクリア（敵が存在していた証拠）
	// 撃破済み敵がいなければ、まだ敵との戦闘が発生していない
	size_t totalEnemyCount = fieldEnemyManager_->GetActiveEnemyCount();

	// 初期配置された敵が全て撃破されたかチェック
	// （この判定はFieldEnemyManagerの初期スポーン数などで判定する必要がある）
	return totalEnemyCount == 0 && fieldEnemyManager_->HasAnyEnemiesBeenSpawned();
}

/*==========================================================================
	終了処理
//========================================================================*/
void FieldScene::Finalize() {
	if (fieldEnemyManager_) {
		fieldEnemyManager_->Finalize();
	}
}