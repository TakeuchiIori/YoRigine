#include "BattleStartCameraState.h"
#include "FollowCamera.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void BattleStartCameraState::Enter(FollowCamera* camera) {
    SetupControlPoints(camera);
    CinematicCameraState::Enter(camera);
}

void BattleStartCameraState::SetupControlPoints(FollowCamera* camera) {
    ClearControlPoints();
    
    if (!camera->GetTarget()) return;
    
    Vector3 targetPos = camera->GetTarget()->translate_;
    
    SetReturnInterpTime(0.5f);
    SetLookAtTarget(true);
    
    // 制御点1: 遠くから俯瞰
    AddControlPoint({
        targetPos + Vector3{0.0f, 15.0f, -20.0f},
        {0.6f, 0.0f, 0.0f},
        0.5f,
        0.6f
    });
    
    // 制御点2: 横から回り込む
    AddControlPoint({
        targetPos + Vector3{10.0f, 5.0f, -5.0f},
        {0.2f, -0.8f, 0.0f},
        0.45f,
        0.5f
    });
    
    // 制御点3: プレイヤーの後ろへ移動
    AddControlPoint({
        targetPos + Vector3{-2.0f, 4.0f, -12.0f},
        {0.1f, 0.0f, 0.0f},
        0.45f,
        0.6f
    });
}

void BattleStartCameraState::DrawEditGui() {
#ifdef USE_IMGUI
    ImGui::Text("戦闘開始カメラ設定");
    ImGui::Separator();
    
    if (ImGui::Button("制御点を再生成")) {
        SetupControlPoints(nullptr);
    }
    
    ImGui::Separator();
    
    // 基底クラスの編集UIを表示
    CinematicCameraState::DrawEditGui();
#endif
}
