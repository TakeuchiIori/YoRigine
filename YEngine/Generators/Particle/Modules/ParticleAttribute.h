#pragma once
#include "Vector3.h"
#include "Vector4.h"

struct ParticleAttribute {
    Vector3 position;
    Vector3 velocity;
    Vector3 scale;
    Vector3 rotation;
    Vector4 color;
	float lifeTime = 1.0f;  // パーティクルの寿命（秒）
	float currentTime = 0.0f; // パーティクルが生成されてからの経過時間（秒）
    bool isActive = false;
    // ageは計算プロパティとして扱う（フィールドではなく都度計算）
    float GetNormalizedAge() const {
        return (lifeTime > 0.0f) ? (currentTime / lifeTime) : 1.0f;
    }
};