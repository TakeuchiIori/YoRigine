#pragma once
#include <string>
#include <vector>
#include "Vector3.h"


///************************* 状態定義 *************************///
enum class BattleEnemyState {
	Idle,       // 待機
	Approach,   // 接近
	Attack,     // 攻撃
	Damaged,    // 被弾
	Dead        // 撃破
};

///************************* 攻撃用構造体 *************************///

// 突進攻撃
struct RushAttackParams {
	// 予備動作フェーズ（後ろに引く）
	float anticipationTime = 0.5f;     // 予備動作時間
	float anticipationDistance = 1.5f; // 後退する距離

	float chargeTime = 1.0f;      // 溜め時間
	float rushTime = 0.5f;        // 突進している時間
	float speedMultiplier = 7.0f; // 速度倍率
	float cooldownTime = 1.2f;    // 突進後の硬直
};

// 強力な突進攻撃
struct ChargeRushAttackParams {
	// 予備動作フェーズ（地面を踏み込む）
	float anticipationTime = 0.8f;       // 予備動作時間
	float stompIntensity = 0.4f;         // 踏み込みの沈み込み
	float anticipationColorPulseSpeed = 8.0f; // 色の点滅速度

	float chargeTime = 1.5f;       // 長い溜め
	float rushTime = 0.5f;         // 突進時間
	float speedMultiplier = 12.0f; // より速い突進
	float cooldownTime = 1.0f;
};

struct SpinAttackParams {
	// 予備動作フェーズ（体を捻る）
	float anticipationTime = 0.5f;         // 予備動作時間
	float twistAngle = 1.57f;              // 捻る角度（ラジアン、約90度）
	float anticipationColorIntensity = 0.7f; // 予備動作中の色の強さ

	float chargeTime = 0.3f;
	float spinTime = 1.0f;
	float rotationCount = 2.0f;    // 何回転するか
	float moveSpeedMultiplier = 2.0f; // 回転中の移動速度倍率
	float cooldownTime = 0.5f;
};

// しゃがんで攻撃
struct JumpAttackParams {
	// 予備動作フェーズ（深くしゃがむ）
	float anticipationTime = 0.7f;     // 予備動作時間
	float anticipationCrouchDepth = 0.8f; // 予備動作の沈み込み
	float anticipationColorPulseSpeed = 6.0f; // 色の点滅速度

	float chargeTime = 0.5f;
	float jumpTime = 0.7f;         // 空中にいる時間
	float jumpHeight = 4.0f;       // ジャンプの高さ
	float crouchDepth = 0.3f;      // 溜め時の沈み込み
	float cooldownTime = 0.6f;
};

struct ComboAttackParams {
	// 予備動作フェーズ（素早く後退）
	float anticipationTime = 0.4f;         // 予備動作時間
	float anticipationBackstepDistance = 1.0f; // 後退距離
	float anticipationColorIntensity = 0.8f;   // 予備動作中の色の強さ

	float phaseDuration = 0.8f;    // 1回ごとのコンボ時間
	float subChargeTime = 0.4f;    // コンボ内での溜め
	float subRushTime = 0.2f;      // コンボ内での突進
	float rushSpeedMultiplier = 8.0f;
	float cooldownTime = 0.8f;
};

// 各状態の攻撃パラメータをまとめた構造体
struct EnemyAttackParams {
	RushAttackParams rush;
	ChargeRushAttackParams chargeRush;
	SpinAttackParams spin;
	JumpAttackParams jump;
	ComboAttackParams combo;
};

///************************* 基本データ *************************///
struct BattleEnemyData {
	std::string enemyId;
	std::vector<std::string> enemyIds;
	std::string modelPath;

	int currentHp_;
	int maxHp_;

	int level = 1;
	int hp = 100;
	int attack = 15;
	int defense = 10;
	float moveSpeed = 5.0f;

	// 接近状態に入る距離
	float approachStateRange = 15.0f;
	// 攻撃状態に入る距離
	float attackStateRange = 10.0f;

	std::vector<std::string> attackPatterns = { "rush","spin","chargeRush","combo","jump" };
	// 攻撃調整用パラメータ
	EnemyAttackParams attackParams;
};

///************************* ノックバック *************************///
struct KnockbackData {
	bool isKnockingBack_ = false;
	Vector3 knockbackDirection_;
	float knockbackPower_ = 0.0f;
	float knockbackDuration_ = 0.0f;
	float knockbackTimer_ = 0.0f;
};