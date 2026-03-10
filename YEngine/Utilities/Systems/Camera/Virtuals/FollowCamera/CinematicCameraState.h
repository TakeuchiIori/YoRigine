#pragma once
#include "CameraState.h"
#include <vector>

/// <summary>
/// 制御点構造体
/// </summary>
struct CameraControlPoint {
    Vector3 position;      // カメラ位置
    Vector3 rotation;      // カメラ回転
    float fov;             // 視野角
    float arrivalTime;     // この制御点に到達するまでの時間（秒）
    
    CameraControlPoint(
        const Vector3& pos = {0,0,0}, 
        const Vector3& rot = {0,0,0}, 
        float f = 0.45f, 
        float time = 1.0f)
        : position(pos), rotation(rot), fov(f), arrivalTime(time) {}
};

/// <summary>
/// 制御点を使ったカメラワークステート
/// </summary>
class CinematicCameraState : public CameraState {
public:
    void Enter(FollowCamera* camera) override;
    void Update(FollowCamera* camera) override;
    void Exit(FollowCamera* camera) override;
    
    bool IsFinished() const override { return isFinished_; }
    const char* GetStateName() const override { return "Cinematic"; }
    
    // 制御点の設定
    void AddControlPoint(const CameraControlPoint& point);
    void ClearControlPoints();
    
    // デフォルトステートに戻る際の補間時間を設定
    void SetReturnInterpTime(float time) { returnInterpTime_ = time; }
    
    // ターゲットを見るかどうか
    void SetLookAtTarget(bool enable) { lookAtTarget_ = enable; }
    
    // 保存・読込
    void Save(nlohmann::json& j) const override;
    void Load(const nlohmann::json& j) override;
    
    // ImGuiでの編集
    void DrawEditGui() override;

    bool IsPerformance() const override { return true; }
    
    // 制御点の取得（編集用）
    std::vector<CameraControlPoint>& GetControlPoints() { return controlPoints_; }
    const std::vector<CameraControlPoint>& GetControlPoints() const { return controlPoints_; }
    
protected:
    // 現在の制御点間を補間
    void InterpolateControlPoints(FollowCamera* camera);
    
    std::vector<CameraControlPoint> controlPoints_;
    int currentPointIndex_ = 0;
    float pointTimer_ = 0.0f;
    bool isFinished_ = false;
    
    // デフォルトに戻る際の補間
    float returnInterpTime_ = 0.5f;
    bool isReturning_ = false;
    float returnTimer_ = 0.0f;
    Vector3 returnStartPos_;
    Vector3 returnStartRot_;
    float returnStartFov_;
    
    // ターゲットを見続けるか
    bool lookAtTarget_ = false;
};
