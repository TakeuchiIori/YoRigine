#pragma once

#include <string>
#include <vector>
#include <map>
#include "Vector3.h"

// Engine
#include "Loaders/Json/StructSerializer.h"

// コンボ状態
enum class ComboState {
	Idle,           // 待機状態
	Attacking,      // 攻撃中
	CanContinue,    // 次の攻撃可能
	Recovery,       // 硬直中
	Finished        // コンボ終了
};

// 攻撃タイプ
enum class AttackType {
	A_Arte,         // A（軽攻撃）- 素早い基本攻撃、CC消費少
	B_Arte,         // B（重攻撃）- 威力の高い特殊攻撃、CC消費中
	Arcane_Arte     // 奥義（必殺技）- 最高威力の技、CC消費大
};

// 攻撃データ（完全自由式）
struct AttackData {
	std::string name;						// 攻撃名
	std::string animationName;				// アニメーション名
	AttackType type;						// 攻撃タイプ
	std::string cameraEffect = "None";

	// 当たり判定の時間
	Vector2 hitDuration;					// ヒット判定の開始と終了時間（秒）

	// タイミング設定
	float duration;							// 攻撃時間（秒）
	float recovery;							// 硬直時間（秒）
	float continueWindow;					// 次の攻撃受付時間（秒）

	// ダメージ・効果
	float baseDamage;						// 基本ダメージ
	float knockback;						// ノックバック力
	float knockbackDuration;				// ノックバック持続時間
	Vector3 attackRange;					// 攻撃範囲（x:幅, y:高さ, z:奥行）

	// CC（コンバットコスト）システム
	int ccCost;								// CC消費量
	int ccOnHit;							// ヒット時CC回復量（カウンター等）

	// コンボ特性
	bool canCancel;							// 他の攻撃でキャンセル可能
	bool canChainToAny;						// 任意の攻撃に繋げられる
	std::vector<AttackType> preferredNext;	// 推奨次攻撃（ボーナス有）

	float stepDistance = 0.0f;

	// その他効果
	bool launches;							// 敵を浮かす
	bool wallBounce;						// 壁バウンド誘発
	bool groundBounce;						// 地面バウンド誘発
	std::string effect;						// 特殊エフェクト名
	float motionSpeed = 1.0f;				// アニメーション速度


	// デフォルトコンストラクタ
	AttackData() = default;

	// 全項目指定コンストラクタ
	AttackData(const std::string& n, const std::string& anim, AttackType t, std::string camEffect,
		Vector2 hitDur,float dur, float rec, float window, float dmg, float kb, float kbd,
		const Vector3& range, int cc, int ccHit, bool cancel, bool chain,
		const std::vector<AttackType>& preferred, float stepDis, bool launch, bool wall, bool ground,
		const std::string& fx, float animSpeed = 1.0f)
		: name(n), animationName(anim), type(t), cameraEffect(camEffect), hitDuration(hitDur),duration(dur), recovery(rec),
		continueWindow(window), baseDamage(dmg), knockback(kb), knockbackDuration(kbd), attackRange(range),
		ccCost(cc), ccOnHit(ccHit), canCancel(cancel), canChainToAny(chain),
		preferredNext(preferred), stepDistance(stepDis), launches(launch), wallBounce(wall),
		groundBounce(ground), effect(fx), motionSpeed(animSpeed) {
	}
};

// CC設定
struct CCConfig {
	int maxCC = 5;                  // 最大CC
	float regenRate = 1.0f;         // CC回復速度（秒あたり）
	float regenDelay = 1.5f;        // 攻撃後の回復開始遅延（秒）
	int dodgeRecovery = 2;          // 回避成功時のCC回復量
	int counterRecovery = 1;        // カウンター成功時のCC回復量

	CCConfig() = default;

	CCConfig(int max, float rate, float delay, int dodge, int counter)
		: maxCC(max), regenRate(rate), regenDelay(delay),
		dodgeRecovery(dodge), counterRecovery(counter) {
	}
};

// コンボ設定
struct ComboConfig {
	int maxLength = 20;             // 最大コンボ長
	float damageDecay = 0.95f;      // コンボ減衰率（3ヒット目以降）
	float chainBonus = 1.15f;       // 推奨チェーンボーナス倍率
	bool enableFreeChain = true;    // 自由チェーン有効
	float comboResetTime = 1.0f;    // コンボリセット時間（秒）

	ComboConfig() = default;

	ComboConfig(int length, float decay, float bonus, bool freeChain, float resetTime)
		: maxLength(length), damageDecay(decay), chainBonus(bonus),
		enableFreeChain(freeChain), comboResetTime(resetTime) {
	}
};

// AttackDataのシリアライザー登録
BEGIN_STRUCT_SERIALIZER(AttackData)
SERIALIZE_FIELD(AttackData, name)
SERIALIZE_FIELD(AttackData, animationName)
SERIALIZE_ENUM_FIELD(AttackData, type)
SERIALIZE_FIELD(AttackData, cameraEffect)
SERIALIZE_FIELD(AttackData, hitDuration);
SERIALIZE_FIELD(AttackData, duration)
SERIALIZE_FIELD(AttackData, recovery)
SERIALIZE_FIELD(AttackData, continueWindow)
SERIALIZE_FIELD(AttackData, baseDamage)
SERIALIZE_FIELD(AttackData, knockback)
SERIALIZE_FIELD(AttackData, knockbackDuration)
SERIALIZE_FIELD(AttackData, attackRange)
SERIALIZE_FIELD(AttackData, ccCost)
SERIALIZE_FIELD(AttackData, ccOnHit)
SERIALIZE_FIELD(AttackData, canCancel)
SERIALIZE_FIELD(AttackData, canChainToAny)
SERIALIZE_FIELD(AttackData, preferredNext)
SERIALIZE_FIELD(AttackData, launches)
SERIALIZE_FIELD(AttackData, wallBounce)
SERIALIZE_FIELD(AttackData, groundBounce)
SERIALIZE_FIELD(AttackData, effect)
SERIALIZE_FIELD(AttackData, motionSpeed)
SERIALIZE_FIELD(AttackData, stepDistance)
END_STRUCT_SERIALIZER(AttackData)

// CCConfigのシリアライザー登録
BEGIN_STRUCT_SERIALIZER(CCConfig)
SERIALIZE_FIELD(CCConfig, maxCC)
SERIALIZE_FIELD(CCConfig, regenRate)
SERIALIZE_FIELD(CCConfig, regenDelay)
SERIALIZE_FIELD(CCConfig, dodgeRecovery)
SERIALIZE_FIELD(CCConfig, counterRecovery)
END_STRUCT_SERIALIZER(CCConfig)

// ComboConfigのシリアライザー登録
BEGIN_STRUCT_SERIALIZER(ComboConfig)
SERIALIZE_FIELD(ComboConfig, maxLength)
SERIALIZE_FIELD(ComboConfig, damageDecay)
SERIALIZE_FIELD(ComboConfig, chainBonus)
SERIALIZE_FIELD(ComboConfig, enableFreeChain)
SERIALIZE_FIELD(ComboConfig, comboResetTime)
END_STRUCT_SERIALIZER(ComboConfig)