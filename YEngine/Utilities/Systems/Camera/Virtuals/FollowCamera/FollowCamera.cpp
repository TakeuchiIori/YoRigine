#include "FollowCamera.h"
#include "MathFunc.h"
#include <Systems/Input/Input.h>
#include <algorithm>
#include <cmath>

#include "CameraState.h"
#include "DefaultCameraState.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include <Systems/GameTime/GameTime.h>
#include <Systems/Camera/CameraDirector.h>
#include <Collision/Core/CollisionTypeIdDef.h>

// ============================================================
// 初期化
// ============================================================
void FollowCamera::Initialize() {
    VirtualCamera::Initialize();
    currentScale_ = 1.0f;
    baseFovY_ = fovY_;

    currentState_ = std::make_unique<DefaultCameraState>();
    currentState_->Enter(this);

    collisionResolver_.Initialize();
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayer));
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kNavObstacle));
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kStaticWall));
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield));
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kPlayerWeapon));
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy));
    collisionResolver_.AddIgnoreTypeID(static_cast<uint32_t>(CollisionTypeIdDef::kFieldEnemy));
}

// ============================================================
// 更新
// ============================================================
void FollowCamera::Update() {
    if (target_ == nullptr && !targetName_.empty()) {
        target_ = CameraDirector::GetInstance()->FindTarget(targetName_);
    }
    if (!target_) return;

    if (currentState_) {
        currentState_->Update(this);
        if (currentState_->IsFinished()) {
            ChangeState(std::make_unique<DefaultCameraState>());
        }
    }

    UpdateZoom();
}

// ============================================================
// スティック入力 (inputEnabled_ = false でスキップ)
// ============================================================
void FollowCamera::UpdateInput() {
    if (!inputEnabled_) return;
    if (isCloseUp_)     return;

    if (YoRigine::Input::GetInstance()->IsControllerConnected()) {
        XINPUT_STATE joyState;
        if (YoRigine::Input::GetInstance()->GetJoystickState(0, joyState)) {
            Vector3 move{};
            move.y += static_cast<float>(joyState.Gamepad.sThumbRX);
            move.x -= static_cast<float>(joyState.Gamepad.sThumbRY);

            if (Length(move) > 5000.0f) {
                move = Normalize(move) * kRotateSpeed_;
                transform_.rotate += move;
            }
            transform_.rotate.x = std::clamp(transform_.rotate.x, minPitch_, maxPitch_);
        }
    }
}

// ============================================================
// 追従位置の計算
// ============================================================
void FollowCamera::FollowProcess() {
    if (!target_) return;

    float targetScale = isCloseUp_ ? closeUpScale_ : 1.0f;
    currentScale_ += (targetScale - currentScale_)
        * std::clamp(interpSpeed_ * YoRigine::GameTime::GetDeltaTime(), 0.0f, 1.0f);

    Vector3 offset = offset_ * currentScale_;
    Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
    Vector3 rotatedOffset = TransformNormal(offset, rotateMat);

    Vector3 pivot   = target_->translate_ + Vector3(0.0f, targetPivot_Height_, 0.0f);
    Vector3 idealPos = pivot + rotatedOffset;

    Vector3 safePos = collisionResolver_.Resolve(idealPos, pivot);

    UpdateShake();
    transform_.translate = safePos + shakeOffset_;
}

// ============================================================
// ステート変更
// ============================================================
void FollowCamera::ChangeState(std::unique_ptr<CameraState> newState) {
    if (currentState_) currentState_->Exit(this);
    currentState_ = std::move(newState);
    if (currentState_) currentState_->Enter(this);
}

// ============================================================
// デフォルトパラメータ取得
// ============================================================
void FollowCamera::GetDefaultCameraParams(Vector3& outPos, Vector3& outRot, float& outFov) const {
    if (!target_) {
        outPos = transform_.translate;
        outRot = transform_.rotate;
        outFov = fovY_;
        return;
    }
    Vector3 offset = offset_ * currentScale_;
    Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
    offset = TransformNormal(offset, rotateMat);
    Vector3 pivot = target_->translate_ + Vector3(0.0f, targetPivot_Height_, 0.0f);
    outPos = pivot + offset;
    outRot = transform_.rotate;
    outFov = fovY_;
}

// ============================================================
// デバッグ GUI
// ============================================================
void FollowCamera::DrawDebugGui() {
#ifdef USE_IMGUI
    ImGui::Text("追従カメラ設定");
    ImGui::Separator();

    if (ImGui::TreeNode("追従パラメータ")) {
        ImGui::DragFloat3("オフセット",         &offset_.x,           0.1f, -100.0f, 100.0f);
        ImGui::DragFloat3("角度",               &transform_.rotate.x, 0.01f, -6.28f,  6.28f);
        ImGui::DragFloat("旋回速度(パッド)",    &kRotateSpeed_,        0.01f,  0.0f,   1.0f);
        ImGui::DragFloat("注視点の高さ",        &targetPivot_Height_,  0.1f,   0.0f, 100.0f);
        ImGui::DragFloat("見上げ限界",          &minPitch_,            0.01f, -1.5f,   0.0f);
        ImGui::DragFloat("見下ろし限界",        &maxPitch_,            0.01f,  0.0f,   1.5f);
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (ImGui::TreeNode("演出設定")) {
        ImGui::Checkbox("クローズアップ",       &isCloseUp_);
        ImGui::DragFloat("クローズアップ倍率",  &closeUpScale_, 0.01f, 0.1f,  1.0f);
        ImGui::DragFloat("補間速度",            &interpSpeed_,  0.1f,  0.1f, 20.0f);
        ImGui::TreePop();
    }
    ImGui::Separator();

    collisionResolver_.DrawDebugGui();
    ImGui::Separator();

    ImGui::Text("入力制御: %s", inputEnabled_ ? "有効" : "無効(外部管理)");
    ImGui::Text("ステート: %s", currentState_ ? currentState_->GetStateName() : "なし");
    ImGui::Text("追従対象: %s", target_ ? "セット済み" : "なし");
#endif
}

// ============================================================
// 保存
// ============================================================
void FollowCamera::Save(nlohmann::json& j) const {
    VirtualCamera::Save(j);
    j["targetName"]         = targetName_;
    j["offset"]             = { offset_.x, offset_.y, offset_.z };
    j["rotate"]             = { transform_.rotate.x, transform_.rotate.y, transform_.rotate.z };
    j["interpSpeed"]        = interpSpeed_;
    j["rotateSpeed"]        = kRotateSpeed_;
    j["closeUpScale"]       = closeUpScale_;
    j["targetPivot_Height"] = targetPivot_Height_;
    j["minPitch"]           = minPitch_;
    j["maxPitch"]           = maxPitch_;

    nlohmann::json resolverJson;
    collisionResolver_.Save(resolverJson);
    j["collisionResolver"] = resolverJson;

    // Game 拡張データをそのまま書き戻す（PlayerCamera が入れた battleStartState 等）
    if (!extensionJson_.empty()) {
        j["extension"] = extensionJson_;
    }
}

// ============================================================
// 読み込み
// ============================================================
void FollowCamera::Load(const nlohmann::json& j) {
    VirtualCamera::Load(j);
    targetName_ = j.value("targetName", "");
    if (j.contains("offset"))
        offset_ = { j["offset"][0], j["offset"][1], j["offset"][2] };
    if (j.contains("rotate"))
        transform_.rotate = { j["rotate"][0], j["rotate"][1], j["rotate"][2] };
    kRotateSpeed_        = j.value("rotateSpeed",        0.1f);
    interpSpeed_         = j.value("interpSpeed",        5.0f);
    closeUpScale_        = j.value("closeUpScale",       0.3f);
    targetPivot_Height_  = j.value("targetPivot_Height", 1.0f);
    minPitch_            = j.value("minPitch",          -0.2f);
    maxPitch_            = j.value("maxPitch",           1.2f);
    if (j.contains("collisionResolver"))
        collisionResolver_.Load(j["collisionResolver"]);

    // Game 拡張データを不透明のまま復元（PlayerCamera が後で解釈する）
    if (j.contains("extension"))
        extensionJson_ = j["extension"];

    // 旧バージョン互換：battleStartState が直接置かれていた場合も extension に吸収
    if (j.contains("battleStartState") && !j.contains("extension")) {
        extensionJson_["battleStartState"] = j["battleStartState"];
    }
}

// ============================================================
// シェイク開始
// ============================================================
void FollowCamera::StartShake(float intensity, float duration) {
    shakeIntensity_ = intensity;
    shakeDuration_  = duration;
    shakeTimer_     = 0.0f;
}

// ============================================================
// シェイク更新
// ============================================================
void FollowCamera::UpdateShake() {
    if (shakeTimer_ < shakeDuration_) {
        shakeTimer_ += YoRigine::GameTime::GetUnscaledDeltaTime();
        float decay = std::max(0.0f, 1.0f - shakeTimer_ / shakeDuration_);
        float rx = ((std::rand() % 2001) - 1000) / 1000.0f;
        float ry = ((std::rand() % 2001) - 1000) / 1000.0f;
        shakeOffset_ = { rx * shakeIntensity_ * decay, ry * shakeIntensity_ * decay, 0.0f };
    } else {
        shakeOffset_ = {};
    }
}

// ============================================================
// ズーム開始
// ============================================================
void FollowCamera::StartZoom(float targetFov, float duration) {
    baseFovY_      = fovY_;
    targetZoomFov_ = targetFov;
    zoomDuration_  = duration;
    zoomTimer_     = duration;
}

// ============================================================
// ズーム更新
// ============================================================
void FollowCamera::UpdateZoom() {
    if (zoomTimer_ > 0.0f) {
        zoomTimer_ -= YoRigine::GameTime::GetUnscaledDeltaTime();
        float t = std::clamp(1.0f - zoomTimer_ / zoomDuration_, 0.0f, 1.0f);
        fovY_ = targetZoomFov_ + (baseFovY_ - targetZoomFov_) * (t * t);
        if (zoomTimer_ <= 0.0f) fovY_ = baseFovY_;
    } else {
        fovY_ = baseFovY_;
    }
}
