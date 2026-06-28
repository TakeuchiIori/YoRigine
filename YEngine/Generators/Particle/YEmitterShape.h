#pragma once
#include "Vector3.h"
#include <memory>
#include <random>
#include <cmath>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

//=================================================================
// エミッター形状クラス群
//
// パーティクルの生成位置をランダムに決定する。
// 全ての形状は min~max の範囲設定に対応しており、
// エディターから直接スライダーで調整できる。
//=================================================================

// ────────────────────────────────────────────────────────────────
// 基底クラス
// ────────────────────────────────────────────────────────────────
class YEmitterShape {
public:
    virtual ~YEmitterShape() = default;

    /// 形状内のランダムなローカル座標を返す
    virtual Vector3 GeneratePoint() = 0;

    /// ImGui エディター（形状パラメーターを編集できる UI を描画）
    virtual void DrawEditor() {}

    /// 形状タイプ識別
    enum class Type { Point, Sphere, Box, Cone };
    virtual Type GetType() const = 0;
};

// ────────────────────────────────────────────────────────────────
// 点（デフォルト: 全パーティクルが同一点から放出）
// ────────────────────────────────────────────────────────────────
class YEmitterPoint : public YEmitterShape {
public:
    Vector3 GeneratePoint() override { return {}; }
    Type    GetType()       const override { return Type::Point; }
    void    DrawEditor()    override {
#ifdef USE_IMGUI
        ImGui::TextDisabled("点放出（パラメーターなし）");
#endif
    }
};

// ────────────────────────────────────────────────────────────────
// 球体 / 球殻
//
// minRadius〜maxRadius のランダム半径で放出。
// minRadius = maxRadius にすると完全な球殻になる。
// shellOnly = true は後方互換のまま残す（minRadius = maxRadius と同義）
// ────────────────────────────────────────────────────────────────
class YEmitterSphere : public YEmitterShape {
public:
    float minRadius = 0.0f;   // 内側半径
    float maxRadius = 1.0f;   // 外側半径
    bool  shellOnly = false;  // true = 表面のみ（minRadius = maxRadius）

    /// 後方互換コンストラクタ（旧 API: radius + shellOnly）
    YEmitterSphere(float radius = 1.0f, bool shell = false)
        : maxRadius(radius), shellOnly(shell)
    {
        minRadius = shell ? radius : 0.0f;
    }

    /// min/max 指定コンストラクタ（新 API）
    YEmitterSphere(float minR, float maxR)
        : minRadius(minR), maxRadius(maxR), shellOnly(minR == maxR)
    {}

    Vector3 GeneratePoint() override;
    Type    GetType()       const override { return Type::Sphere; }
    void    DrawEditor()    override;
};

// ────────────────────────────────────────────────────────────────
// 箱型 (AABB)
//
// 各軸で minSize〜maxSize のランダムサイズ範囲から放出。
// minSize = {0,0,0} なら中心から外側まで、minSize > 0 なら内側をくり抜いた形状。
// ────────────────────────────────────────────────────────────────
class YEmitterBox : public YEmitterShape {
public:
    Vector3 minSize = { 0.0f, 0.0f, 0.0f };  // 内側サイズ（ハーフサイズ）
    Vector3 maxSize = { 1.0f, 1.0f, 1.0f };  // 外側サイズ（ハーフサイズ）

    /// 後方互換コンストラクタ（旧 API: 単一サイズ）
    YEmitterBox(const Vector3& size = { 1,1,1 })
        : maxSize(size)
    {}

    /// min/max 指定コンストラクタ（新 API）
    YEmitterBox(const Vector3& minS, const Vector3& maxS)
        : minSize(minS), maxSize(maxS)
    {}

    Vector3 GeneratePoint() override;
    Type    GetType()       const override { return Type::Box; }
    void    DrawEditor()    override;
};

// ────────────────────────────────────────────────────────────────
// コーン（円錐台）
//
// 指定方向に向いた円錐の内部から放出する。
// innerAngle〜outerAngle の範囲で放出方向を制御。
// 用途: 炎の噴射、スラッシュの斬撃、ビーム
// ────────────────────────────────────────────────────────────────
class YEmitterCone : public YEmitterShape {
public:
    Vector3 direction  = { 0.0f, 1.0f, 0.0f }; // コーン軸方向
    float   innerAngle = 0.0f;   // 内側の半頂角（度）0 = 中心軸のみ
    float   outerAngle = 25.0f;  // 外側の半頂角（度）
    float   minRadius  = 0.0f;   // 底面の内側半径
    float   maxRadius  = 1.0f;   // 底面の外側半径
    float   height     = 2.0f;   // コーンの高さ（放出方向へのオフセット）

    YEmitterCone() = default;
    YEmitterCone(float outerAngleDeg, float h, const Vector3& dir = {0,1,0})
        : direction(dir), outerAngle(outerAngleDeg), height(h)
    {}

    Vector3 GeneratePoint() override;
    Type    GetType()       const override { return Type::Cone; }
    void    DrawEditor()    override;
};
