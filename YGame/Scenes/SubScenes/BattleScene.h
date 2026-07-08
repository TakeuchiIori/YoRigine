#pragma once

// C++
#include <memory>
#include <functional>
#include <string>

// Engine
#include "Systems/Camera/Camera.h"
#include "Object3D/Object3d.h"
#include "Player/Player.h"
#include "WorldTransform./WorldTransform.h"
#include "Drawer/LineManager/Line.h"
#include "Collision/AreaCollision/Base/AreaManager.h"
#include "Collision/AreaCollision/CircleArea.h"

// App
#include "Enemy/BattleEnemy/BattleEnemyManager.h"
#include "SceneDataStructures.h"
#include "BaseSubScene.h"
#include "Ground/Ground.h"
#include <UI/LockOn/LockOnUI.h>

///************************* バトルシーン *************************///
class BattleScene : public BaseSubScene {
public:
	///************************* 基本関数 *************************///

	// コンストラクタ
	BattleScene() : BaseSubScene("Battle") {}

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

	// VFXの描画処理
	void DrawVFX() override;

	// 非オフスクリーン描画処理
	void DrawNonOffscreen() override;

	// シャドウ描画処理
	void DrawShadow() override;

	// 終了処理
	void Finalize() override;

	///************************* ライフサイクル *************************///

	// シーンに入ったときの処理
	void OnEnter() override;

	// シーンから出るときの処理
	void OnExit() override;

	///************************* シーン固有処理 *************************///

	// 戦闘を開始（SubSceneManager からフェード演出途中で型付き呼び出し）
	void StartBattle(const BattleTransitionData& data);

	// 戦闘を強制終了
	void ForceBattleEnd();

	// バトルカメラ終了フラグを設定
	void SetBattleCameraFinished(bool finished);

	// ゲームクリアフラグを設定
	void SetGameClearFlag(bool isFinalBattle) { isFinalBattle_ = isFinalBattle; }

	bool IsAllEnemysDefeated() { return battleEnemyManager_->AreAllEnemiesDefeated(); };

	// BattleEnemyManagerへのアクセサ
	BattleEnemyManager* GetBattleEnemyManager() const { return battleEnemyManager_.get(); }

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

	// 戦闘終了時の処理
	void HandleBattleEnd(BattleResult result, const BattleStats& stats);

	// プレイヤー状態を保存
	void SavePlayerState(const BattleTransitionData& data);

	// 戦闘終了後の戻りデータを作成
	void CreateBattleReturnData(FieldReturnData& data, BattleResult result, const BattleStats& stats);

	void FocusNearestEnemyAfterDefeat();

private:
	///************************* メンバ変数 *************************///

	std::unique_ptr<BattleEnemyManager> battleEnemyManager_;
	std::unique_ptr<Line> line_;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Ground> ground_;
	std::unique_ptr<BaseObject> enemy_;
	std::unique_ptr<LockOnUI> lockOnUI_;

	std::string currentEnemyGroup_;
	BattleTransitionData originalTransitionData_;
	bool battleCameraFinished_ = false;
	bool shouldResetBattleCamera_ = false;
	bool isFinalBattle_ = false;  // 最終バトルかどうか
	size_t totalRemainingFieldEnemies_ = 0;  // ★追加: フィールドに残っているエンカウントグループ数

	// 最終バトルクリア演出が起動済みかどうか（毎フレーム再発火するのを防ぐ）
	bool clearCinematicStarted_ = false;
};
