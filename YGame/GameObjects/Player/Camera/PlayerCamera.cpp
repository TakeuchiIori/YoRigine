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

    // 脅威察知パラメータを extension JSON から復元
    if (ext.contains("threatAwareness")) {
        LoadThreatAwareness(ext["threatAwareness"]);
    }

    // ロックオン2ショット・フレーミングを extension JSON から復元
    if (ext.contains("lockOnFraming")) {
        LoadLockOnFraming(ext["lockOnFraming"]);
    }

    // 見切れヒット演出パラメータを extension JSON から復元
    if (ext.contains("offscreenHitReaction")) {
        LoadOffscreenHitReaction(ext["offscreenHitReaction"]);
    }

#ifdef USE_IMGUI
    editor_.Initialize(&attackCamera_, this);
    editor_.SetFilePath(defaultPath);
#endif
}

// ============================================================
// スティック入力・ロックオン更新
// CameraDirector::Update() の前に GameScene::Update() から呼ぶ。
// FollowCamera の回転を確定させることで、後続の FollowProcess() が
// 正しい位置を計算できるようにする。
// ============================================================
void PlayerCamera::UpdatePreDirector() {
    if (!followCamera_) return;

    float dt = YoRigine::GameTime::GetDeltaTime();
    bool  inPerformance = IsInPerformance();

    // 見切れヒット演出のクールダウンを進める（ヒットストップの影響を受けないよう非スケール）
    if (offscreenHitCdTimer_ > 0.0f) {
        offscreenHitCdTimer_ -= YoRigine::GameTime::GetUnscaledDeltaTime();
        if (offscreenHitCdTimer_ < 0.0f) offscreenHitCdTimer_ = 0.0f;
    }

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

        // RB / LB で自機の向いている方向の背後へ素早くリセンター（フリーカメラ時のみ）
        if (!isLockOn_ &&
            (YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::RB) ||
             YoRigine::Input::GetInstance()->IsPadTriggered(0, GamePadButton::LB))) {
            followCamera_->RecenterBehindTarget();
        }

        if (isLockOn_) {
            // ロックオン中はグランスを使わない。蓄積をリセットして次回フリー時の段差を防ぐ。
            awarenessYawBias_ = 0.0f;
            awarenessAppliedBias_ = 0.0f;
            UpdateLockOn();
        } else {
            UpdateStickInput();
            // 脅威察知（気配）：周辺視グランス（右スティック未操作時のみ介入）
            UpdateThreatAwareness(dt);
        }

        // 攻撃カメラのシェイクトリガーも先に処理（次フレームの FollowProcess で適用）
        attackCamera_.UpdatePre(followCamera_, dt);
    }
}

// ============================================================
// 攻撃カメラオフセットを sceneCamera に後付けで適用
// CameraDirector::Update() + UpdateCamera() の後に呼ぶ
// ============================================================
void PlayerCamera::ApplyPostDirector(Camera* sceneCamera, float dt) {
    if (!sceneCamera) return;

    // 見切れヒット判定に使う最終カメラをキャッシュ（OnAttackHit から参照）
    lastSceneCamera_ = sceneCamera;

    bool inPerformance = IsInPerformance();
    if (inPerformance) return;

    // 脅威察知：囲まれFOV拡大（攻撃カメラより先に適用。攻撃中は後段が上書きする）
    ApplyThreatFovWiden(sceneCamera, dt);

    // キーフレーム補間値を更新
    attackCamera_.UpdatePost(dt);

    if (!attackCamera_.IsActive()) {
        // 演出が完全に終了したフレームでタイムスケールをリセット
        YoRigine::GameTime::SetTimeScale(1.0f);
        return;
    }

    // ---- 位置オフセット（参照フレーム空間 → ワールド空間に変換して加算） ----
    Vector3 posOff = attackCamera_.GetCurrentPosOffset();
    if (posOff.x != 0.0f || posOff.y != 0.0f || posOff.z != 0.0f) {
        Matrix4x4 frame    = BuildOffsetFrame(attackCamera_.GetCurrentPosSpace(), sceneCamera);
        Vector3   worldOff = TransformNormal(posOff, frame);
        sceneCamera->transform_.translate = sceneCamera->transform_.translate + worldOff;
    }

    // ---- 注視ブレンド（rotOffset 加算より先に。対象へ向けてから微調整を足す） ----
    float lookWeight = attackCamera_.GetCurrentLookAtWeight();
    if (lookWeight > 0.0f) {
        ApplyLookAt(sceneCamera, attackCamera_.GetCurrentLookAt(), lookWeight);
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

    //------------------------------------------------------------
    // 最終フレーミングガード
    // オフセット適用後の view-projection でプレイヤーを投影し、
    // 画角外へはみ出していたら引き戻す（ワーク側で許可されている場合のみ）。
    //------------------------------------------------------------
    if (framingGuardEnabled_ && attackCamera_.ShouldKeepPlayerInFrame()) {
        EnsurePlayerInFrame(sceneCamera, dt);
    }
}

// ============================================================
// 最終フレーミングガード
// プレイヤーのピボットを最終 view-projection で NDC に投影し、
// ハードリミットを越えた分だけ yaw / pitch を引き戻してフレーム内へ収める。
// ============================================================
void PlayerCamera::EnsurePlayerInFrame(Camera* sceneCamera, float dt) {
    if (!sceneCamera || !playerWT_) return;

    //------------------------------------------------------------
    // プレイヤーのピボット位置（ワールド空間）を求める
    //------------------------------------------------------------
    // matWorld_ の平行移動成分がプレイヤーのワールド原点
    Vector3 playerPivot = Transform(Vector3{ 0.0f, 0.0f, 0.0f }, playerWT_->matWorld_);
    playerPivot.y += pivotHeight_;

    //------------------------------------------------------------
    // NDC への投影（透視除算前の w で前後判定）
    //------------------------------------------------------------
    Vector4 clip = Transform(
        Vector4{ playerPivot.x, playerPivot.y, playerPivot.z, 1.0f },
        sceneCamera->GetViewProjectionMatrix());

    // カメラの背後（w <= 0）なら NDC が破綻するので、向きを直接プレイヤーへ向ける
    bool behindCamera = (clip.w <= 0.0001f);

    float ndcX = 0.0f;
    float ndcY = 0.0f;
    if (!behindCamera) {
        ndcX = clip.x / clip.w;
        ndcY = clip.y / clip.w;
        // ハードリミット内に完全に収まっていれば何もしない（小刻みな揺れ防止）
        if (std::abs(ndcX) <= framingHardLimitX_ &&
            std::abs(ndcY) <= framingHardLimitY_) {
            return;
        }
    }

    //------------------------------------------------------------
    // プレイヤーを中央に置くための理想 yaw / pitch を計算
    //------------------------------------------------------------
    Vector3 toPivot = playerPivot - sceneCamera->transform_.translate;
    float   dist    = Length(toPivot);
    if (dist < 0.01f) return;
    Vector3 dir = toPivot / dist;

    float desiredYaw   = atan2f(dir.x, dir.z);
    float clampedY     = std::clamp(-dir.y, -1.0f, 1.0f);
    float desiredPitch = asinf(clampedY);

    float deltaYaw   = desiredYaw   - sceneCamera->transform_.rotate.y;
    float deltaPitch = desiredPitch - sceneCamera->transform_.rotate.x;

    constexpr float kPi    = 3.14159265358979f;
    constexpr float kTwoPi = 6.28318530717958f;
    while (deltaYaw >  kPi) deltaYaw -= kTwoPi;
    while (deltaYaw < -kPi) deltaYaw += kTwoPi;

    //------------------------------------------------------------
    // NDC ハードリミットを「中央からの角度許容量」に変換
    // 縦半画角 = fovY / 2、横半画角 = atan(tan(縦半画角) * アスペクト比)
    //------------------------------------------------------------
    float halfFovY = sceneCamera->fovY_ * 0.5f;
    float halfFovX = atanf(tanf(halfFovY) * sceneCamera->aspectRatio_);
    float yawTol   = atanf(framingHardLimitX_ * tanf(halfFovX));
    float pitchTol = atanf(framingHardLimitY_ * tanf(halfFovY));

    const float maxStep = framingGuardSpeed_ * dt;

    //------------------------------------------------------------
    // Yaw：許容量を越えた分だけ引き戻す（背後ならフルに向ける）
    //------------------------------------------------------------
    if (behindCamera || std::abs(deltaYaw) > yawTol) {
        float overshoot = behindCamera
            ? deltaYaw
            : (deltaYaw > 0.0f ? deltaYaw - yawTol : deltaYaw + yawTol);
        float step = std::clamp(overshoot, -maxStep, maxStep);
        sceneCamera->transform_.rotate.y += step;
    }

    //------------------------------------------------------------
    // Pitch：同様に引き戻す（pitch 制限内にクランプ）
    //------------------------------------------------------------
    if (behindCamera || std::abs(deltaPitch) > pitchTol) {
        float overshoot = behindCamera
            ? deltaPitch
            : (deltaPitch > 0.0f ? deltaPitch - pitchTol : deltaPitch + pitchTol);
        float step = std::clamp(overshoot, -maxStep, maxStep);
        sceneCamera->transform_.rotate.x = std::clamp(
            sceneCamera->transform_.rotate.x + step, minPitch_, maxPitch_);
    }

    //------------------------------------------------------------
    // 引き戻した回転を反映
    //------------------------------------------------------------
    sceneCamera->UpdateMatrix();
}


// ============================================================
// スティック入力で FollowCamera の回転を更新
// ============================================================
void PlayerCamera::UpdateStickInput() {
    stickActiveThisFrame_ = false;
    if (!followCamera_) return;
    if (followCamera_->GetIsCloseUp()) return;

    if (YoRigine::Input::GetInstance()->IsControllerConnected()) {
        XINPUT_STATE js;
        if (YoRigine::Input::GetInstance()->GetJoystickState(0, js)) {
            Vector3 move{};
            move.y += static_cast<float>(js.Gamepad.sThumbRX);
            move.x -= static_cast<float>(js.Gamepad.sThumbRY);

            if (Length(move) > 5000.0f) {
                stickActiveThisFrame_ = true;
                // 手動操作が入ったらリセンターを中断（プレイヤー操作を優先）
                followCamera_->CancelRecenter();
                move = Normalize(move) * rotateSpeed_;
                Vector3 rot = followCamera_->GetRotate();
                rot += move;
                rot.x = std::clamp(rot.x, minPitch_, maxPitch_);
                followCamera_->SetRotate(rot);
            }
        }
    }
}

// ── ヨー角を ±π に正規化 ──
static float WrapPi(float a) {
    constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717958f;
    while (a <= -kPi) a += kTwoPi;
    while (a >   kPi) a -= kTwoPi;
    return a;
}

// ============================================================
// 周辺視グランス（pre-director）
//   視界外/背後の敵がいたら、その方向へ awarenessMaxYaw_ までだけカメラを
//   軽く傾ける。ロックオンのように敵を中央へ固定はしない＝「気配」を伝える。
//   バイアスはテレスコープ方式で yaw に加減算し、脅威が消えたら自然に戻す。
// ============================================================
void PlayerCamera::UpdateThreatAwareness(float dt) {
    if (!followCamera_) return;

    // 目標グランス量を決める（無効・操作中・対象なしなら 0 へ戻す）
    float targetBias = 0.0f;
    if (threatAwarenessEnabled_ && threatAwarenessSceneAllowed_
        && !stickActiveThisFrame_ && playerWT_ && !followCamera_->GetIsCloseUp()) {
        targetBias = ComputeGlanceBias();
    }

    // バイアスを滑らかに目標へ寄せる
    float t = std::clamp(awarenessYawSpeed_ * dt, 0.0f, 1.0f);
    awarenessYawBias_ += (targetBias - awarenessYawBias_) * t;

    // テレスコープ適用：前回適用分との差分だけ yaw を動かす
    //   （プレイヤーの右スティック操作とは独立に、awareness の寄与は常に
    //    awarenessYawBias_ 分だけになる。脅威が消えれば 0 に戻り原状回復する）
    float delta = awarenessYawBias_ - awarenessAppliedBias_;
    if (std::abs(delta) > 1e-7f) {
        Vector3 rot = followCamera_->GetRotate();
        rot.y = WrapPi(rot.y + delta);
        followCamera_->SetRotate(rot);
        awarenessAppliedBias_ = awarenessYawBias_;
    }
}

// ============================================================
// グランス目標量を算出
//   カメラ前方から awarenessTriggerYaw_ 以上外れた（視界外寄りの）敵のうち
//   プレイヤーに最も近いものへ、符号付きで awarenessMaxYaw_ までの傾きを返す。
// ============================================================
float PlayerCamera::ComputeGlanceBias() const {
    std::vector<BaseCollider*> enemies;
    if (GatherNearbyEnemies(enemies) == 0) return 0.0f;

    Vector3 camPos  = followCamera_->GetTranslate();
    float   baseYaw = followCamera_->GetRotate().y - awarenessAppliedBias_; // プレイヤー由来の素の yaw
    Vector3 playerPos = playerWT_->translate_;

    BaseCollider* pick = nullptr;
    float pickDist   = awarenessRange_;
    float pickSigned = 0.0f;
    for (auto* e : enemies) {
        Vector3 d = e->GetCenterPosition() - camPos; d.y = 0.0f;
        if (Length(d) < 0.01f) continue;
        float signedYaw = WrapPi(atan2f(d.x, d.z) - baseYaw);
        if (std::abs(signedYaw) < awarenessTriggerYaw_) continue; // 視界内寄り＝気配対象外

        float pd = Length(e->GetCenterPosition() - playerPos);
        if (pd < pickDist) { pickDist = pd; pick = e; pickSigned = signedYaw; }
    }
    if (!pick) return 0.0f;

    return std::clamp(pickSigned, -awarenessMaxYaw_, awarenessMaxYaw_);
}

// ============================================================
// 囲まれFOV拡大（post-director）
//   近くの敵が awarenessFovMinCount_ 体以上いるほど FOV を少し広げ、
//   「囲まれている」状況を見渡せるようにする。攻撃カメラ等が FOV を
//   上書きするフレームでは自然にそちらが優先される。
// ============================================================
void PlayerCamera::ApplyThreatFovWiden(Camera* sceneCamera, float dt) {
    if (!sceneCamera) return;

    float target = 0.0f;
    if (threatAwarenessEnabled_ && threatAwarenessSceneAllowed_) {
        std::vector<BaseCollider*> enemies;
        int n = GatherNearbyEnemies(enemies);
        if (n >= awarenessFovMinCount_) {
            target = std::min(
                (n - awarenessFovMinCount_ + 1) * awarenessFovPerEnemy_,
                awarenessFovMax_);
        }
    }

    float t = std::clamp(awarenessFovSpeed_ * dt, 0.0f, 1.0f);
    awarenessFovBias_ += (target - awarenessFovBias_) * t;

    if (awarenessFovBias_ > 1e-5f) {
        sceneCamera->fovY_ += awarenessFovBias_;
        sceneCamera->UpdateMatrix();
    }
}

// ============================================================
// 近傍の敵を収集（プレイヤーから awarenessRange_ 内のアクティブな敵）
// ============================================================
int PlayerCamera::GatherNearbyEnemies(std::vector<BaseCollider*>& out) const {
    out.clear();
    if (!playerWT_) return 0;

    Vector3 playerPos = playerWT_->translate_;
    for (auto* col : YoRigine::CollisionManager::GetInstance()->GetColliders()) {
        if (!col->GetIsActive()) continue;
        uint32_t id = col->GetTypeID();
        if (id != static_cast<uint32_t>(CollisionTypeIdDef::kBattleEnemy) &&
            id != static_cast<uint32_t>(CollisionTypeIdDef::kFieldEnemy)) {
            continue;
        }
        if (Length(col->GetCenterPosition() - playerPos) <= awarenessRange_) {
            out.push_back(col);
        }
    }
    return static_cast<int>(out.size());
}

// ============================================================
// 脅威察知パラメータの保存（FollowCamera の extension JSON へ書き込む内容）
//   シーン許可フラグ・内部状態（bias 等）は実行時のものなので保存しない。
// ============================================================
void PlayerCamera::SaveThreatAwareness(nlohmann::json& j) const {
    j["enabled"]        = threatAwarenessEnabled_;
    j["range"]          = awarenessRange_;
    j["triggerYaw"]     = awarenessTriggerYaw_;
    j["maxYaw"]         = awarenessMaxYaw_;
    j["yawSpeed"]       = awarenessYawSpeed_;
    j["fovMinCount"]    = awarenessFovMinCount_;
    j["fovPerEnemy"]    = awarenessFovPerEnemy_;
    j["fovMax"]         = awarenessFovMax_;
    j["fovSpeed"]       = awarenessFovSpeed_;
}

// ============================================================
// 脅威察知パラメータの復元
// ============================================================
void PlayerCamera::LoadThreatAwareness(const nlohmann::json& j) {
    threatAwarenessEnabled_ = j.value("enabled",      true);
    awarenessRange_         = j.value("range",        25.0f);
    awarenessTriggerYaw_    = j.value("triggerYaw",   0.70f);
    awarenessMaxYaw_        = j.value("maxYaw",        0.18f);
    awarenessYawSpeed_      = j.value("yawSpeed",      4.0f);
    awarenessFovMinCount_   = j.value("fovMinCount",   2);
    awarenessFovPerEnemy_   = j.value("fovPerEnemy",   0.03f);
    awarenessFovMax_        = j.value("fovMax",        0.12f);
    awarenessFovSpeed_      = j.value("fovSpeed",      3.0f);
}

// ============================================================
// ロックオン2ショット・フレーミングパラメータの保存 / 復元
// ============================================================
void PlayerCamera::SaveLockOnFraming(nlohmann::json& j) const {
    j["shoulderYaw"] = lockOnShoulderYaw_;
    j["pitchBias"]   = lockOnPitchBias_;
    j["lerpSpeed"]   = lockOnLerpSpeed_;
}

void PlayerCamera::LoadLockOnFraming(const nlohmann::json& j) {
    lockOnShoulderYaw_ = j.value("shoulderYaw", 0.25f);
    lockOnPitchBias_   = j.value("pitchBias",   0.20f);
    lockOnLerpSpeed_   = j.value("lerpSpeed",   10.0f);
}

// ============================================================
// 見切れヒット演出
// ============================================================

// ワールド座標を最終カメラの view-projection で NDC へ投影し、画角外か判定する。
bool PlayerCamera::IsWorldPosOffscreen(const Vector3& worldPos) const {
    if (!lastSceneCamera_) return false;  // まだ最終カメラが無い＝判定不能（誤発火させない）

    Vector4 clip = Transform(
        Vector4{ worldPos.x, worldPos.y, worldPos.z, 1.0f },
        lastSceneCamera_->GetViewProjectionMatrix());

    if (clip.w <= 0.0001f) return true;   // カメラ背後＝完全に見えていない

    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    return std::abs(ndcX) > offscreenHitMargin_ || std::abs(ndcY) > offscreenHitMargin_;
}

// 背後リセンター＋ズーム寄り＋シェイクで、見失った敵を捉え直す決めカメラを発火する。
void PlayerCamera::TriggerOffscreenHitReaction(const Vector3* enemyWorldPos) {
    if (!followCamera_) return;

    // 背後リセンター：カメラ→プレイヤー→敵 が一直線になる位置まで回り込ませる。
    // 敵座標があれば「プレイヤー→敵の方向」へ寄せる＝プレイヤーの向きに依存せず
    // 確実にプレイヤー越しの敵を正面に捉える。無ければ従来のプレイヤー向きへ。
    if (offscreenHitFaceEnemy_ && enemyWorldPos && playerWT_) {
        Vector3 dir = *enemyWorldPos - PlayerPivotWorld();
        dir.y = 0.0f;
        if (Length(dir) > 0.001f) {
            followCamera_->RecenterToYaw(atan2f(dir.x, dir.z));
        } else {
            followCamera_->RecenterBehindTarget();
        }
    } else {
        followCamera_->RecenterBehindTarget();
    }

    // 軽い寄り（StartZoom は指定FOV→基準FOVへイーズで戻る＝ズームパンチ）
    if (offscreenHitZoomFov_ > 0.0f) {
        followCamera_->StartZoom(offscreenHitZoomFov_, offscreenHitZoomDur_);
    }

    // コツンとシェイク
    if (offscreenHitShakeInt_ > 0.0f && offscreenHitShakeDur_ > 0.0f) {
        followCamera_->StartShake(offscreenHitShakeInt_, offscreenHitShakeDur_);
    }
}

// 武器のヒット検出から呼ぶ入口。発火条件を満たし、かつ敵が画面外の時だけ演出する。
void PlayerCamera::OnAttackHit(const Vector3& enemyWorldPos) {
    if (!offscreenHitEnabled_ || !followCamera_) return;
    if (!threatAwarenessSceneAllowed_)  return;  // バトル中のみ（フィールドでは出さない）
    if (IsInPerformance())              return;  // 戦闘開始演出等の最中はスキップ
    if (offscreenHitCdTimer_ > 0.0f)    return;  // クールダウン中
    // NOTE: 攻撃カメラワーク再生中でもあえて発火させる。ヒット時は NotifyAttackHit が
    //       先に攻撃カメラを起動するため、ここで弾くとほぼ毎回スキップされてしまう。
    //       背後リセンターは base 回転を動かすので、攻撃ワークのオフセットと共存できる。
    if (!IsWorldPosOffscreen(enemyWorldPos)) return;  // 画面内に見えているなら何もしない

    TriggerOffscreenHitReaction(&enemyWorldPos);
    offscreenHitCdTimer_ = offscreenHitCooldown_;
}

// パラメータの保存 / 復元（FollowCamera の extension JSON に相乗り）
void PlayerCamera::SaveOffscreenHitReaction(nlohmann::json& j) const {
    j["enabled"]   = offscreenHitEnabled_;
    j["faceEnemy"] = offscreenHitFaceEnemy_;
    j["margin"]   = offscreenHitMargin_;
    j["cooldown"] = offscreenHitCooldown_;
    j["zoomFov"]  = offscreenHitZoomFov_;
    j["zoomDur"]  = offscreenHitZoomDur_;
    j["shakeInt"] = offscreenHitShakeInt_;
    j["shakeDur"] = offscreenHitShakeDur_;
}

void PlayerCamera::LoadOffscreenHitReaction(const nlohmann::json& j) {
    offscreenHitEnabled_   = j.value("enabled",   true);
    offscreenHitFaceEnemy_ = j.value("faceEnemy", true);
    offscreenHitMargin_   = j.value("margin",   0.9f);
    offscreenHitCooldown_ = j.value("cooldown", 0.8f);
    offscreenHitZoomFov_  = j.value("zoomFov",  0.40f);
    offscreenHitZoomDur_  = j.value("zoomDur",  0.30f);
    offscreenHitShakeInt_ = j.value("shakeInt", 0.30f);
    offscreenHitShakeDur_ = j.value("shakeDur", 0.15f);
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

    // ── 2ショット・フレーミング ──────────────────────────────
    // カメラを player→enemy 軸の真後ろに置くと、プレイヤーの背中が敵を隠して
    // 攻撃が当てづらくなる。そこで:
    //   ・ヨーに肩オフセット(lockOnShoulderYaw_)を足して軸からずらす
    //     （offset_ に X 成分が無いためプレイヤーは中央のまま、敵が片側へ寄る）
    //   ・見下ろし(lockOnPitchBias_)を足して2体を縦に分離する
    // ことで、プレイヤーと敵が重ならない斜め2ショットにする。
    Vector3 pivot    = playerWT_->translate_ + Vector3(0.0f, pivotHeight_, 0.0f);
    Vector3 enemyPos = lockedTarget_->GetCenterPosition();
    Vector3 dir      = Normalize(enemyPos - pivot);

    float targetYaw   = atan2f(dir.x, dir.z) + lockOnShoulderYaw_;
    float targetPitch = std::clamp(asinf(-dir.y) + lockOnPitchBias_, minPitch_, maxPitch_);

    float t = std::clamp(lockOnLerpSpeed_ * YoRigine::GameTime::GetUnscaledDeltaTime(), 0.0f, 1.0f);
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

    // アンカー未指定 → ロックオン対象があればそれを既定アンカーにする
    hasCameraWorkAnchor_ = false;

    // 再生前 FOV を保存 → Play → savedBaseFov に渡す順で呼ぶ
    const float baseFov = followCamera_->GetBaseFovY();
    attackCamera_.Play(attackName);
    attackCamera_.SetSavedBaseFov(baseFov);
}

void PlayerCamera::PlayAttackCameraWork(const std::string& attackName, const Vector3& anchor) {
    if (!followCamera_) return;

    cameraWorkAnchor_    = anchor;
    hasCameraWorkAnchor_ = true;

    const float baseFov = followCamera_->GetBaseFovY();
    attackCamera_.Play(attackName);
    attackCamera_.SetSavedBaseFov(baseFov);
}

// ============================================================
// 攻撃カメラワーク停止
// ============================================================
void PlayerCamera::StopAttackCameraWork() {
    attackCamera_.Stop(followCamera_);
    hasCameraWorkAnchor_ = false;
    YoRigine::GameTime::SetTimeScale(1.0f);
}

// ============================================================
// プレイヤーピボット（pivotHeight 込み）のワールド座標
// ============================================================
Vector3 PlayerCamera::PlayerPivotWorld() const {
    if (!playerWT_) return {};
    Vector3 p = Transform(Vector3{ 0.0f, 0.0f, 0.0f }, playerWT_->matWorld_);
    p.y += pivotHeight_;
    return p;
}

// ============================================================
// 注視対象のワールド座標を解決（対象が無ければ false）
// ============================================================
bool PlayerCamera::ResolveLookAtPos(LookAtTarget target, Vector3& outPos) const {
    switch (target) {
    case LookAtTarget::None:
        return false;

    case LookAtTarget::Player:
        outPos = PlayerPivotWorld();
        return true;

    case LookAtTarget::LockedEnemy:
        if (lockedTarget_)        { outPos = lockedTarget_->GetCenterPosition(); return true; }
        if (hasCameraWorkAnchor_) { outPos = cameraWorkAnchor_;                  return true; }
        return false;

    case LookAtTarget::Midpoint: {
        Vector3 b;
        if      (lockedTarget_)        b = lockedTarget_->GetCenterPosition();
        else if (hasCameraWorkAnchor_) b = cameraWorkAnchor_;
        else                           return false;
        outPos = (PlayerPivotWorld() + b) * 0.5f;
        return true;
    }

    case LookAtTarget::Anchor:
        if (hasCameraWorkAnchor_) { outPos = cameraWorkAnchor_; return true; }
        return false;
    }
    return false;
}

// ============================================================
// posOffset を解釈する参照フレーム（回転行列）を組む
// ============================================================
Matrix4x4 PlayerCamera::BuildOffsetFrame(CameraSpace space, const Camera* sceneCamera) const {
    switch (space) {
    case CameraSpace::CameraLocal:
        return MakeRotateMatrixXYZ(sceneCamera->transform_.rotate);

    case CameraSpace::World:
        return MakeIdentity4x4();

    case CameraSpace::PlayerLocal: {
        float yaw = playerWT_ ? playerWT_->rotate_.y : 0.0f;
        return MakeRotateMatrixXYZ(Vector3{ 0.0f, yaw, 0.0f });
    }

    case CameraSpace::TargetRelative: {
        // プレイヤー→対象 の水平方向を +Z とした yaw のみの基底
        Vector3 target{};
        bool    ok = false;
        if      (lockedTarget_)        { target = lockedTarget_->GetCenterPosition(); ok = true; }
        else if (hasCameraWorkAnchor_) { target = cameraWorkAnchor_;                  ok = true; }

        if (ok && playerWT_) {
            Vector3 d = target - PlayerPivotWorld();
            d.y = 0.0f;
            if (Length(d) > 0.001f) {
                float yaw = atan2f(d.x, d.z);
                return MakeRotateMatrixXYZ(Vector3{ 0.0f, yaw, 0.0f });
            }
        }
        // 退避：プレイヤー基準
        float yaw = playerWT_ ? playerWT_->rotate_.y : 0.0f;
        return MakeRotateMatrixXYZ(Vector3{ 0.0f, yaw, 0.0f });
    }
    }
    return MakeIdentity4x4();
}

// ============================================================
// 注視ブレンド：カメラ回転を対象へ weight だけ寄せる
// （weight=1 で完全に対象を向く。yaw は最短角で補間）
// ============================================================
void PlayerCamera::ApplyLookAt(Camera* sceneCamera, LookAtTarget target, float weight) const {
    if (!sceneCamera) return;

    Vector3 lookPos{};
    if (!ResolveLookAtPos(target, lookPos)) return;

    Vector3 toTarget = lookPos - sceneCamera->transform_.translate;
    float   dist     = Length(toTarget);
    if (dist < 0.01f) return;
    Vector3 dir = toTarget / dist;

    float desiredYaw   = atan2f(dir.x, dir.z);
    float clampedY     = std::clamp(-dir.y, -1.0f, 1.0f);
    float desiredPitch = asinf(clampedY);

    float w = std::clamp(weight, 0.0f, 1.0f);

    // yaw は ±π 最短角で補間（巻き戻り防止）
    float deltaYaw = WrapPi(desiredYaw - sceneCamera->transform_.rotate.y);
    sceneCamera->transform_.rotate.y += deltaYaw * w;
    sceneCamera->transform_.rotate.x += (desiredPitch - sceneCamera->transform_.rotate.x) * w;
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

    if (ImGui::CollapsingHeader("脅威察知（気配）")) {
        ImGui::Checkbox("有効", &threatAwarenessEnabled_);
        ImGui::DragFloat("対象範囲", &awarenessRange_, 0.5f, 1.0f, 100.0f);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("この距離内の敵を『気配』対象にする");

        ImGui::SeparatorText("周辺視グランス");
        ImGui::DragFloat("発動角(rad)",   &awarenessTriggerYaw_, 0.01f, 0.1f, 3.14f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("カメラ前方からこの角度以上外れた敵に反応（小さいほど敏感）");
        ImGui::DragFloat("最大グランス量(rad)", &awarenessMaxYaw_, 0.01f, 0.0f, 0.8f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("傾ける最大ヨー量。小さいほど『チラ見』。大きくしすぎるとロックオン的になる");
        ImGui::DragFloat("グランス速度", &awarenessYawSpeed_, 0.1f, 0.5f, 20.0f);

        ImGui::SeparatorText("囲まれFOV");
        ImGui::DragInt("発動体数", &awarenessFovMinCount_, 1, 1, 10);
        ImGui::DragFloat("1体あたり拡大(rad)", &awarenessFovPerEnemy_, 0.005f, 0.0f, 0.2f, "%.3f");
        ImGui::DragFloat("拡大上限(rad)", &awarenessFovMax_, 0.01f, 0.0f, 0.5f, "%.2f");
        ImGui::DragFloat("FOV補間速度", &awarenessFovSpeed_, 0.1f, 0.5f, 20.0f);

        ImGui::Separator();
        ImGui::Text("シーン許可: %s", threatAwarenessSceneAllowed_ ? "ON(バトル)" : "OFF(フィールド等)");
        ImGui::Text("現在グランス: %.2f rad / FOV拡大: %.3f", awarenessYawBias_, awarenessFovBias_);

        // 編集値を extension JSON に反映（カメラ設定保存時に一緒に永続化される）
        nlohmann::json taJson;
        SaveThreatAwareness(taJson);
        nlohmann::json ext = followCamera_->GetExtensionJson();
        ext["threatAwareness"] = taJson;
        followCamera_->SetExtensionJson(ext);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("ロックオン2ショット")) {
        ImGui::DragFloat("肩オフセット(rad)", &lockOnShoulderYaw_, 0.01f, -0.8f, 0.8f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("player→enemy 軸からヨーをずらす量。0=真後ろ(被る)。±で左右の肩");
        ImGui::DragFloat("見下ろし加算(rad)", &lockOnPitchBias_, 0.01f, -0.3f, 0.8f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("プレイヤーと敵を縦に分離する見下ろし量");
        ImGui::DragFloat("追従速度", &lockOnLerpSpeed_, 0.1f, 1.0f, 30.0f);

        // 編集値を extension JSON に反映（カメラ設定保存時に一緒に永続化される）
        nlohmann::json loJson;
        SaveLockOnFraming(loJson);
        nlohmann::json ext = followCamera_->GetExtensionJson();
        ext["lockOnFraming"] = loJson;
        followCamera_->SetExtensionJson(ext);
    }

    ImGui::Separator();

    if (ImGui::CollapsingHeader("見切れヒット演出")) {
        ImGui::Checkbox("有効", &offscreenHitEnabled_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("画面外の敵に攻撃が当たった瞬間、敵を捉え直す決めカメラを発火");
        ImGui::Checkbox("敵の方向へ回り込む", &offscreenHitFaceEnemy_);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("ON=カメラ→プレイヤー→敵が一直線になる位置へ(確実に敵を正面へ) / OFF=プレイヤーの向いてる方向の背後へ");
        ImGui::DragFloat("画面外マージン(NDC)", &offscreenHitMargin_, 0.01f, 0.0f, 1.2f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("敵がこのNDCを越えたら『見切れ』扱い。値を小さくするほど中央寄りでも発動。0.9=端で切れかけ / 1.0=完全に枠外のみ / 0=画面中央以外すべて");
        ImGui::DragFloat("クールダウン(秒)", &offscreenHitCooldown_, 0.05f, 0.0f, 5.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("連続ヒットで毎回発火しないための間隔");

        ImGui::SeparatorText("演出");
        ImGui::DragFloat("ズームFOV", &offscreenHitZoomFov_, 0.005f, 0.0f, 1.0f, "%.3f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("寄りの目標FOV(基準より小さいほど寄る)。0でズームなし");
        ImGui::DragFloat("ズーム時間(秒)", &offscreenHitZoomDur_, 0.01f, 0.05f, 2.0f, "%.2f");
        ImGui::DragFloat("シェイク強度", &offscreenHitShakeInt_, 0.05f, 0.0f, 3.0f, "%.2f");
        ImGui::DragFloat("シェイク時間(秒)", &offscreenHitShakeDur_, 0.01f, 0.0f, 1.0f, "%.2f");

        ImGui::Separator();
        ImGui::Text("CD残り: %.2fs / シーン許可: %s", offscreenHitCdTimer_,
            threatAwarenessSceneAllowed_ ? "ON(バトル)" : "OFF");
        if (ImGui::Button("今すぐテスト発火")) {
            TriggerOffscreenHitReaction();
            offscreenHitCdTimer_ = offscreenHitCooldown_;
        }

        // 編集値を extension JSON に反映（カメラ設定保存時に一緒に永続化される）
        nlohmann::json ohJson;
        SaveOffscreenHitReaction(ohJson);
        nlohmann::json ext = followCamera_->GetExtensionJson();
        ext["offscreenHitReaction"] = ohJson;
        followCamera_->SetExtensionJson(ext);
    }

    ImGui::Separator();

    // ロックオン状態
    ImGui::Text("ロックオン: %s", isLockOn_ ? "ON" : "OFF");
    if (lockedTarget_) {
        ImGui::Text("ターゲット: %p", static_cast<void*>(lockedTarget_));
    }
#endif
}
