#pragma once
// C++
#include <string>

// Engine
#include "ParticleManager.h"
#include "WorldTransform./WorldTransform.h"
#include "Loaders/Json/JsonManager.h"

// Math
#include  "Vector3.h"


/// <summary>
/// CPUパーティクルエミッタークラス
/// </summary>
class ParticleEmitter
{
public:
	///************************* 基本関数 *************************///
	ParticleEmitter(const std::string& particleName, const Vector3& position = Vector3{ 0,0,0 }, uint32_t count = 1);
	void Update(float deltaTime);

public:
	///************************* アクセッサ *************************///
	void SetPosition(const Vector3& position) { position_ = position; }
	void SetEmissionRate(float rate) { emissionRate_ = rate; }
	void SetActive(bool active) { isActive_ = active; }
	void SetAutoEmit(bool autoEmit) { autoEmit_ = autoEmit; }
	void SetCount(uint32_t count) { count_ = count; }
	const Vector3& GetPosition() const { return position_; }
	float GetEmissionRate() const { return emissionRate_; }
	bool IsActive() const { return isActive_; }
	bool GetAutoEmit() const { return autoEmit_; }
	uint32_t GetCount() const { return count_; }

public:
	///************************* メンバ関数 *************************///
	void Emit(int count = -1);
	void EmitBurst(int count);
	void FollowEmit(const Vector3& pos, int count);

	// パーティクル設定への直接アクセス
	void SetParticleGravity(const Vector3& gravity);
	void SetParticleColor(const Vector4& startColor, const Vector4& endColor);
	void SetParticleSpeed(float speed);
	void SetParticleLifeTime(const Vector2& lifeTimeRange);


private:
	///************************* メンバ変数 *************************///
	std::string particleName_;
	Vector3 position_;
	float emissionRate_;
	float timer_;
	bool isActive_;
	bool autoEmit_;
	uint32_t count_;
};

