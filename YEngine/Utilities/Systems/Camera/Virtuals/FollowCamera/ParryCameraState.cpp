#include "ParryCameraState.h"
#include "FollowCamera.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void ParryCameraState::Enter(FollowCamera* camera) {
    SetupControlPoints(camera);
    CinematicCameraState::Enter(camera);
}

void ParryCameraState::SetupControlPoints(FollowCamera* camera) {
    ClearControlPoints();
    
    if (!camera->GetTarget()) return;
    
    Vector3 targetPos = camera->GetTarget()->translate_;
    Vector3 currentPos = camera->GetTranslate();
    
    switch (parryType_) {
    case ParryType::Quick:
        // 素早く近づいて戻る
        SetReturnInterpTime(0.3f);
        SetLookAtTarget(true);
        
        // 制御点1: 素早く接近
        AddControlPoint({
            targetPos + Vector3{3.0f, 2.0f, -5.0f},
            {0.1f, 0.0f, 0.0f},
            0.5f,  // FOV
            0.2f   // 到達時間
        });
        
        // 制御点2: 横にパン
        AddControlPoint({
            targetPos + Vector3{-3.0f, 2.0f, -5.0f},
            {0.1f, 0.0f, 0.0f},
            0.5f,
            0.3f
        });
        break;
        
    case ParryType::Dramatic:
        // ドラマチックな回り込み
        SetReturnInterpTime(0.5f);
        SetLookAtTarget(true);
        
        // 制御点1: 上から見下ろす
        AddControlPoint({
            targetPos + Vector3{0.0f, 8.0f, -8.0f},
            {0.5f, 0.0f, 0.0f},
            0.4f,
            0.4f
        });
        
        // 制御点2: 回り込む
        AddControlPoint({
            targetPos + Vector3{6.0f, 3.0f, 0.0f},
            {0.2f, -1.57f, 0.0f},
            0.45f,
            0.5f
        });
        
        // 制御点3: 正面へ
        AddControlPoint({
            targetPos + Vector3{0.0f, 3.0f, -10.0f},
            {0.1f, 0.0f, 0.0f},
            0.45f,
            0.4f
        });
        break;
        
    case ParryType::SlowMotion:
        // ゆっくりズームイン
        SetReturnInterpTime(0.6f);
        SetLookAtTarget(true);
        
        // 制御点1: ゆっくり近づく
        AddControlPoint({
            targetPos + Vector3{2.0f, 2.0f, -6.0f},
            {0.0f, 0.0f, 0.0f},
            0.35f,
            0.8f
        });
        
        // 制御点2: さらに接近
        AddControlPoint({
            targetPos + Vector3{1.0f, 1.5f, -3.0f},
            {0.0f, 0.0f, 0.0f},
            0.3f,
            0.6f
        });
        break;
    }
}

void ParryCameraState::Save(nlohmann::json& j) const {
    CinematicCameraState::Save(j);
    j["parryType"] = static_cast<int>(parryType_);
}

void ParryCameraState::Load(const nlohmann::json& j) {
    CinematicCameraState::Load(j);
    parryType_ = static_cast<ParryType>(j.value("parryType", 0));
}

void ParryCameraState::DrawEditGui() {
#ifdef USE_IMGUI
    ImGui::Text("パリィカメラ設定");
    ImGui::Separator();
    
    // パリィタイプの選択
    const char* typeNames[] = { "Quick", "Dramatic", "SlowMotion" };
    int currentType = static_cast<int>(parryType_);
    if (ImGui::Combo("パリィタイプ", &currentType, typeNames, 3)) {
        parryType_ = static_cast<ParryType>(currentType);
    }
    
    if (ImGui::Button("制御点を再生成")) {
        // ダミーのカメラを使って制御点を再生成
        // 実際の使用時は現在のカメラを渡す
        SetupControlPoints(nullptr);
    }
    
    ImGui::Separator();
    
    // 基底クラスの編集UIを表示
    CinematicCameraState::DrawEditGui();
#endif
}
