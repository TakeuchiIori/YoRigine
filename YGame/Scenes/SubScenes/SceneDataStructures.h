#pragma once
#include "Vector3.h"
#include <vector>
#include <string>
#include "Systems/Camera/CameraMode.h"

///************************* バトル遷移データ *************************///
struct BattleTransitionData {
	std::string enemyGroup;
	std::string battleEnemyId;  // 単体バトル用
	std::vector<std::string> battleEnemyIds;  // 複数体バトル用
	std::string battleFormation;
	Vector3 playerPosition;
	Vector3 cameraPosition;
	CameraMode cameraMode = CameraMode::FOLLOW;
	bool isFinalBattle = false;
	size_t totalRemainingFieldEnemies = 0;
	float playerHitDamage = 0.0f;
};

///************************* フィールド復帰データ *************************///
struct FieldReturnData {
	Vector3 playerPosition;
	Vector3 cameraPosition;
	CameraMode cameraMode = CameraMode::FOLLOW;
	std::string defeatedEnemyGroup;
	bool playerWon = false;
	int expGained = 0;
	int goldGained = 0;
	std::vector<std::string> itemsGained;
	float playerHpRatio = 1.0f;
};

///************************* サブシーン遷移タイプ *************************///
enum class SubSceneTransitionType {
	TO_FIELD,
	TO_BATTLE,
	TO_MENU,
	CUSTOM
};

///************************* サブシーン遷移リクエスト *************************///
struct SubSceneTransitionRequest {
	SubSceneTransitionType type;
	void* transitionData = nullptr;
	std::string targetSceneName;
};