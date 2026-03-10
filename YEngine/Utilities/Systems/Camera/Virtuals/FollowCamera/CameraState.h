#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"
#include "WorldTransform/WorldTransform.h"
#include "json.hpp"

class FollowCamera;

/// <summary>
/// カメラステートの基底クラス
/// </summary>
class CameraState {
public:
    virtual ~CameraState() = default;
    
    // 各ステートで実装する関数
    virtual void Enter(FollowCamera* camera) = 0;  // ステート開始時
    virtual void Update(FollowCamera* camera) = 0; // 更新処理
    virtual void Exit(FollowCamera* camera) = 0;   // ステート終了時
    
    // ステートが終了したかどうか
    virtual bool IsFinished() const { return false; }
    
    // デバッグ用の名前
    virtual const char* GetStateName() const = 0;
    
    // 保存・読込
    virtual void Save([[maybe_unused]] nlohmann::json& j) const {}
    virtual void Load([[maybe_unused]] const nlohmann::json& j) {}
    virtual bool IsPerformance() const { return false; }
    // ImGuiでの編集
    virtual void DrawEditGui() {}

protected:
    float stateTimer_ = 0.0f;  // ステート内の経過時間
};
