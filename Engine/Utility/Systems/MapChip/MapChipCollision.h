#pragma once

#include "MapChipField.h"
#include "Vector3.h"
#include <functional>
#include <vector>
#include <stdint.h>


enum CollisionDirection {
    NoneDir = 0,
    LeftDir = 1,
    RightDir = 2,
    TopDir = 3,
    BottomDir = 4
};

struct CollisionInfo {
    // 衝突したブロックのインデックス
    uint32_t xIndex = 0;
    uint32_t yIndex = 0;
    // 衝突したブロックのタイプ
    MapChipType blockType = MapChipType::kBlank;
    // 衝突方向 (0:None, 1:左, 2:右, 3:上, 4:下)
    CollisionDirection direction;
    // 衝突の深さ（めり込み量）
    float penetrationDepth = 0.0f;
    // 衝突したブロックの矩形
    MapChipField::Rect blockRect;
};



class MapChipCollision{
public:
    // 移動オブジェクト用の矩形を表す構造体
    struct ColliderRect {
        float width;
        float height;
        // オフセット（中心からの相対位置）
        float offsetX;
        float offsetY;

        // デフォルトコンストラクタ
        ColliderRect(float w = 1.0f, float h = 1.0f, float ox = 0.0f, float oy = 0.0f)
            : width(w), height(h), offsetX(ox), offsetY(oy) {
        }
    };

    // 衝突判定フラグ（ビットフラグとして使用）
    enum CollisionFlag {
        None = 0,
        Left = 1 << 0,
        Right = 1 << 1,
        Top = 1 << 2,
        Bottom = 1 << 3,
        All = Left | Right | Top | Bottom
    };

    // コンストラクタ
    MapChipCollision(MapChipField& mapChipField) : mapChipField_(mapChipField) {}

    // 指定した位置と速度をもとに衝突判定と解決を行う
    // colliderRect: 衝突判定に使用する矩形
    // position: 現在位置（参照渡しで修正可能）
    // velocity: 現在速度（参照渡しで修正可能）
    // checkFlags: チェックする方向のフラグ
    // collisionCallback: 衝突時に呼び出されるコールバック関数
    void DetectAndResolveCollision(
        const ColliderRect& colliderRect,
        Vector3& position,
        Vector3& velocity,
        int checkFlags = CollisionFlag::All,
        std::function<void(const CollisionInfo&)> collisionCallback = nullptr);


private:
    MapChipField& mapChipField_;
};