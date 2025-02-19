#pragma once
#include <Vector3.h>
#include <WorldTransform/WorldTransform.h>

class TopDownCamera
{
public:

    void Initialize();
    void Update();

    void TopDownProsess();

    Vector3 translate_ = { 0,0,0 };
    Vector3 scale_ = { 1,1,1 };
    Vector3 rotate_ = { 0,0,0 };

    Matrix4x4 matView_ = {};

    void SetTarget(const WorldTransform& target) { target_ = &target; }

private:

    void ImGui();

    // 真上から見ている
    Vector3 offset_ = { 0.0f, 50.0f, 0.0f };
    // 追従対象
    const WorldTransform* target_;
};
