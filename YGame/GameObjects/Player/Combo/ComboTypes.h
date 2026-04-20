#pragma once

#include <string>
#include <vector>
#include <map>
#include "Vector3.h"
#include "Vector2.h"

// Engine
#include "Loaders/Json/StructSerializer.h"

// ============================================================
// コンボ状態の定義
// プレイヤーの攻撃フェーズと連携可能状態を表す
// ============================================================
enum class ComboState {
	Idle,           // 待機状態
	Attacking,      // 攻撃実行中
	CanContinue,    // 次の攻撃へ連携可能
	Recovery,       // 攻撃終了後の硬直時間
	Finished        // コンボ終了
};

// ============================================================
// 攻撃タイプの定義
// 操作によって使い分ける攻撃の種別
// ============================================================
enum class AttackType {
	A_Arte,         // 軽攻撃（素早い、CC消費小）
	B_Arte,         // 重攻撃（威力高、CC消費中）
};

// ============================================================
// 攻撃データ構造体
// 個別の攻撃アクションに関するフレー��・ダメージ・消費コストなど全てを管理する
// ゲームロジック（秒）とエディタ（フレーム）で値を共有する設計
// ============================================================
struct AttackData {

	// ------------------------------------------------------------
	// 識別・基本情報
	// ------------------------------------------------------------
	std::string name;
	std::string animationName;
	AttackType  type = AttackType::A_Arte;

	// ------------------------------------------------------------
	// タイムライン設定（エディタ用・フレーム単位）
	// ドープシートで直接編集される値群
	// ------------------------------------------------------------
	int totalFrames = 60;
	int fps = 60;

	int hitStart = 0;
	int hitEnd = 10;
	int recoveryStart = 0;
	int recoveryEnd = 0;
	int cancelStart = 0;
	int comboWindowStart = 0;
	int comboWindowEnd = 0;
	int invincibleStart = 0;
	int invincibleEnd = 0;

	struct FrameEvent {
		int frame = 0;
		std::string tag;
	};
	std::vector<FrameEvent> effects;
	std::vector<FrameEvent> sounds;

	// ------------------------------------------------------------
	// タイミング設定（ゲームロジック用・秒単位）
	// フレーム設定から同期・自動計算される値
	// ------------------------------------------------------------
	float duration = 0.0f;
	float recovery = 0.0f;
	float continueWindow = 0.0f;

	// ------------------------------------------------------------
	// ダメージと物理効果設定
	// ------------------------------------------------------------
	float baseDamage = 0.0f;
	float knockback = 0.0f;
	float knockbackDuration = 0.0f;
	float stepDistance = 0.0f;

	// ------------------------------------------------------------
	// コンバットコスト（CC）設定
	// ------------------------------------------------------------
	int ccCost = 0;
	int ccOnHit = 0;

	// ------------------------------------------------------------
	// コンボ・連携特性
	// ------------------------------------------------------------
	bool canCancel = true;
	bool canChainToAny = true;
	std::vector<AttackType> preferredNext;

	// ------------------------------------------------------------
	// 特殊効果フラグ
	// ------------------------------------------------------------
	bool launches = false;
	bool wallBounce = false;
	bool groundBounce = false;
	std::string effect;
	float motionSpeed = 1.0f;

	// ------------------------------------------------------------
	// フレーム値から秒単位のフィールドを同期・計算する
	// ------------------------------------------------------------
	void SyncFramesToSeconds()
	{
		if (fps <= 0) return;
		// 必要に応じて計算処理を追加
	}

	AttackData() = default;
};

// ============================================================
// CC（チェインキャパシティ）のルール設定
// ============================================================
struct CCConfig {
	int maxCC = 5;
	float regenRate = 1.0f;
	float regenDelay = 1.5f;
	int dodgeRecovery = 2;
	int counterRecovery = 1;

	CCConfig() = default;
	CCConfig(int max, float rate, float delay, int dodge, int counter)
		: maxCC(max), regenRate(rate), regenDelay(delay),
		dodgeRecovery(dodge), counterRecovery(counter) {
	}
};

// ============================================================
// コンボシステムの挙動設定
// ============================================================
struct ComboConfig {
	int maxLength = 20;
	float damageDecay = 0.95f;
	float chainBonus = 1.15f;
	bool enableFreeChain = true;
	float comboResetTime = 1.0f;

	ComboConfig() = default;
	ComboConfig(int length, float decay, float bonus, bool freeChain, float resetTime)
		: maxLength(length), damageDecay(decay), chainBonus(bonus),
		enableFreeChain(freeChain), comboResetTime(resetTime) {
	}
};

// ============================================================
// JSON用シリアライザー定義マクロ
// ============================================================

BEGIN_STRUCT_SERIALIZER(AttackData)
SERIALIZE_FIELD(AttackData, name)
SERIALIZE_FIELD(AttackData, animationName)
SERIALIZE_ENUM_FIELD(AttackData, type)

SERIALIZE_FIELD(AttackData, totalFrames)
SERIALIZE_FIELD(AttackData, fps)
SERIALIZE_FIELD(AttackData, hitStart)
SERIALIZE_FIELD(AttackData, hitEnd)
SERIALIZE_FIELD(AttackData, recoveryStart)
SERIALIZE_FIELD(AttackData, recoveryEnd)
SERIALIZE_FIELD(AttackData, cancelStart)
SERIALIZE_FIELD(AttackData, comboWindowStart)
SERIALIZE_FIELD(AttackData, comboWindowEnd)
SERIALIZE_FIELD(AttackData, invincibleStart)
SERIALIZE_FIELD(AttackData, invincibleEnd)

SERIALIZE_FIELD(AttackData, duration)
SERIALIZE_FIELD(AttackData, recovery)
SERIALIZE_FIELD(AttackData, continueWindow)

SERIALIZE_FIELD(AttackData, baseDamage)
SERIALIZE_FIELD(AttackData, knockback)
SERIALIZE_FIELD(AttackData, knockbackDuration)
SERIALIZE_FIELD(AttackData, stepDistance)

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
END_STRUCT_SERIALIZER(AttackData)

BEGIN_STRUCT_SERIALIZER(CCConfig)
SERIALIZE_FIELD(CCConfig, maxCC)
SERIALIZE_FIELD(CCConfig, regenRate)
SERIALIZE_FIELD(CCConfig, regenDelay)
SERIALIZE_FIELD(CCConfig, dodgeRecovery)
SERIALIZE_FIELD(CCConfig, counterRecovery)
END_STRUCT_SERIALIZER(CCConfig)

BEGIN_STRUCT_SERIALIZER(ComboConfig)
SERIALIZE_FIELD(ComboConfig, maxLength)
SERIALIZE_FIELD(ComboConfig, damageDecay)
SERIALIZE_FIELD(ComboConfig, chainBonus)
SERIALIZE_FIELD(ComboConfig, enableFreeChain)
SERIALIZE_FIELD(ComboConfig, comboResetTime)
END_STRUCT_SERIALIZER(ComboConfig)