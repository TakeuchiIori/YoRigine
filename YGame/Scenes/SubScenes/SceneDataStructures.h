#pragma once
#include "Vector3.h"
#include <vector>
#include <string>
#include <variant>
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
	// フィールド敵の見た目スケール（バトル敵へ引き継ぐ）
	Vector3 battleEnemyScale = Vector3(1.0f, 1.0f, 1.0f);
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

///************************* サブシーン遷移ペイロード *************************///
// シーン間の受け渡しデータは型付きの variant で保持する。
// std::monostate はデータ不要な遷移（メニュー遷移など）用。
using SubScenePayload = std::variant<std::monostate, BattleTransitionData, FieldReturnData>;

///************************* サブシーン遷移リクエスト *************************///
struct SubSceneTransitionRequest {
	SubSceneTransitionType type = SubSceneTransitionType::TO_FIELD;
	SubScenePayload payload;
	std::string targetSceneName;
};