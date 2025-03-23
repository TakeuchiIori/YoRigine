#pragma once
#include <Vector3.h>
#include <WorldTransform/WorldTransform.h>
#include "Loaders/Json/JsonManager.h"

class DebugCamera
{
public:
    void Initialize();
    void Update();
    void UpdateInput();

    Vector3 translate_ = { 0, 0, -40.0f };
    Vector3 scale_ = { 1, 1, 1 };
    Vector3 rotate_ = { 0, 0, 0 };

    Matrix4x4 matView_ = {};
    void SetPosition(const Vector3& position) { translate_ = position; }
    Vector3 GetPosition() const { return translate_; }
	float GetFov() const { return (fov_ >= 110.0f) ? fov_ : fov_; }

private:
    std::unique_ptr<JsonManager> jsonManager_;
    void ImGui();
    void InitJson();
    void JsonImGui();

    Vector2 prevMousePos_ = { 0.0f, 0.0f };
    bool isDragging_ = false;
    float rotateSpeed_ = 0.05f;
    float rotateSpeedController_ = 0.005f;
    float moveSpeed_ = 0.5f;
	float moveSpeedController_ = 0.1f;
    float fov_ = 0.90f;

};