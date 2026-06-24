#include "FieldScene.h"

// Engine
#include "Systems./Input./Input.h"
#include "Object3D/Object3dCommon.h"
#include "LightManager/LightManager.h"
#include "Collision/Core/CollisionManager.h"
#include "Systems/GameTime/GameTime.h"
#include "Systems/Camera/CameraDirector.h"
#include <Editor/Editor.h>
#include "Debugger/Logger.h"
#include <Object3D/ObjectManager.h>
#include "Collision/AreaCollision/Base/AreaManager.h"
#include <ModelManipulator/ModelManipulator.h>
#include "Collision/AreaCollision/Base/AreaEditor.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#endif

/*==========================================================================
	初期化
//========================================================================*/
void FieldScene::Initialize(Camera* camera, Player* player) {

	sceneCamera_ = camera;
	player_ = player;
	player_->Reset();

	//------------------------------------------------------------
	// プレイヤースポーン設定 (JSON ロード → プレイヤー / カメラに適用)
	//------------------------------------------------------------
	spawnJson_ = std::make_unique<YoRigine::JsonManager>("FieldSpawn", "Resources/Json/Scenes");
	spawnJson_->Register("位置",            &spawnPos_);
	spawnJson_->Register("向き (deg)",      &spawnYawDeg_);
	spawnJson_->Register("カメラ向き (deg)", &spawnCameraRotDeg_);

	// スポーン姿勢を適用。リトライ(HandleRetry→Initialize)でも初回と同じ位置から
	// 始められるよう、ここで適用しつつ、フェード遷移後の OnEnter でも再適用する
	// （遷移途中で位置が書き換わるケースを打ち消すため）。
	ApplySpawnPose();
	pendingSpawnReset_ = true;

	//------------------------------------------------------------
	// オブジェクト初期化
	//------------------------------------------------------------
	ground_ = std::make_unique<Ground>();
	ground_->Initialize(sceneCamera_);


	line_ = std::make_unique<Line>();
	line_->Initialize();
	line_->SetCamera(sceneCamera_);

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
	int blockedCount = 0;
	for (const auto& cell : navGrid_.GetCells()) {
		if (!cell.walkable) ++blockedCount;
	}
	char buf[128];
	sprintf_s(buf, "[NavGrid] 通行不可セル: %d / %d\n",
		blockedCount,
		navGrid_.GetWidthCells() * navGrid_.GetDepthCells());
	Logger(buf);
	//------------------------------------------------------------
	// フィールド敵管理システム初期化
	//------------------------------------------------------------
	fieldEnemyManager_ = std::make_unique<FieldEnemyManager>();
	// FieldEnemyManager に NavPathfinder を渡す
	fieldEnemyManager_->SetNavPathfinder(&navPathfinder_);
	fieldEnemyManager_->Initialize(sceneCamera_);
	fieldEnemyManager_->SetPlayer(player_);



	// 敵エンカウント時の詳細コールバック登録
	fieldEnemyManager_->SetEncounterDetailCallback([this](const EncountInfo& encounterInfo) {
		HandleDetailedEncounter(encounterInfo);
		});

	// FieldEnemyManager の撃破コールバックを全 OpenGateAction にディスパッチ
	fieldEnemyManager_->SetOnEnemyDefeatedCallback([this](const std::string& group) {
		for (auto* act : openGateActions_) {
			if (act) act->NotifyEnemyDefeated(group);
		}
	});

	// EventTrigger のロードはシーン入場のたびに必要なため、OnEnter 側で実行する
	// (ModelManipulator::LoadScene が ObjectManager をクリアして PlacedObject を作り直すので、
	//  EventTrigger も同タイミングで作り直さないとターゲット参照がずれる)。

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize("Resources/Textures/GameScene/FieldScene.png");



	// エリア設定
	// 登録名は "FieldArea" で統一。OnExit の RemoveArea("FieldArea") と一致させ、
	// バトル遷移時にフィールド側のエリアが残らないようにする
	// (以前は "TestArea" として登録されていたため OnExit で消えず、
	//  BattleScene の "BattleArea" と二重に描画されて "100 のまま" に見えていた)。
	auto battleFieldArea = std::make_shared<CircleArea>();
	battleFieldArea->Initialize(Vector3(0, 0, 0), 100.0f);
	battleFieldArea->SetPurpose(AreaPurpose::Boundary);  // 明示
	battleFieldArea->SetCamera(sceneCamera_);

	auto* mgr = AreaManager::GetInstance();
	mgr->AddArea("FieldArea", battleFieldArea);
	mgr->SetDebugDrawEnabled(true);

#ifdef USE_IMGUI
	Editor::GetInstance()->RegisterGameUI("フィールドモード:デバッグ情報",
		[this]() { fieldEnemyManager_->ShowDebugInfo(); }, "Game");

	// AreaEditor は Game シーン全体(Field/Battle 両サブシーン)で開けるよう一度だけ登録。
	// AreaEditor / AreaManager はシングルトンなのでサブシーンを跨いでも同じ状態を共有する。
	Editor::GetInstance()->RegisterGameUI("AreaEditor",
		[]() { AreaEditor::GetInstance()->Update(); }, "Game");

	// プレイヤースポーン地点エディタ
	Editor::GetInstance()->RegisterGameUI("プレイヤースポーン", [this]() {
		constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
		constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;

		ImGui::TextDisabled("ゲーム開始時のプレイヤー / カメラの初期状態");
		ImGui::Separator();

		ImGui::TextUnformatted("プレイヤー");
		ImGui::DragFloat3("位置##spawn", &spawnPos_.x, 0.1f);
		ImGui::DragFloat("向き (deg)##spawn", &spawnYawDeg_, 1.0f, -180.0f, 180.0f, "%.1f");

		ImGui::Separator();
		ImGui::TextUnformatted("フォローカメラ");
		ImGui::DragFloat3("カメラ向き (deg)##spawn", &spawnCameraRotDeg_.x, 1.0f, -180.0f, 180.0f, "%.1f");
		ImGui::TextDisabled("x=Pitch / y=Yaw / z=Roll");

		ImGui::Separator();
		if (ImGui::Button("現在の状態を取得")) {
			spawnPos_ = player_->GetWT().translate_;
			spawnYawDeg_ = player_->GetWT().rotate_.y * kRadToDeg;
			if (auto* pc = player_->GetPlayerCamera()) {
				const Vector3 r = pc->GetRotate();
				spawnCameraRotDeg_ = { r.x * kRadToDeg, r.y * kRadToDeg, r.z * kRadToDeg };
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("ここにワープ")) {
			player_->SetPosition(spawnPos_);
			player_->GetWT().rotate_.y = spawnYawDeg_ * kDegToRad;
			player_->GetWT().UpdateMatrix();
			if (auto* pc = player_->GetPlayerCamera()) {
				pc->SetRotate({
					spawnCameraRotDeg_.x * kDegToRad,
					spawnCameraRotDeg_.y * kDegToRad,
					spawnCameraRotDeg_.z * kDegToRad });
			}
		}

		ImGui::Separator();
		if (ImGui::Button("保存")) {
			spawnJson_->Save();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("(編集値はワープ or 起動時に反映)");
	}, "Game");

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

	Editor::GetInstance()->RegisterGameUI("EventTrigger", [this]() {
		DrawEventTriggerEditor();
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
	for (auto& trig : eventTriggers_) {
		if (trig) trig->Update();
	}
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
	// フレーム冒頭の頂点・マテリアル CB スロットのリセット (複数の DrawLine() 呼出の干渉を避ける)
	line_->Reset();

	player_->DrawCollision();
	fieldEnemyManager_->DrawCollision();
	for (auto& trig : eventTriggers_) {
		if (trig) trig->DrawCollision();
	}
	fieldEnemyManager_->DrawLine(line_.get());
	fieldEnemyManager_->DrawEditorMarkers(line_.get());
	player_->DrawBone(*line_.get());
	AreaManager::GetInstance()->DrawArea("FieldArea", line_.get());
	AreaManager::GetInstance()->Draw(line_.get(), { "FieldArea" });
	// LineManager にキューイングされた頂点をここで GPU 提出。
	// 後段の NavGrid デバッグ分岐は自前で DrawLine しているが、
	// それが無効なケースだとエリア線が出ないので明示的にフラッシュする。
	line_->DrawLine();

	// ── NavGrid デバッグ描画 ─────────────────────────────────────────────
	// showNavGridDebug_ が true のときだけグリッドを可視化する。
	// 通行不可セル（障害物）は赤の薄いAABB、通行可能は緑の薄い点で描く。
	if (showNavGridDebug_ && navGrid_.IsInitialized()) {
		const int W = navGrid_.GetWidthCells();
		const int D = navGrid_.GetDepthCells();
		const float cs = navGrid_.GetCellSize();
		const float yBase = 0.0f; // 地面のY座標

		// 配置オブジェクトの footprint と erosion を色分けして描画。
		// 赤 = オブジェクト本体に重なる "raw" セル (配置物の位置・サイズと一致)
		// 橙 = それ以外の通行不可 = erosion (エージェント半径ぶんの安全マージン)
		// 厚さ 0.6m / Y=0.1 から立ち上げて OBB ワイヤフレームと重ならないようにする
		const auto& cells = navGrid_.GetCells();
		const float yLo = yBase + 0.1f;
		const float yHi = yBase + 0.7f;
		for (int gz = 0; gz < D; ++gz) {
			for (int gx = 0; gx < W; ++gx) {
				const auto& c = cells[gz * W + gx];
				if (c.walkable) continue;

				NavGrid::GridPos gp{ gx, gz };
				Vector3 center = navGrid_.GridToWorld(gp);
				Vector3 mn = { center.x - cs * 0.5f, yLo, center.z - cs * 0.5f };
				Vector3 mx = { center.x + cs * 0.5f, yHi, center.z + cs * 0.5f };

				if (c.rawObstacle) {
					line_->SetColor({ 1.0f, 0.15f, 0.15f, 0.95f }); // 赤: obstacle footprint
				} else {
					line_->SetColor({ 1.0f, 0.55f, 0.10f, 0.45f }); // 橙: erosion margin
				}
				line_->DrawAABB(mn, mx);
				line_->DrawLine();
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
/*==========================================================================
	スポーン姿勢をプレイヤー・カメラへ適用
//========================================================================*/
void FieldScene::ApplySpawnPose() {
	if (!player_) return;

	constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

	player_->SetPosition(spawnPos_);
	player_->GetWT().rotate_.y = spawnYawDeg_ * kDegToRad;
	player_->GetWT().UpdateMatrix();

	// PlayerCamera は GameScene::Initialize で先にセットアップ済み(なければ無視)。
	if (auto* pc = player_->GetPlayerCamera()) {
		pc->SetRotate({
			spawnCameraRotDeg_.x * kDegToRad,
			spawnCameraRotDeg_.y * kDegToRad,
			spawnCameraRotDeg_.z * kDegToRad });
	}
}

void FieldScene::OnEnter() {
	BaseSubScene::OnEnter();

	// フィールドでは脅威察知を無効化（背景・マップを自由に見渡せるようにする）
	if (player_ && player_->GetPlayerCamera()) {
		player_->GetPlayerCamera()->SetThreatAwarenessAllowed(false);
	}

	Logger("[FieldScene] ===== OnEnter() START =====\n");

	// フィールド用 ModelManipulator シーンへ切替。
	// Field.json が無い場合は空シーンになるので、初回は GameScene.json から
	// 必要なものだけ残して保存して Field.json を作る運用にする。
	// (LoadScene 内で ObjectManager をクリアするため NavGrid も貼り直す)
	YoRigine::ModelManipulator::GetInstance()->LoadScene("Field");

	// EventTrigger は初回入場時のみロードする。
	// バトル復帰時の再入場でも作り直すと OpenGateAction の currentCount_ が 0 に戻り、
	// 倒すたびにカウントがリセットされてトリガーが永遠に発火しない。
	// PlacedObject が LoadScene で作り直されても、cachedTargetId_ は BeginOpening 時に
	// targetName_ から取り直すので問題ない。
	if (eventTriggers_.empty()) {
		EventTriggerLoader::Load(
			EventTriggerPaths::Field,
			sceneCamera_,
			[this]() { RebakeNavGrid(); },
			eventTriggers_,
			openGateActions_);
	}

	RebakeNavGrid();

	// バトル復帰時に BattleArea(半径50) が残って FieldArea(半径100) が消えるバグを修正。
	// BattleScene::OnExit が BattleArea を削除していないので、ここで掃除する。
	// FieldArea も OnExit で消されているので、入場のたびに登録し直す。
	{
		auto* mgr = AreaManager::GetInstance();
		mgr->RemoveArea("BattleArea");
		mgr->RemoveArea("FieldArea");
		auto fieldArea = std::make_shared<CircleArea>();
		fieldArea->Initialize(Vector3(0, 0, 0), 100.0f);
		fieldArea->SetPurpose(AreaPurpose::Boundary);
		fieldArea->SetCamera(sceneCamera_);
		mgr->AddArea("FieldArea", fieldArea);
	}

	// フィールドの敵を再開（OBB コライダーもまとめて有効化される）
	if (fieldEnemyManager_) {
		fieldEnemyManager_->SetAllEnemiesActive(true);
		fieldEnemyManager_->ResetEnCount();
		currentCameraMode_ = CameraMode::FOLLOW;
	}

	// リトライ/初回スポーン：フェード遷移の途中でプレイヤー位置が書き換わっても、
	// シーン有効化時に確実にスポーン姿勢を再適用して「初期化時と同じ位置」を保証する。
	// バトル復帰時は pendingSpawnReset_ が立たないので干渉せず、この後の
	// ApplyTransitionData→HandleBattleReturn が復帰位置を上書きする。
	if (pendingSpawnReset_) {
		ApplySpawnPose();
		pendingSpawnReset_ = false;
	}

	// バトルからの復帰データは SubSceneManager::ApplyTransitionData →
	// HandleBattleReturn(data) で型付きに渡されるので、ここでは読まない。
	Logger("[FieldScene] ===== OnEnter() END =====\n");
}

/*==========================================================================
	シーンを抜けるときの処理
//========================================================================*/
void FieldScene::OnExit() {
	BaseSubScene::OnExit();
	AreaManager::GetInstance()->RemoveArea("FieldArea");

	Logger("[FieldScene] ===== OnExit() START =====\n");

	// 敵の更新とコライダーをまとめて停止。これで BattleScene 中に
	// FieldEnemy の OBB がグローバル CollisionManager で当たり判定されなくなる。
	if (fieldEnemyManager_) {
		fieldEnemyManager_->SetAllEnemiesActive(false);
	}

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
	transitionData.battleEnemyScale = encounterInfo.encounterScale;
	transitionData.playerPosition = GetPlayerPosition();
	transitionData.playerHitDamage = encounterInfo.encounteredEnemy->GetTakeDamage();
	SaveCameraState(transitionData);

	// エンカウント位置を自前で保存しておく。BattleScene を経由する FieldReturnData
	// と二重化することで、復帰経路のどこかでデータが落ちても戻れるようにする。
	savedEncounterPos_    = transitionData.playerPosition;
	hasSavedEncounterPos_ = true;
	{
		char dbg[160];
		sprintf_s(dbg, "[FieldScene] エンカウント位置を保存: (%.2f, %.2f, %.2f)\n",
			savedEncounterPos_.x, savedEncounterPos_.y, savedEncounterPos_.z);
		Logger(dbg);
	}

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

	// エンカウント位置の復元は自前 (savedEncounterPos_) を優先する。
	// 経由データ (data.playerPosition) はパイプライン上で上書きされたり消えたりする
	// 余地があるため、信頼できる側を採用する。両方無ければ data.playerPosition fallback。
	Vector3 returnPos = hasSavedEncounterPos_ ? savedEncounterPos_ : data.playerPosition;

	{
		char dbg[256];
		sprintf_s(dbg,
			"[FieldScene] 復帰位置: (%.2f, %.2f, %.2f)  [saved=%s, win=%s]\n",
			returnPos.x, returnPos.y, returnPos.z,
			hasSavedEncounterPos_ ? "yes" : "no",
			data.playerWon ? "yes" : "no");
		Logger(dbg);
	}

	if (data.playerWon) {
		char buffer[256];
		sprintf_s(buffer, "[FieldScene] Victory! Defeated enemy: %s\n", data.defeatedEnemyGroup.c_str());
		Logger(buffer);
	}
	else {
		Logger("[FieldScene] Defeat!\n");
	}

	// 復帰位置が原点近傍かつ savedEncounterPos_ も無い場合、データパイプラインが
	// 壊れている可能性が高い。プレイヤーが意図せず (0,0,0) にワープしないよう、
	// SetPosition をスキップして現在位置を維持する (= バトル直前のフィールド位置)。
	constexpr float kOriginEps = 1e-3f;
	const bool isOriginish =
		returnPos.x > -kOriginEps && returnPos.x < kOriginEps &&
		returnPos.z > -kOriginEps && returnPos.z < kOriginEps;
	if (isOriginish && !hasSavedEncounterPos_) {
		Logger("[FieldScene] WARN: 復帰位置が原点 & savedEncounterPos_ 無効 - SetPosition skip\n");
	} else {
		player_->SetPosition(returnPos);
	}

	// 使い終わったらクリア (次のエンカウントで上書きされる前の保険)
	hasSavedEncounterPos_ = false;

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
	openGateActions_.clear();
	eventTriggers_.clear();
}

void FieldScene::RebakeNavGrid()
{
	navGrid_.Bake(ObjectManager::GetInstance());
	// 全敵の経路をクリア（次フレームで再計算される）
	for (auto* enemy : fieldEnemyManager_->GetActiveFieldEnemies()) {
		enemy->ClearNavPath();
	}
}

#ifdef USE_IMGUI
/*==========================================================================
	EventTrigger エディタ
//========================================================================*/
void FieldScene::DrawEventTriggerEditor() {
	ImGui::TextDisabled("Resources/Json/EventTriggers/field_triggers.json");
	ImGui::Separator();

	// ── ツールバー ───────────────────────────────────────────
	if (ImGui::Button("＋ OpenGate トリガー追加")) {
		auto trigger = std::make_unique<EventTrigger>();
		trigger->Initialize(sceneCamera_);
		trigger->SetName("NewTrigger");

		auto action = std::make_unique<OpenGateAction>(
			std::string(""), std::string(""), 1);
		action->SetOnGateOpened([this]() { RebakeNavGrid(); });

		openGateActions_.push_back(action.get());
		trigger->SetAction(std::move(action));
		eventTriggers_.push_back(std::move(trigger));
	}
	ImGui::SameLine();
	if (ImGui::Button("保存")) {
		EventTriggerLoader::Save(EventTriggerPaths::Field, eventTriggers_);
	}
	ImGui::SameLine();
	if (ImGui::Button("再ロード")) {
		eventTriggers_.clear();
		openGateActions_.clear();
		EventTriggerLoader::Load(
			EventTriggerPaths::Field,
			sceneCamera_,
			[this]() { RebakeNavGrid(); },
			eventTriggers_,
			openGateActions_);
	}

	ImGui::Separator();
	ImGui::Text("登録数: %d", static_cast<int>(eventTriggers_.size()));
	ImGui::Separator();

	// ── トリガー一覧 ──────────────────────────────────────────
	int deleteIndex = -1;
	for (size_t i = 0; i < eventTriggers_.size(); ++i) {
		auto& trig = eventTriggers_[i];
		if (!trig) continue;

		ImGui::PushID(static_cast<int>(i));

		const std::string header = "[" + std::to_string(i) + "] " +
			(trig->GetName().empty() ? "(無名)" : trig->GetName());

		if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
			// 名前
			{
				char buf[128];
				std::snprintf(buf, sizeof(buf), "%s", trig->GetName().c_str());
				if (ImGui::InputText("名前", buf, sizeof(buf))) {
					trig->SetName(buf);
				}
			}

			// トランスフォーム (アクションが空間情報を使う時だけ表示)
			TriggerAction* curAction = trig->GetAction();
			const bool needsSpatial = curAction ? curAction->NeedsSpatialPlacement() : true;
			if (needsSpatial) {
				auto& wt = trig->GetWT();
				bool xformChanged = false;
				if (ImGui::DragFloat3("位置",     &wt.translate_.x, 0.1f)) xformChanged = true;
				if (ImGui::DragFloat3("回転(rad)", &wt.rotate_.x,    0.05f)) xformChanged = true;
				if (ImGui::DragFloat3("スケール",  &wt.scale_.x,     0.1f, 0.1f, 100.0f)) xformChanged = true;
				if (xformChanged) wt.UpdateMatrix();
			} else {
				ImGui::TextDisabled("(このアクションは位置情報を使いません)");
			}

			// Action 編集 (OpenGateAction のみ対応)
			if (auto* gate = dynamic_cast<OpenGateAction*>(trig->GetAction())) {
				ImGui::Separator();
				ImGui::TextDisabled("Action: OpenGate");

				{
					char buf[128];
					std::snprintf(buf, sizeof(buf), "%s", gate->GetTargetName().c_str());
					if (ImGui::InputText("ターゲット nameTag", buf, sizeof(buf))) {
						gate->SetTargetName(buf);
					}
				}
				{
					char buf[128];
					std::snprintf(buf, sizeof(buf), "%s", gate->GetRequiredGroup().c_str());
					if (ImGui::InputText("必要敵グループ (enemyId)", buf, sizeof(buf))) {
						gate->SetRequiredGroup(buf);
					}
					if (gate->GetRequiredGroup().empty()) {
						ImGui::SameLine();
						ImGui::TextDisabled("(空欄 = どの敵でもカウント)");
					}
				}
				{
					int n = gate->GetRequiredCount();
					if (ImGui::DragInt("必要撃破数", &n, 1.0f, 1, 999)) {
						gate->SetRequiredCount(n);
					}
				}

				// ── 開放モーション (コード補間) ────────────────────────
				// 元の transform に対する差分を duration 秒で線形補間。
				ImGui::Separator();
				ImGui::TextDisabled("開放モーション (元 transform に対する差分)");
				{
					Vector3 v = gate->GetOpenOffsetPosition();
					if (ImGui::DragFloat3("オフセット位置", &v.x, 0.1f)) gate->SetOpenOffsetPosition(v);
				}
				{
					Vector3 v = gate->GetOpenOffsetRotationDeg();
					if (ImGui::DragFloat3("オフセット回転(deg)", &v.x, 1.0f)) gate->SetOpenOffsetRotationDeg(v);
				}
				{
					Vector3 v = gate->GetOpenOffsetScale();
					if (ImGui::DragFloat3("オフセットスケール", &v.x, 0.05f)) gate->SetOpenOffsetScale(v);
				}
				{
					float d = gate->GetOpenDuration();
					if (ImGui::DragFloat("補間時間 (秒)", &d, 0.05f, 0.05f, 60.0f)) gate->SetOpenDuration(d);
				}

				ImGui::Separator();
				ImGui::TextDisabled("ヒンジ式の扉なら、ターゲット PlacedObject 側でアンカーを設定してください (シーンエディタ → アンカー使用)");

				// ── 閉位置の管理 ───────────────────────────────────
				ImGui::Separator();
				ImGui::TextDisabled("閉 (初期) 位置: %s",
					gate->HasClosedPose() ? "記録済み" : "未記録 (初回起動時に target 現在位置を捕捉)");
				if (gate->HasClosedPose()) {
					ImGui::TextDisabled("  pos (%.2f, %.2f, %.2f)",
						gate->GetClosedPosition().x, gate->GetClosedPosition().y, gate->GetClosedPosition().z);
				}
				if (ImGui::Button("現在の target 位置を「閉」として記録")) {
					if (auto* om = ObjectManager::GetInstance()) {
						if (auto* tgt = om->GetObjectByName(gate->GetTargetName())) {
							gate->SetClosedPose(tgt->position, tgt->rotation, tgt->scale);
						}
					}
				}

				ImGui::Separator();
				ImGui::Text("進捗: %d / %d (Phase: %d)",
					gate->GetCurrentCount(), gate->GetRequiredCount(),
					static_cast<int>(gate->GetPhase()));

				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
				if (ImGui::Button(" プレビュー (即発火)")) {
					gate->TriggerPreview();
				}
				ImGui::PopStyleColor();
				ImGui::SameLine();
				if (ImGui::Button("状態リセット")) {
					gate->Reset();
				}
			}
			else if (trig->GetAction()) {
				ImGui::TextDisabled("Action: %s (未対応のエディタ)",
					trig->GetAction()->GetTypeName().c_str());
			}

			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
			if (ImGui::Button("このトリガーを削除")) {
				deleteIndex = static_cast<int>(i);
			}
			ImGui::PopStyleColor();
		}

		ImGui::PopID();
	}

	// 削除を遅延適用 (ループ終了後)
	if (deleteIndex >= 0) {
		// openGateActions_ から該当ポインタを除外
		auto* doomedAction = eventTriggers_[deleteIndex]->GetAction();
		openGateActions_.erase(
			std::remove(openGateActions_.begin(), openGateActions_.end(), doomedAction),
			openGateActions_.end());
		eventTriggers_.erase(eventTriggers_.begin() + deleteIndex);
	}
}
#endif