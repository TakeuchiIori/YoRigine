#pragma once
#include <Vector3.h>
#include <WorldTransform/WorldTransform.h>
#include "Loaders/Json/JsonManager.h"

class FollowCamera
{
public:

    void Initialize();
    void Update();

    void UpdateInput();
	void FollowProsess();
    
    void InitJson();
	void JsonImGui();

    Vector3 translate_ = { 0,0,0 };
    Vector3 scale_ = { 1,1,1 };
    Vector3 rotate_ = { 0,0,0 };

    Matrix4x4 matView_ = {};

    void SetTarget(const WorldTransform& target) { target_ = &target; }

private:

    std::unique_ptr <JsonManager> jsonManager_;
    void ImGui();
    Vector3 rotation_;
    float kDeadZoneL_ = 100.0f;
    // 追従対象
    const WorldTransform* target_;
    Vector3 offset_ = { 0.0f, 6.0f, -40.0f };
};

