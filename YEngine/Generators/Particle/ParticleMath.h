#pragma once
#include <random>
#include "Vector3.h"
#include "Vector4.h"

namespace ParticleMath {
    // 0.0f ~ 1.0f のランダムな浮動小数
    inline float Random01() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(gen);
    }

    // 指定範囲のランダム
    inline float RandomRange(float min, float max) {
        return min + (max - min) * Random01();
    }

    // Vector3の指定範囲ランダム
    inline Vector3 RandomVector3(const Vector3& min, const Vector3& max) {
        return {
            RandomRange(min.x, max.x),
            RandomRange(min.y, max.y),
            RandomRange(min.z, max.z)
        };
    }

    // ★ ご要望の関数：半径 radius の球体の中のランダムな座標
    inline Vector3 RandomInSphere(float radius) {
        float phi = RandomRange(0, 2.0f * 3.14159f);
        float theta = RandomRange(0, 3.14159f);
        float r = radius * std::pow(Random01(), 1.0f / 3.0f); // 均一に分布させるため

        return {
            r * std::sin(theta) * std::cos(phi),
            r * std::sin(theta) * std::sin(phi),
            r * std::cos(theta)
        };
    }

    // 線形補間（色や座標用）
    template<typename T>
    inline T Lerp(const T& start, const T& end, float t) {
        return start + (end - start) * t;
    }
}