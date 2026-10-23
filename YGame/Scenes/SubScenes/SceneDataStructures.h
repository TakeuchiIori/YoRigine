#pragma once
#include "Vector3.h"
#include <vector>
#include <string>
#include <variant>

///************************* バトル遷移データ *************************///
struct BattleTransitionData {
	std::string enemyGroup;
	std::string battleEnemyId;  // 単体バトル用
	std::vector<std::string> battleEnemyIds;  // 複数体バトル用
	std::string battleFormation;
	Vector3 playerPosition;
	Vector3 cameraPosition;
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
	std::string defeatedEnemyGroup;
	bool playerWon = false;
	int expGained = 0;
	int goldGained = 0;
	std::vector<std::string> itemsGained;
	float playerHpRatio = 1.0f;
};

///************************* サブシーン遷移ペイロード *************************///
// シーン間の受け渡しデータは型付きの variant で保持する。
// 受け取る側は OnEnter で自分宛ての型だけを std::get_if で取り出す。
// std::monostate はデータ不要な遷移（初回入場・リトライなど）用。
using SubScenePayload = std::variant<std::monostate, BattleTransitionData, FieldReturnData>;

///************************* サブシーン遷移リクエスト *************************///
struct SubSceneTransitionRequest {
	std::string targetSceneName;
	SubScenePayload payload;
};

///************************* サブシーンから親への通知 *************************///
// サブシーンはメインシーンの遷移を自分で行わず、GameScene へ通知して判断を任せる。
enum class SubSceneEvent {
	GameClear, // 最終戦の勝利演出が終わった
};
