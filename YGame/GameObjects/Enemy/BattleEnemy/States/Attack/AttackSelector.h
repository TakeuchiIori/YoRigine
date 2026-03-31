#pragma once
#include "../../BattleEnemy.h"
#include "BattleRushAttackState.h"
#include "BattleJumpAttackState.h"
#include "BattleSpinAttackState.h"
#include "BattleChargeRushAttackState.h"
#include "BattleComboAttackState.h"

#include "Player/Player.h"
#include <memory>
#include <random>
#include <vector>
#include <string>
#include <algorithm>

/// <summary>
/// 攻撃選択の重みづけ設定
/// </summary>
struct AttackWeightConfig {
	// 距離による重み係数
	struct DistanceWeights {
		float veryCloseRange = 5.0f;      // 超近距離判定の閾値
		float closeRange = 8.0f;          // 近距離判定の閾値
		float midRange = 12.0f;           // 中距離判定の閾値
		float farRange = 16.0f;           // 遠距離判定の閾値
	};

	// プレイヤーのHP状態による重み補正
	struct PlayerHPWeights {
		float criticalHP = 2.5f;         // 瀕死時（0-20%）の補正
		float lowHP = 1.8f;              // 低HP時（20-40%）の補正
		float mediumHP = 1.0f;           // 中HP時（40-70%）の補正
		float highHP = 0.7f;             // 高HP時（70-100%）の補正
	};

	// 攻撃パターンごとの距離適性
	struct AttackDistanceAffinity {
		// rush攻撃の距離適性
		float rushVeryClose = 0.5f;
		float rushClose = 0.8f;
		float rushMid = 1.0f;
		float rushFar = 0.4f;

		// spin攻撃の距離適性
		float spinVeryClose = 1.2f;
		float spinClose = 1.0f;
		float spinMid = 0.4f;
		float spinFar = 0.1f;

		// charge攻撃の距離適性
		float chargeVeryClose = 0.3f;
		float chargeClose = 0.5f;
		float chargeMid = 0.8f;
		float chargeFar = 1.0f;

		// leap攻撃の距離適性
		float leapVeryClose = 0.2f;
		float leapClose = 0.4f;
		float leapMid = 0.9f;
		float leapFar = 1.0f;

		// combo攻撃の距離適性
		float comboVeryClose = 1.2f;
		float comboClose = 1.0f;
		float comboMid = 0.5f;
		float comboFar = 0.2f;
	};

	// 攻撃パターンごとのHP適性（プレイヤーのHPが低い時にどれだけ有効か）
	struct AttackHPPriority {
		float rushPriority = 1.0f;       // rush攻撃のHP優先度
		float spinPriority = 1.2f;       // spin攻撃のHP優先度
		float chargePriority = 1.5f;     // charge攻撃のHP優先度（大技）
		float leapPriority = 1.3f;       // leap攻撃のHP優先度
		float comboPriority = 1.8f;      // combo攻撃のHP優先度（トドメ向き）
	};

	// HP閾値
	struct HPThresholds {
		float criticalHPThreshold = 0.2f;  // 20%以下を瀕死と判定
		float lowHPThreshold = 0.4f;       // 40%以下を低HPと判定
		float mediumHPThreshold = 0.7f;    // 70%以下を中HPと判定
	};

	DistanceWeights distance;
	PlayerHPWeights playerHP;
	AttackDistanceAffinity distanceAffinity;
	AttackHPPriority hpPriority;
	HPThresholds hpThreshold;
};

/// <summary>
/// 攻撃パターンごとの重み情報
/// </summary>
struct AttackWeight {
	std::string patternName;
	float baseWeight = 1.0f;
	float distanceWeight = 1.0f;
	float hpWeight = 1.0f;
	float finalWeight = 1.0f;

	AttackWeight(const std::string& name, float base = 1.0f)
		: patternName(name), baseWeight(base) {
	}

	void CalculateFinalWeight() {
		finalWeight = baseWeight * distanceWeight * hpWeight;
	}
};

/// <summary>
/// 敵の攻撃パターンを選択するヘルパークラス
/// プレイヤーの距離とHPに基づいて重み付けで攻撃を選択
/// </summary>
class AttackSelector {
public:
	/// <summary>
	/// 重み付け設定を初期化
	/// </summary>
	static void Initialize(const AttackWeightConfig& config = AttackWeightConfig()) {
		config_ = config;
	}

	/// <summary>
	/// プレイヤーの距離とHPに基づいて重み付けで攻撃を選択
	/// </summary>
	static std::unique_ptr<IEnemyState<BattleEnemy>> SelectWeightedAttack(
		const BattleEnemy& enemy)
	{
		if (!enemy.GetPlayer()) {
			return std::make_unique<BattleRushAttackState>();
		}

		const auto& patterns = enemy.GetEnemyData().attackPatterns;
		if (patterns.empty()) {
			return std::make_unique<BattleRushAttackState>();
		}

		// 距離とHP情報を取得
		Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
		float distance = Length(toPlayer);

		const Player* player = enemy.GetPlayer();
		float hpRatio = static_cast<float>(player->GetHP()) / static_cast<float>(player->GetMaxHP());

		// 各攻撃の重みを計算
		std::vector<AttackWeight> weights = CalculateWeights(patterns, distance, hpRatio);

		// 重みに基づいて攻撃を選択
		std::string selectedPattern = SelectByWeight(weights);

		return CreateAttackState(selectedPattern);
	}

	/// <summary>
	/// 距離に応じて最適な攻撃を選択（従来のロジック保持）
	/// </summary>
	static std::unique_ptr<IEnemyState<BattleEnemy>> SelectSmartAttack(
		const BattleEnemy& enemy) {
		if (!enemy.GetPlayer()) {
			return std::make_unique<BattleRushAttackState>();
		}

		const auto& patterns = enemy.GetEnemyData().attackPatterns;
		Vector3 toPlayer = enemy.GetPlayerPosition() - enemy.GetTranslate();
		float distance = Length(toPlayer);

		// 条件に合致する攻撃名を格納するリスト
		std::vector<std::string> candidatePatterns;

		// --- 距離に応じた候補の絞り込み ---
		// 遠距離
		if (distance > config_.distance.midRange) {
			if (HasPattern(patterns, "jump")) candidatePatterns.push_back("jump");
			if (HasPattern(patterns, "chargeRush")) candidatePatterns.push_back("chargeRush");
		}
		// 中距離
		else if (distance > config_.distance.closeRange) {
			if (HasPattern(patterns, "rush")) candidatePatterns.push_back("rush");
			// もし中距離に他の候補を追加したい場合はここに追記
		}
		// 近距離
		else {
			if (HasPattern(patterns, "spin")) candidatePatterns.push_back("spin");
			if (HasPattern(patterns, "combo")) candidatePatterns.push_back("combo");
		}

		// --- 抽選処理 ---
		// 候補が見つかった場合、その中からランダムで1つ選ぶ
		if (!candidatePatterns.empty()) {
			static std::random_device rd;
			static std::mt19937 gen(rd());
			std::uniform_int_distribution<size_t> dist(0, candidatePatterns.size() - 1);

			std::string selected = candidatePatterns[dist(gen)];
			return CreateAttackState(selected);
		}

		// 候補が空（その距離に適した攻撃を所持していない）場合は、
		// 全所持パターンから重み付けで選ぶフォールバックへ
		return SelectWeightedAttack(enemy);
	}
private:
	/// <summary>
	/// 各攻撃の重みを計算
	/// </summary>
	static std::vector<AttackWeight> CalculateWeights(
		const std::vector<std::string>& patterns,
		float distance,
		float hpRatio)
	{
		std::vector<AttackWeight> weights;

		for (const auto& pattern : patterns) {
			AttackWeight weight(pattern, 1.0f);

			// 距離による重み調整
			weight.distanceWeight = GetDistanceWeight(pattern, distance);

			// プレイヤーのHP状態による重み調整
			weight.hpWeight = GetPlayerHPWeight(pattern, hpRatio);

			// 最終重みを計算
			weight.CalculateFinalWeight();

			weights.push_back(weight);
		}

		return weights;
	}

	/// <summary>
	/// 距離に基づく重み係数を取得
	/// </summary>
	static float GetDistanceWeight(const std::string& pattern, float distance) {
		const auto& affinity = config_.distanceAffinity;

		// 距離レンジを判定して適性値を返す
		if (distance < config_.distance.veryCloseRange) {
			// 超近距離（0-2m）
			if (pattern == "rush") return affinity.rushVeryClose;
			if (pattern == "spin") return affinity.spinVeryClose;
			if (pattern == "chargeRush") return affinity.chargeVeryClose;
			if (pattern == "jump") return affinity.leapVeryClose;
			if (pattern == "combo") return affinity.comboVeryClose;
		}
		else if (distance < config_.distance.closeRange) {
			// 近距離（2-4m）
			if (pattern == "rush") return affinity.rushClose;
			if (pattern == "spin") return affinity.spinClose;
			if (pattern == "chargeRush") return affinity.chargeClose;
			if (pattern == "jump") return affinity.leapClose;
			if (pattern == "combo") return affinity.comboClose;
		}
		else if (distance < config_.distance.midRange) {
			// 中距離（4-8m）
			if (pattern == "rush") return affinity.rushMid;
			if (pattern == "spin") return affinity.spinMid;
			if (pattern == "chargeRush") return affinity.chargeMid;
			if (pattern == "jump") return affinity.leapMid;
			if (pattern == "combo") return affinity.comboMid;
		}
		else if (distance < config_.distance.farRange) {
			// 遠距離（8-15m）
			if (pattern == "rush") return affinity.rushFar;
			if (pattern == "spin") return affinity.spinFar;
			if (pattern == "chargeRush") return affinity.chargeFar;
			if (pattern == "jump") return affinity.leapFar;
			if (pattern == "combo") return affinity.comboFar;
		}
		else {
			// 超遠距離（15m以上）
			if (pattern == "chargeRush") return affinity.chargeFar * 1.2f;
			if (pattern == "jump") return affinity.leapFar * 1.2f;
			return 0.1f; // その他の攻撃は超遠距離では不向き
		}

		return 1.0f;
	}

	/// <summary>
	/// プレイヤーのHP状態に基づく重み係数を取得
	/// </summary>
	static float GetPlayerHPWeight(const std::string& pattern, float hpRatio) {
		const auto& priority = config_.hpPriority;
		float hpMultiplier = 1.0f;

		// HP状態による基本補正を取得
		if (hpRatio <= config_.hpThreshold.criticalHPThreshold) {
			// 瀕死（0-20%）：積極的にトドメを狙う
			hpMultiplier = config_.playerHP.criticalHP;
		}
		else if (hpRatio <= config_.hpThreshold.lowHPThreshold) {
			// 低HP（20-40%）：攻撃的に
			hpMultiplier = config_.playerHP.lowHP;
		}
		else if (hpRatio <= config_.hpThreshold.mediumHPThreshold) {
			// 中HP（40-70%）：標準
			hpMultiplier = config_.playerHP.mediumHP;
		}
		else {
			// 高HP（70-100%）：慎重に
			hpMultiplier = config_.playerHP.highHP;
		}

		// 攻撃パターンごとのHP優先度を取得
		float attackPriority = 1.0f;
		if (pattern == "rush") attackPriority = priority.rushPriority;
		else if (pattern == "spin") attackPriority = priority.spinPriority;
		else if (pattern == "charge") attackPriority = priority.chargePriority;
		else if (pattern == "jump") attackPriority = priority.leapPriority;
		else if (pattern == "combo") attackPriority = priority.comboPriority;

		return hpMultiplier * attackPriority;
	}

	/// <summary>
	/// 重みに基づいて攻撃を選択
	/// </summary>
	static std::string SelectByWeight(const std::vector<AttackWeight>& weights) {
		if (weights.empty()) {
			return "rush";
		}

		// 総重みを計算
		float totalWeight = 0.0f;
		for (const auto& weight : weights) {
			totalWeight += weight.finalWeight;
		}

		if (totalWeight <= 0.0f) {
			return weights.front().patternName;
		}

		// ランダム値を生成（0.0 ~ totalWeight）
		static std::random_device rd;
		static std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dist(0.0f, totalWeight);
		float randomValue = dist(gen);

		// 重みに基づいて選択
		float cumulative = 0.0f;
		for (const auto& weight : weights) {
			cumulative += weight.finalWeight;
			if (randomValue <= cumulative) {
				return weight.patternName;
			}
		}

		// フォールバック（通常は到達しない）
		return weights.back().patternName;
	}

	/// <summary>
	/// 攻撃名から状態を生成
	/// </summary>
	static std::unique_ptr<IEnemyState<BattleEnemy>> CreateAttackState(
		const std::string& patternName)
	{
		if (patternName == "rush") {
			return std::make_unique<BattleRushAttackState>();
		}
		else if (patternName == "jump") {
			return std::make_unique<BattleJumpAttackState>();
		}
		else if (patternName == "spin") {
			return std::make_unique<BattleSpinAttackState>();
		}
		else if (patternName == "chargeRush") {
			return std::make_unique<BattleChargeRushAttackState>();
		}
		else if (patternName == "combo") {
			return std::make_unique<BattleComboAttackState>();
		}

		// デフォルト
		return std::make_unique<BattleRushAttackState>();
	}

	/// <summary>
	/// パターンリストに指定の攻撃が含まれるか
	/// </summary>
	static bool HasPattern(const std::vector<std::string>& patterns,
		const std::string& name)
	{
		return std::find(patterns.begin(), patterns.end(), name) != patterns.end();
	}

	// 設定データ
	static AttackWeightConfig config_;
};

// 静的メンバの定義
inline AttackWeightConfig AttackSelector::config_ = AttackWeightConfig();