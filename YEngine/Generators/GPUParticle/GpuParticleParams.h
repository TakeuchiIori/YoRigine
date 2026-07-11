#pragma once
#include "Vector3.h"
#include <vector>

// ──── GPU Force Field ──────────────────────────────────────────────────────
enum class GpuFieldShape : uint32_t { Sphere = 0, AABB = 1 };
enum class GpuFieldMode  : uint32_t { DirectionalAccel = 0, ConvergeToCenter = 1, RadialRepel = 2 };

struct GpuForceFieldParams {
	GpuFieldShape shape          = GpuFieldShape::Sphere;
	Vector3       center         = {};
	Vector3       halfExtents    = { 5.f, 5.f, 5.f };
	float         radius         = 5.0f;

	GpuFieldMode  mode           = GpuFieldMode::ConvergeToCenter;
	Vector3       direction      = { 0.f, 1.f, 0.f };
	float         strength       = 10.0f;

	float falloff             = 0.5f;
	float spiralStrengthMin   = 0.0f;
	float spiralStrengthMax   = 0.0f;
	float randomAxisBlend     = 0.0f;
	float orbitHoldRatio      = 0.0f;
	float approachVariance    = 0.0f;
	float maxSpeed            = 0.0f;
	float killRadius          = 0.0f;

	bool isEnable = true;
};
// ──────────────────────────────────────────────────────────────────────────

struct ChildParticleParams {
	bool isTrail = false;      // トレイルを出すか
	bool isInheritScale = true;
	float minDistance = 0.5f;   // どのくらい動いたら出すか（密度）
	float lifeTime = 0.5f;      // 子の寿命
	float startScale = 0.5f;    // 親に対するサイズの倍率
	float endScale = 0.0f;      // 消える時のサイズ
	int emissionCount = 1;      // 1回に何個出すか
};

struct ParticleParams {
	// 生存時間
	float lifeTime = 3.0f;
	float lifeTimeVariance = 0.5f;

	// スケール
	Vector3 startScale = { 1.0f, 1.0f, 1.0f };
	Vector3 startScaleVariance = { 0.3f, 0.3f, 0.3f };
	Vector3 endScale = { 0.0f, 0.0f, 0.0f };
	Vector3 endScaleVariance = { 0.3f, 0.3f, 0.3f };

	// 回転
	float rotation = 0.0f;
	float rotationVariance = 0.0f;
	float rotationSpeed = 0.0f;
	float rotationSpeedVariance = 0.0f;

	// 速度
	Vector3 velocity = { 0.0f, 0.1f, 0.0f };
	Vector3 velocityVariance = { 0.1f, 0.05f, 0.1f };

	// 色
	Vector4 startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 startColorVariance = { 0.0f, 0.0f, 0.0f, 0.0f };
	Vector4 endColor = { 1.0f, 1.0f, 1.0f, 0.0f };
	Vector4 endColorVariance = { 0.0f, 0.0f, 0.0f, 0.0f };

	// 重力
	float gravity = 0.0f;
	// ビルボード
	bool isBillboard = true;

	// トレイル
	ChildParticleParams child;
};