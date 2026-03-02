#pragma once
#include "YParticleManager.h"
#include "Vector3.h"
#include <string>
#include <memory>
#include <random> // ランダム生成用
#include "YParticleSystem.h"

//=================================================================
// 形状定義用クラス群
//=================================================================

/// <summary>
/// エミッター形状の基底クラス
/// </summary>
class YEmitterShape {
public:
    virtual ~YEmitterShape() = default;
    /// <summary>
    /// 形状内部のランダムなローカル座標を取得
    /// </summary>
    virtual Vector3 GeneratePoint() = 0;

    // 形状の種類識別用
    enum class Type { Point, Sphere, Box, Cone };
    virtual Type GetType() const = 0;
};

/// <summary>
/// 点（デフォルト）
/// </summary>
class YEmitterPoint : public YEmitterShape {
public:
    Vector3 GeneratePoint() override { return Vector3{ 0, 0, 0 }; }
    Type GetType() const override { return Type::Point; }
};

/// <summary>
/// 球体 / 球殻
/// </summary>
class YEmitterSphere : public YEmitterShape {
public:
    float radius = 1.0f;
    bool shellOnly = false; // trueなら表面のみから発生

    YEmitterSphere(float r, bool shell = false) : radius(r), shellOnly(shell) {}
    Vector3 GeneratePoint() override;
    Type GetType() const override { return Type::Sphere; }
};

/// <summary>
/// 箱型 (AABB)
/// </summary>
class YEmitterBox : public YEmitterShape {
public:
    Vector3 size = { 1.0f, 1.0f, 1.0f };

    YEmitterBox(const Vector3& s) : size(s) {}
    Vector3 GeneratePoint() override;
    Type GetType() const override { return Type::Box; }
};