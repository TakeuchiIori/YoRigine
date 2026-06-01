#include "PlayerCamera.h"

#include "Systems/GameTime/GameTime.h"
#include "Systems/Input/Input.h"
#include "Systems/Camera/Camera.h"
#include "Collision/Core/CollisionManager.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "MathFunc.h"

// BattleStartCameraState は Debug/Release 両方で必要
#include "Systems/Camera/Virtuals/FollowCamera/BattleStartCameraState.h"
#include "Systems/Camera/Virtuals/FollowCamera/DefaultCameraState.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>
#include <limits>
#include <filesystem>

// ============================================================
// 初期化
// ============================================================
void PlayerCamera::Initialize(FollowCamera* followCamera, const WorldTransform* playerWT) {
    followCamera_ = followCamera;
    playerWT_     = playerWT;

    if (!followCamera_) return;

    // FollowCamera の内部スティック入力を無効化し、PlayerCamera が一元管理する
    followCamera_->SetInputEnabled(false);

    // FollowCamera からパラメータを同期
    minPitch_    = followCamera_->GetMinPitch();
    maxPitch_    = followCamera_->GetMaxPitch();
    rotateSpeed_ = followCamera_->GetRotateSpeed();
    pivotHeight_ = followCamera_->GetTargetPivotHeight();

    // 攻撃カメラワーク初期化
    attackCamera_.Initialize();

    // デフォルトのカメラワークファイルを読み込む（あれば）
    const std::string defaultPath = "Resources/Json/Cameras/AttackCameraWorks.json";
    if (std::filesystem::exists(defaultPath)) {
        attackCamera_.LoadFromFile(defaultPath);
    }

    // BattleStartState を初期化し、FollowCamera の extension JSON から設定を復元
    battleStartState_ = std::make_shared<BattleStartCameraState>();
    const auto& ext = followCamera_->GetExtensionJson();
    if (ext.contains("battleStartState")) {
        battleStartState_->Load(ext["battleStartState"]);
    }

#ifdef USE_IMGUI
    editor_.Initialize(&attackCamera_, this);
    editor_.SetFilePath(defaultPath);
#endif
}

// ============================================================
// フェーズ1: スティック入力・ロックオン更新
// CameraDirector::Update() の前に GameScene::Update() から呼ぶ。
// FollowCamera の回転を確定させることで、後続の FollowProcess() が
// 正しい位置を計算できるようにする。
// ============================================================
void PlayerCamera::UpdatePreDirector() {
    if (!followCamera_) return;

    float dt = YoRigine::GameTime::GetDeltaTime();
    bool  inPerformance = IsInPerformance();

    if (!inPerformance) {
        // R3 でロックオン切り替え（isLockOn_ の状態に関わらず常にチェックする）
        if (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::R_Stick)) {
            isLockOn_ = !isLockOn_;
            if (isLockOn_) {
                lockedTarget_ = nullptr;
                SwitchLockOnTarget(0);
                if (!lockedTarget_) isLockOn_ = false;
            } else {
                lockedTarget_ = nullptr;
            }
        }

        if (isLockOn_) {
            UpdateLockOn();
        } else {
            UpdateStickInput();
        }

        // 攻撃カメラのシェイクトリガーも先に処理（次フレームの FollowProcess で適用）
        attackCamera_.UpdatePre(followCamera_, dt);
    }
}

// ============================================================
// フェーズ2: 攻撃カメラオフセットを sceneCamera に後付けで適用
// CameraDirector::Update() + UpdateCamera() の後に呼ぶ。
// CameraDirector が sceneCamera に転送した後なので、
// ここで加算すると確実に描画に反映される。
// ============================================================
void PlayerCamera::ApplyPostDirector(Camera* sceneCamera, float dt) {
    if (!sceneCamera) return;
    bool inPerformance = IsInPerformance();
    if (inPerformance) return;

    // キーフレーム補間値を更新
    attackCamera_.UpdatePost(dt);

    if (!attackCamera_.IsActive()) {
        // 演出が完全に終了したフレームでタイムスケールをリセット
        YoRigine::GameTime::SetTimeScale(1.0f);
        return;
    }

    // ---- 位置オフセット（カメラローカル空間 → ワールド空間に変換して加算） ----
    Vector3 posOff = attackCamera_.GetCurrentPosOffset();
    if (posOff.x != 0.0f || posOff.y != 0.0f || posOff.z != 0.0f) {
        Matrix4x4 rotMat = MakeRotateMatrixXYZ(sceneCamera->transform_.rotate);
        Vector3   worldOff = TransformNormal(posOff, rotMat);
        sceneCamera->transform_.translate = sceneCamera->transform_.translate + worldOff;
    }

    // ---- 回転オフセット（pitch, yaw, roll 追加） ----
    Vector3 rotOff = attackCamera_.GetCurrentRotOffset();
    if (rotOff.x != 0.0f || rotOff.y != 0.0f || rotOff.z != 0.0f) {
        sceneCamera->transform_.rotate = sceneCamera->transform_.rotate + rotOff;
    }

    // ---- FOV オフセット ----
    float fovDelta = attackCamera_.GetCurrentFovDelta();
    if (fovDelta != 0.0f) {
        // savedBaseFov_ は AttackCameraComponent 側が保持している
        sceneCamera->fovY_ = attackCamera_.GetSavedBaseFov() + fovDelta;
    }

    // ---- タイムスケール ----
    float ts = attackCamera_.GetCurrentTimeScale();
    if (ts != 1.0f) {
        YoRigine::GameTime::SetTimeScale(ts);
    } else {
        YoRigine::GameTime::SetTimeScale(1.0f);
    }

    // オフセット適用後に行列を再計算
    sceneCamera->UpdateMatrix();
}


// ============================================================
// スティック入力で FollowCamera の回転を更新
// ============================================================
void PlayerCamera::UpdateStickInput() {
    if (!followCamera_) return;
    if (followCamera_->GetIsCloseUp()) return;

    if (YoRigine::Input::GetInstance()->IsControllerConnected()) {
        XINPUT_STATE js;
        if (YoRigine::Input::GetInstance()->GetJoystickState(0, js)) {
            Vector3 move{};
            move.y += static_cast<float>(js.Gamepad.sThumbRX);
            move.x -= static_cast<float>(js.Gamepad.sThumbRY);

            if (Length(move) > 5000.0f) {
                move = Normalize(move) * rotateSpeed_;
                Vector3 rot = followCamera_->GetRotate();
                rot += move;
                rot.x = std::clamp(rot.x, minPitch_, maxPitch_);
                followCamera_->SetRotate(rot);
            }
        }
    }
}

// ============================================================
// ロックオン更新
// ============================================================
void PlayerCamera::UpdateLockOn() {
    if (!playerWT_ || !followCamera_) return;

    auto* input = YoRigine::Input::GetInstance();

    if (!isLockOn_ || !lockedTarget_) {
        isLockOn_     = false;
        lockedTarget_ = nullptr;
        return;
    }

    // ターゲット生存確認
    bool alive = false;
    for (auto* col : YoRigine::CollisionManager::GetInstance()->GetColliders()) {
        if (col == lockedTarget_ && col->GetIsActive()) { alive = true; break; }
    }
    if (!alive) {
        lockedTarget_ = nullptr;
        SwitchLockOnTarget(0);
        if (!lockedTarget_) { isLockOn_ = false; return; }
    }

    // スティック左右で切り替え
    if (lockOnSwitchCooldown_ > 0.0f) {
        lockOnSwitchCooldown_ -= YoRigine::GameTime::GetUnscaledDeltaTime();
    } else {
        float rx = input->GetRightStickX(0);
        if (std::abs(rx) > 0.6f) {
            SwitchLockOnTarget(rx > 0.0f ? 1 : -1);
            lockOnSwitchCooldown_ = 0.3f;
        }
    }

    // カメラをターゲット方向に向ける
    Vector3 pivot    = playerWT_->translate_ + Vector3(0.0f, pivotHeight_, 0.0f);
    Vector3 enemyPos = lockedTarget_->GetCenterPosition();
    Vector3 dir      = Normalize(enemyPos - pivot);

    float targetYaw   = atan2f(dir.x, dir.z);
    float targetPitch = std::clamp(asinf(-dir.y) + 0.15f, minPitch_, maxPitch_);

    float t = std::clamp(10.0f * YoRigine::GameTime::GetUnscaledDeltaTime(), 0.0f, 1.0f);
    Vector3 rot = followCamera_->GetRotate();

    float diffY = targetYaw - rot.y;
    while (diffY <= -3.14159265f) diffY += 6.2831853f;
    while (diffY >   3.14159265f) diffY -= 6.2831853f;
    rot.y += diffY * t;
    while (rot.y <= -3.14159265f) rot.y += 6.2831853f;
    while (rot.y >   3.14159265f) rot.y -= 6.2831853f;

    rot.x = Lerp(rot.x, targetPitch, t);
    followCamera_->SetRotate(rot);
}

// ============================================================
// ロックオンターゲット切り替え
// ============================================================
void PlayerCamera::SwitchLockOnTarget(int direction) {
    if (!playerWT_) return;

    std::vector<BaseCollider*> enemies;
    for (auto* col : YoRigine::CollisionManager::GetInstance()->GetColliders()) {
        if (col->GetTypeID() == static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy)
            && col->GetIsActive()) {
            enemies.push_back(col);
        }
    }
    if (enemies.empty()) { lockedTarget_ = nullptr; return; }

    Vector3 playerPos = playerWT_->translate_;

    if (direction == 0 || !lockedTarget_) {
        BaseCollider* closest = nullptr;
        float minDist = (std::numeric_limits<float>::max)();
        for (auto* e : enemies) {
            float d = Length(e->GetCenterPosition() - playerPos);
            if (d < minDist) { minDist = d; closest = e; }
        }
        lockedTarget_ = closest;
        return;
    }

    // 左右切り替え
    Vector3 toCurrent = Normalize(lockedTarget_->GetCenterPosition() - playerPos);
    float   curYaw    = atan2f(toCurrent.x, toCurrent.z);

    BaseCollider* best     = nullptr;
    float         minAngle = (std::numeric_limits<float>::max)();

    for (auto* e : enemies) {
        if (e == lockedTarget_) continue;
        Vector3 toE  = Normalize(e->GetCenterPosition() - playerPos);
        float   eYaw = atan2f(toE.x, toE.z);
        float   diff = eYaw - curYaw;
        while (diff <= -3.14159265f) diff += 6.2831853f;
        while (diff >   3.14159265f) diff -= 6.2831853f;

        if (direction == 1 && diff > 0.05f && diff < minAngle) {
            minAngle = diff; best = e;
        } else if (direction == -1 && diff < -0.05f && std::abs(diff) < minAngle) {
            minAngle = std::abs(diff); best = e;
        }
    }
    if (best) lockedTarget_ = best;
}

// ============================================================
// 攻撃カメラワーク開始
// ============================================================
void PlayerCamera::PlayAttackCameraWork(const std::string& attackName) {
    if (!followCamera_) return;

    // 再生前 FOV を保存 → Play → savedBaseFov に渡す順で呼ぶ
    const float baseFov = followCamera_->GetBaseFovY();
    attackCamera_.Play(attackName);
    attackCamera_.SetSavedBaseFov(baseFov);
}

// ============================================================
// 攻撃カメラワーク停止
// ============================================================
void PlayerCamera::StopAttackCameraWork() {
    attackCamera_.Stop(followCamera_);
    YoRigine::GameTime::SetTimeScale(1.0f);
}

// ============================================================
// 戦闘開始演出
// ============================================================
void PlayerCamera::PlayBattleStart() {
    if (!followCamera_) return;

    if (!battleStartState_) {
        battleStartState_ = std::make_shared<BattleStartCameraState>();
    }

    // battleStartState_ の現在設定をコピーして再生用インスタンスを作る
    auto play = std::make_unique<BattleStartCameraState>();
    nlohmann::json j;
    battleStartState_->Save(j);
    play->Load(j);
    play->RebuildControlPoints(followCamera_);
    followCamera_->ChangeState(std::move(play));
    isPreviewMode_ = false; // 実ゲームからの呼び出しなのでプレビューフラグは立てない
}

// ============================================================
// 演出中判定
// ============================================================
bool PlayerCamera::IsInPerformance() const {
    return followCamera_ && followCamera_->IsInPerformance();
}

// ============================================================
// ImGui 描画
// ============================================================
void PlayerCamera::DrawImGui() {
#ifdef USE_IMGUI
    if (!followCamera_) return;

    if (ImGui::CollapsingHeader("追従カメラ設定")) {
        followCamera_->DrawDebugGui();
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("攻撃カメラワーク エディター")) {
        editor_.DrawImGui();
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("戦闘開始カメラ演出")) {
        if (!battleStartState_) {
            battleStartState_ = std::make_shared<BattleStartCameraState>();
        }

        battleStartState_->DrawEditGui();

        // 設定変更時は FollowCamera の extensionJson にも書き込む
        // （カメラプリセット保存時に一緒に永続化される）
        {
            nlohmann::json bsJson;
            battleStartState_->Save(bsJson);
            nlohmann::json ext = followCamera_->GetExtensionJson();
            ext["battleStartState"] = bsJson;
            followCamera_->SetExtensionJson(ext);
        }

        ImGui::Separator();

        if (!isPreviewMode_) {
            if (ImGui::Button("プレビュー再生")) {
                // プレビューは isPreviewMode_ = true で区別する
                auto play = std::make_unique<BattleStartCameraState>();
                nlohmann::json j;
                battleStartState_->Save(j);
                play->Load(j);
                play->RebuildControlPoints(followCamera_);
                followCamera_->ChangeState(std::move(play));
                isPreviewMode_ = true;
            }
        } else {
            ImGui::TextColored({ 0.3f, 1.0f, 0.3f, 1.0f }, "再生中");
            ImGui::SameLine();
            if (ImGui::Button("停止")) {
                followCamera_->ChangeState(std::make_unique<DefaultCameraState>());
                isPreviewMode_ = false;
            }
            if (!followCamera_->IsInPerformance()) {
                isPreviewMode_ = false;
            }
        }
    }

    ImGui::Separator();

    // ロックオン状態
    ImGui::Text("ロックオン: %s", isLockOn_ ? "ON" : "OFF");
    if (lockedTarget_) {
        ImGui::Text("ターゲット: %p", static_cast<void*>(lockedTarget_));
    }
#endif
}
