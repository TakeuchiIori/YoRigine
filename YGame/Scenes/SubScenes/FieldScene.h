#pragma once

// C++
#include <memory>
#include <functional>
#include <vector>

// Engine
#include "Systems/Camera/Camera.h"
#include "Object3D/Object3d.h"
#include "Player/Player.h"
#include "WorldTransform./WorldTransform.h"
#include "Drawer/LineManager/Line.h"
#include "Ground/Ground.h"
#include <GPUParticle/GPUEmitter.h>
#include "Collision/AreaCollision/CircleArea.h"

// App
#include "Enemy/FieldEnemy/FieldEnemyManager.h"
#include "Trigger/EventTrigger.h"
#include "Trigger/EventTriggerLoader.h"
#include "Trigger/Actions/OpenGateAction.h"

// Navigation
#include <Systems/Navigation/NavGrid.h>
#include <Systems/Navigation/NavPathfinder.h>
#include <Systems/Navigation/NavGridConfig.h>
#include "SceneDataStructures.h"
#include "BaseSubScene.h"

// JSON
#include "Loaders/Json/JsonManager.h"

///************************* フィールドシーン *************************///
class FieldScene : public BaseSubScene {
public:
	///************************* 基本関数 *************************///

	// コンストラクタ
	FieldScene() : BaseSubScene("Field") {}

	// 初期化処理
	void Initialize(Camera* camera, Player* player) override;

	// 更新処理
	void Update() override;

	// オブジェクト描画処理
	void DrawObject() override;

	// ライン描画処理
	void DrawLine() override;

	// UI描画処理
	void DrawUI() override;

	// 非オフスクリーン描画処理
	void DrawNonOffscreen() override;

	// シャドウ描画処理
	void DrawShadow() override;

	// 終了処理
	void Finalize() override;

	void RebakeNavGrid();

	///************************* ライフサイクル *************************///

	// シーンに入ったときの処理
	void OnEnter() override;

	// シーンから出るときの処理
	void OnExit() override;

	///************************* シーン固有処理 *************************///

	// バトルシーンから戻った際の処理
	void HandleBattleReturn(const FieldReturnData& data);

#ifdef USE_IMGUI
	// EventTrigger エディタ (Editor::RegisterGameUI から呼ばれる)
	void DrawEventTriggerEditor();
#endif

	///************************* アクセッサ *************************///

	// プレイヤーの現在位置を取得
	Vector3 GetPlayerPosition() const;

	// プレイヤーを取得
	Player* GetPlayer() const { return player_; }

	// すべての敵が撃破されたかチェック
	bool AreAllEnemiesDefeated() const;

private:
	///************************* 内部処理 *************************///

	// カメラモードを更新
	void UpdateCameraMode();

	// 詳細なエンカウント処理を実行
	void HandleDetailedEncounter(const EncountInfo& encounterInfo);

	// カメラ状態を保存
	void SaveCameraState(BattleTransitionData& data);

	// カメラ状態を復元
	void RestoreCameraState(const FieldReturnData& data);

private:
	///************************* メンバ変数 *************************///

	std::unique_ptr<Ground> ground_;
	std::unique_ptr<FieldEnemyManager> fieldEnemyManager_;
	std::unique_ptr<GPUEmitter> gpuEmitter_;
	std::unique_ptr<Line> line_;
	std::unique_ptr<Sprite> sprite_;

	// イベントトリガー (討伐数→扉開放など)。Action は EventTrigger 所有。
	std::vector<std::unique_ptr<EventTrigger>> eventTriggers_;
	// FieldEnemyManager の撃破コールバックから dispatch するための弱参照リスト。
	std::vector<OpenGateAction*> openGateActions_;

	// Navigation
	NavGridConfig navGridConfig_;  // データドリブン設定（JSON管理）
	NavGrid navGrid_;
	NavPathfinder navPathfinder_;
	bool showNavGridDebug_ = false; // NavGridデバッグ描画ON/OFF

	// プレイヤー初期スポーン (フィールドに最初に降り立つ位置 / 向き)。JSON 保存対象。
	// バトル復帰時の位置 (HandleBattleReturn) とは別物。
	Vector3 spawnPos_ = { 0.0f, 0.0f, 0.0f };
	float   spawnYawDeg_ = 0.0f;
	// フォローカメラの初期 Euler (deg)。x=pitch / y=yaw / z=roll。
	Vector3 spawnCameraRotDeg_ = { 0.0f, 0.0f, 0.0f };
	std::unique_ptr<YoRigine::JsonManager> spawnJson_;
};