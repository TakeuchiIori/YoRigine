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
    // カメラのめり込み防止は「許可リスト方式」。
    // Initialize() 内で既定の遮蔽対象（壁 kStaticWall / NavObstacle）がセットされる。
    // プレイヤー・武器・盾・敵・魔法・トリガー領域などは登録されないので素通りする。
    // 遮る対象を変えたい場合は JSON(collisionResolver.blockTypeIDs) かエディタから設定する。
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
// クローズアップ切り替え
// 死亡演出への切り替え時に、通常追従の慣性や揺れを持ち込まない。
// ============================================================
void FollowCamera::SetIsCloseUp(bool v) {
    if (isCloseUp_ == v) return;

    isCloseUp_ = v;
    if (isCloseUp_) {
        CancelRecenter(true);
        smoothedLookAhead_ = {};
        pivotVel_ = {};
        shakeOffset_ = {};
        shakeTimer_ = shakeDuration_;
        idleRecenterTimer_ = 0.0f;
    }
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
                // スティック操作中はアイドルオートリセンターのタイマーをリセット
                NotifyCameraActive();
            }
            transform_.rotate.x = std::clamp(transform_.rotate.x, minPitch_, maxPitch_);
        }
    }
}

// ============================================================
// 臨界減衰スプリング（Unity SmoothDamp 相当）
//   current → target へオーバーシュートせず滑らかに寄せる。
//   vel は呼び出し側が保持する速度アキュムレータ（参照で更新）。
// ============================================================
static float SmoothDampScalar(float current, float target, float& vel,
    float smoothTime, float dt, float maxSpeed) {
    smoothTime = std::max(0.0001f, smoothTime);
    const float omega = 2.0f / smoothTime;
    const float x = omega * dt;
    const float expo = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

    float change = current - target;
    const float originalTo = target;

    const float maxChange = maxSpeed * smoothTime;
    change = std::clamp(change, -maxChange, maxChange);
    target = current - change;

    const float temp = (vel + omega * change) * dt;
    vel = (vel - omega * temp) * expo;
    float output = target + (change + temp) * expo;

    // オーバーシュート防止
    if ((originalTo - current > 0.0f) == (output > originalTo)) {
        output = originalTo;
        vel = (output - originalTo) / dt;
    }
    return output;
}

static Vector3 SmoothDampVec3(const Vector3& current, const Vector3& target, Vector3& vel,
    float smoothTime, float dt, float maxSpeed) {
    return {
        SmoothDampScalar(current.x, target.x, vel.x, smoothTime, dt, maxSpeed),
        SmoothDampScalar(current.y, target.y, vel.y, smoothTime, dt, maxSpeed),
        SmoothDampScalar(current.z, target.z, vel.z, smoothTime, dt, maxSpeed),
    };
}

// ============================================================
// 追従位置の計算
// ============================================================
void FollowCamera::FollowProcess() {
    if (!target_) return;

    // クローズアップは死亡モーション終了後にゲーム時間が止まっても、
    // 最終位置まで確実に収束させる。
    const float dt = isCloseUp_
        ? YoRigine::GameTime::GetUnscaledDeltaTime()
        : YoRigine::GameTime::GetDeltaTime();

    // ── ヘディング追従：カメラ yaw を対象 facing へ毎フレーム寄せる ──
    //   相対操舵（差分入力）の後方視点用。入力はカメラ向きを参照しないため
    //   ここでカメラをいくら滑らかに回してもプレイヤーの向きはズレない。
    if (yawFollowsTarget_ && target_ && !isCloseUp_) {
        constexpr float kPi    = 3.14159265358979f;
        constexpr float kTwoPi = 6.28318530717958f;
        float delta = target_->rotate_.y - transform_.rotate.y;
        while (delta >  kPi) delta -= kTwoPi;
        while (delta < -kPi) delta += kTwoPi;
        const float targetYaw = transform_.rotate.y + delta; // 最短角の目標
        transform_.rotate.y = SmoothDampScalar(
            transform_.rotate.y, targetYaw, yawFollowVel_,
            yawFollowSmoothTime_, dt, 1000.0f);
    }

    // ── リセンター（対象 facing 背後へ寄せる）を先に進める ──
    if (!isCloseUp_) UpdateRecenter(dt);

    // ── クローズアップ倍率の補間 ──
    float targetScale = isCloseUp_ ? closeUpScale_ : 1.0f;
    currentScale_ += (targetScale - currentScale_)
        * std::clamp(interpSpeed_ * dt, 0.0f, 1.0f);

    // ── ターゲット速度から先読みオフセットを算出 ──
    const Vector3 targetPos = target_->translate_;
    bool teleported = false;
    Vector3 targetVel{};
    if (hasPrevTargetPos_ && dt > 1e-6f) {
        Vector3 delta = targetPos - prevTargetPos_;
        // 1フレームで followSnapDistance_ 以上動いたらテレポート扱い（速度は無効化）
        if (Length(delta) > followSnapDistance_) {
            teleported = true;
        } else {
            targetVel = delta / dt;
        }
    }
    prevTargetPos_ = targetPos;
    hasPrevTargetPos_ = true;

    Vector3 desiredLookAhead{};
    if (!isCloseUp_ && lookAheadEnabled_ && !teleported) {
        Vector3 flatVel = { targetVel.x,
                            lookAheadVertical_ ? targetVel.y : 0.0f,
                            targetVel.z };
        desiredLookAhead = flatVel * lookAheadTime_;
        float la = Length(desiredLookAhead);
        if (la > lookAheadMaxDist_ && la > 1e-6f) {
            desiredLookAhead = desiredLookAhead / la * lookAheadMaxDist_;
        }
    }
    // 先読みは急変させずイーズイン/アウト
    smoothedLookAhead_ += (desiredLookAhead - smoothedLookAhead_)
        * std::clamp(lookAheadSmooth_ * dt, 0.0f, 1.0f);

    // ── 注視点（先読み込み）と理想カメラ位置 ──
    Vector3 pivot = targetPos + Vector3(0.0f, targetPivot_Height_, 0.0f) + smoothedLookAhead_;

    // ── 位置ダンピング（臨界減衰スプリング）は「対象の追従」にだけ掛ける ──
    //   pivot（＝キャラの位置）だけを SmoothDamp し、スティック回転による
    //   オフセット(rotatedOffset)はダンピング後に毎フレーム生で足す。
    //   こうしないとスティックでカメラを振った瞬間の回転までスプリング
    //   越しになり、右スティック操作が「フワッと遅れる」ように感じてしまう。
    if (!followInitialized_) {
        smoothedPivot_ = pivot;
        pivotVel_ = {};
        followInitialized_ = true;
    } else if (teleported || !positionSmoothing_
        || Length(pivot - smoothedPivot_) > followSnapDistance_) {
        // テレポート / スムージング無効 / 大きくズレた場合は瞬間スナップ
        smoothedPivot_ = pivot;
        pivotVel_ = {};
    } else {
        smoothedPivot_ = SmoothDampVec3(smoothedPivot_, pivot, pivotVel_,
            positionSmoothTime_, dt, maxFollowSpeed_);
    }

    Vector3 offset = offset_ * currentScale_;
    Matrix4x4 rotateMat = MakeRotateMatrixXYZ(transform_.rotate);
    Vector3 rotatedOffset = TransformNormal(offset, rotateMat);
    Vector3 idealPos = smoothedPivot_ + rotatedOffset;  // 追従は平滑化済み、回転は生の値

    // ── 壁めり込み回避は最後にハード補正 ──
    // 近接演出ではカメラ距離に合わせて壁との余白も縮める。
    // また、Y 座標だけを大きく押し上げるハイアングル補正は死亡時の
    // フレーミング崩れの原因になるためクローズアップ中は無効化する。
    Vector3 safePos = collisionResolver_.Resolve(
        idealPos, pivot, dt, currentScale_, !isCloseUp_);

    // アイドル時オートリセンター：カメラ操作が一定時間なければ静かに背後へ戻す
    if (!isCloseUp_) UpdateIdleRecenter(dt);

    UpdateShake();
    transform_.translate = safePos + shakeOffset_;

    // フレーミング補正：追従対象が画角外に出そうな時だけ rotation を引き戻す
    if (!isCloseUp_) EnsureTargetInView(pivot, dt);
}

// ============================================================
// アイドル時オートリセンター更新
//
//   移動方向を追いかける旧オート Yaw とは異なり、「カメラ操作が一定時間
//   なかったか」だけを見る。PlayerCamera 側でスティック操作やロックオン
//   照準など能動的な制御があったフレームでは NotifyCameraActive() が
//   呼ばれ idleRecenterTimer_ が 0 に戻るため、操作している間は絶対に
//   介入しない。無操作が idleRecenterDelay_ 秒続いたときだけ、既存の
//   RecenterToYaw（RB/LB の手動リセンターと同じイーズ処理）を１回だけ
//   起動して対象の背後へ静かに寄せる。
// ============================================================
void FollowCamera::UpdateIdleRecenter(float dt) {
    if (yawFollowsTarget_)        return;  // ヘディング追従中は yaw を常時制御しているので不要
    if (!idleRecenterEnabled_)    return;
    if (idleRecenterSuppressed_)  return;  // Game 側からの一時停止中（例：バトル中）
    if (recentering_)             return;  // 手動リセンター等が既に進行中なら何もしない
    if (IsInPerformance())        return;
    if (!target_)                 return;

    idleRecenterTimer_ += dt;
    if (idleRecenterTimer_ < idleRecenterDelay_) return;

    // 対象の向いている方向の背後へ、手動リセンターより緩やかなイーズで寄せる。
    // RecenterToYaw 内部で idleRecenterTimer_ は 0 にリセットされる。
    RecenterToYaw(target_->rotate_.y, recenterResetPitch_, idleRecenterDuration_);
}

// ============================================================
// フレーミング補正
// pivot がカメラ視界マージン外（framing*Margin_）に出そうなときだけ、
// はみ出した分を framingSpeed_ * dt 上限で rotation に加算して引き戻す。
// 演出中（IsInPerformance）は介入しない。
// ============================================================
void FollowCamera::EnsureTargetInView(const Vector3& pivot, float dt) {
    if (!framingEnabled_) return;
    if (IsInPerformance()) return;

    Vector3 toPivot = pivot - transform_.translate;
    float   dist    = Length(toPivot);
    if (dist < 0.01f) return;

    Vector3 dir = toPivot / dist;

    // pivot を中央に置くための理想 yaw / pitch
    float desiredYaw   = atan2f(dir.x, dir.z);
    float clampedY     = std::clamp(-dir.y, -1.0f, 1.0f);
    float desiredPitch = asinf(clampedY);

    // 現在角との差分（yaw は ±π にラップ）
    float deltaYaw   = desiredYaw   - transform_.rotate.y;
    float deltaPitch = desiredPitch - transform_.rotate.x;

    constexpr float kPi    = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717958f;
    while (deltaYaw >  kPi) deltaYaw -= kTwoPi;
    while (deltaYaw < -kPi) deltaYaw += kTwoPi;

    const float maxStep = framingSpeed_ * dt;

    // ── Yaw ：マージンを越えた分だけ補正 ──────────────────────
    if (std::abs(deltaYaw) > framingYawMargin_) {
        float overshoot = (deltaYaw > 0.0f)
            ? deltaYaw - framingYawMargin_
            : deltaYaw + framingYawMargin_;
        float step = std::clamp(overshoot, -maxStep, maxStep);
        transform_.rotate.y += step;
    }

    // ── Pitch ：マージンを越えた分だけ補正 ────────────────────
    if (std::abs(deltaPitch) > framingPitchMargin_) {
        float overshoot = (deltaPitch > 0.0f)
            ? deltaPitch - framingPitchMargin_
            : deltaPitch + framingPitchMargin_;
        float step = std::clamp(overshoot, -maxStep, maxStep);
        transform_.rotate.x = std::clamp(
            transform_.rotate.x + step, minPitch_, maxPitch_);
    }
}

// ============================================================
// リセンター開始：対象 facing の背後へ回す目標角を確定する
//   yaw は ±π 最短角で寄せ、pitch は既定値へ戻す（設定時）。
//   recenterDuration_ が 0 なら即時スナップ。
// ============================================================
void FollowCamera::RecenterBehindTarget() {
    if (!target_) return;
    // 対象の向きへ寄せる（pitch リセットは従来どおり recenterResetPitch_ に従う）
    RecenterToYaw(target_->rotate_.y, recenterResetPitch_);
}

// ============================================================
// 任意のワールド yaw へ背後リセンター（RecenterBehindTarget の汎用版）
//   duration が負値なら recenterDuration_（手動リセンターの既定値）を使う。
//   アイドルオートリセンターなど、専用の緩やかな時間を使いたい場合だけ
//   正の値を明示的に渡す。
// ============================================================
void FollowCamera::RecenterToYaw(float targetWorldYaw, bool resetPitch, float duration, float protectDuration) {
    constexpr float kPi    = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717958f;

    // アイドルオートリセンター経由・手動リセンター経由を問わず、
    // 何らかのリセンターが起動したらアイドルタイマーは必ず0に戻す
    // （直後に再発火して二重に動くのを防ぐ）。
    idleRecenterTimer_ = 0.0f;

    recenterFromYaw_ = transform_.rotate.y;

    // 現在角から最短回転になるよう正規化
    float delta = targetWorldYaw - recenterFromYaw_;
    while (delta >  kPi) delta -= kTwoPi;
    while (delta < -kPi) delta += kTwoPi;
    recenterToYaw_ = recenterFromYaw_ + delta;

    recenterFromPitch_ = transform_.rotate.x;
    recenterToPitch_   = resetPitch
        ? std::clamp(recenterPitch_, minPitch_, maxPitch_)
        : transform_.rotate.x;

    activeRecenterDuration_ = (duration >= 0.0f) ? duration : recenterDuration_;

    recenterTimer_ = 0.0f;
    recentering_   = true;
    recenterProtectTimer_ = std::max(0.0f, protectDuration);

    // 即時スナップ
    if (activeRecenterDuration_ <= 0.0f) {
        transform_.rotate.y = recenterToYaw_;
        transform_.rotate.x = recenterToPitch_;
        recentering_ = false;
        recenterProtectTimer_ = 0.0f;
    }
}

// ============================================================
// リセンター中断
//   force=false: 保護時間中は無視（スティック等の割り込みから決めカメラを守る）
//   force=true : 保護を無視して即中断
// ============================================================
void FollowCamera::CancelRecenter(bool force) {
    if (!force && recenterProtectTimer_ > 0.0f) return;
    recentering_ = false;
    recenterProtectTimer_ = 0.0f;
}

// ============================================================
// リセンター更新：目標角へイーズアウトで寄せる
// ============================================================
void FollowCamera::UpdateRecenter(float dt) {
    if (!recentering_) return;

    if (recenterProtectTimer_ > 0.0f) {
        recenterProtectTimer_ = std::max(0.0f, recenterProtectTimer_ - dt);
    }

    recenterTimer_ += dt;
    float t = (activeRecenterDuration_ > 0.0f)
        ? std::clamp(recenterTimer_ / activeRecenterDuration_, 0.0f, 1.0f)
        : 1.0f;

    // イーズアウト（1-(1-t)^3）：最初キビキビ動いて減速しながら収まる
    float inv  = 1.0f - t;
    float ease = 1.0f - inv * inv * inv;

    transform_.rotate.y = recenterFromYaw_   + (recenterToYaw_   - recenterFromYaw_)   * ease;
    transform_.rotate.x = recenterFromPitch_ + (recenterToPitch_ - recenterFromPitch_) * ease;

    if (t >= 1.0f) recentering_ = false;
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

    if (ImGui::TreeNode("追従スムージング＆先読み")) {
        ImGui::Checkbox("位置ダンピング有効", &positionSmoothing_);
        ImGui::DragFloat("追従遅れ時間 (秒)", &positionSmoothTime_, 0.005f, 0.0f, 1.0f, "%.3f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("小さいほどキビキビ、大きいほどフワッと遅れて追従");
        ImGui::DragFloat("追従速度上限",       &maxFollowSpeed_,     1.0f,   1.0f, 2000.0f);
        ImGui::DragFloat("スナップ距離",       &followSnapDistance_, 0.5f,   1.0f, 200.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("この距離以上ズレたら瞬間移動（テレポート対策）");
        ImGui::Spacing();
        ImGui::Checkbox("先読み有効",          &lookAheadEnabled_);
        ImGui::Checkbox("Y方向も先読み",       &lookAheadVertical_);
        ImGui::DragFloat("先読み時間 (秒)",    &lookAheadTime_,      0.01f,  0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("移動速度×この秒数だけ進行方向の先を見る");
        ImGui::DragFloat("先読み距離上限",     &lookAheadMaxDist_,   0.1f,   0.0f, 30.0f);
        ImGui::DragFloat("先読みイーズ速度",   &lookAheadSmooth_,    0.1f,   0.1f, 30.0f);
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

    if (ImGui::TreeNode("フレーミング補正（画角外引き戻し）")) {
        ImGui::Checkbox("有効",                 &framingEnabled_);
        ImGui::DragFloat("Yaw マージン (rad)",  &framingYawMargin_,   0.01f, 0.0f, 1.5f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("カメラ前方とpivot方向の差がこれを越えたら横方向補正開始");
        ImGui::DragFloat("Pitch マージン (rad)", &framingPitchMargin_, 0.01f, 0.0f, 1.5f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("同じく縦方向の補正開始しきい値");
        ImGui::DragFloat("補正速度 (rad/秒)",    &framingSpeed_,       0.1f,  0.0f, 30.0f, "%.1f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("はみ出した分に対する最大変化量。大きいほどキビキビ戻る");
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (ImGui::TreeNode("アイドル時オートリセンター")) {
        ImGui::Checkbox("有効", &idleRecenterEnabled_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("カメラ操作(スティック/ロックオン等)が一定時間なければ対象の背後へ静かに戻す");
        ImGui::DragFloat("発動までの無操作時間 (秒)", &idleRecenterDelay_,    0.1f, 0.0f, 15.0f, "%.1f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("NotifyCameraActive() が呼ばれない状態がこの秒数続いたら発動");
        ImGui::DragFloat("寄せ時間 (秒)",             &idleRecenterDuration_, 0.05f, 0.0f, 5.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("RB/LBの手動リセンターより緩やかにするのが基本");
        ImGui::TextDisabled("無操作継続: %.1f / %.1f 秒", idleRecenterTimer_, idleRecenterDelay_);
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (ImGui::TreeNode("リセンター（RB/LB で対象背後へ）")) {
        ImGui::DragFloat("寄せ時間 (秒)", &recenterDuration_, 0.01f, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("対象の向きへ寄せる時間。0 で即時スナップ");
        ImGui::Checkbox("pitch も既定へ戻す", &recenterResetPitch_);
        if (recenterResetPitch_) {
            ImGui::DragFloat("pitch リセット先 (rad)", &recenterPitch_, 0.01f, minPitch_, maxPitch_, "%.2f");
        }
        if (ImGui::Button("テスト発火")) RecenterBehindTarget();
        ImGui::SameLine();
        ImGui::Text(recentering_ ? "寄せ中..." : "待機");
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

    // 追従スムージング
    j["positionSmoothing"]  = positionSmoothing_;
    j["positionSmoothTime"] = positionSmoothTime_;
    j["maxFollowSpeed"]     = maxFollowSpeed_;
    j["followSnapDistance"] = followSnapDistance_;

    // 速度先読み
    j["lookAheadEnabled"]   = lookAheadEnabled_;
    j["lookAheadVertical"]  = lookAheadVertical_;
    j["lookAheadTime"]      = lookAheadTime_;
    j["lookAheadMaxDist"]   = lookAheadMaxDist_;
    j["lookAheadSmooth"]    = lookAheadSmooth_;

    // フレーミング補正
    j["framingEnabled"]     = framingEnabled_;
    j["framingYawMargin"]   = framingYawMargin_;
    j["framingPitchMargin"] = framingPitchMargin_;
    j["framingSpeed"]       = framingSpeed_;

    // アイドル時オートリセンター
    j["idleRecenterEnabled"]  = idleRecenterEnabled_;
    j["idleRecenterDelay"]    = idleRecenterDelay_;
    j["idleRecenterDuration"] = idleRecenterDuration_;

    // リセンター
    j["recenterDuration"]   = recenterDuration_;
    j["recenterResetPitch"] = recenterResetPitch_;
    j["recenterPitch"]      = recenterPitch_;

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

    // 追従スムージング
    positionSmoothing_   = j.value("positionSmoothing",  true);
    positionSmoothTime_  = j.value("positionSmoothTime", 0.12f);
    maxFollowSpeed_      = j.value("maxFollowSpeed",      300.0f);
    followSnapDistance_  = j.value("followSnapDistance",  30.0f);

    // 速度先読み
    lookAheadEnabled_    = j.value("lookAheadEnabled",   true);
    lookAheadVertical_   = j.value("lookAheadVertical",  false);
    lookAheadTime_       = j.value("lookAheadTime",      0.25f);
    lookAheadMaxDist_    = j.value("lookAheadMaxDist",   6.0f);
    lookAheadSmooth_     = j.value("lookAheadSmooth",    6.0f);

    framingEnabled_      = j.value("framingEnabled",      true);
    // セーフティネット既定値を拡大（≈60°/≈31°）。通常のプレイでは介入せず、
    // 画角から本当に外れそうな時だけの最終保険として機能させる。
    framingYawMargin_    = j.value("framingYawMargin",    1.05f);
    framingPitchMargin_  = j.value("framingPitchMargin",  0.55f);
    framingSpeed_        = j.value("framingSpeed",        4.0f);

    // アイドル時オートリセンター
    idleRecenterEnabled_  = j.value("idleRecenterEnabled",  true);
    idleRecenterDelay_    = j.value("idleRecenterDelay",    2.5f);
    idleRecenterDuration_ = j.value("idleRecenterDuration", 1.4f);

    // リセンター
    recenterDuration_    = j.value("recenterDuration",   0.18f);
    recenterResetPitch_  = j.value("recenterResetPitch", true);
    recenterPitch_       = j.value("recenterPitch",      0.30f);
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
