#include "PlayerShield.h"
#include "Collision/Core/CollisionManager.h"
#include "Collision/Core/CollisionTypeIdDef.h"
#include "MathFunc.h"
#include "Player/Player.h"
#include "Systems/GameTime/GameTime.h"

#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
Matrix4x4 RemoveScaleFromMatrix(const Matrix4x4 &matrix) {
  Matrix4x4 result = matrix;

  for (int row = 0; row < 3; ++row) {
    Vector3 axis{result.m[row][0], result.m[row][1], result.m[row][2]};

    const float length = Length(axis);
    if (length > 0.0001f) {
      result.m[row][0] /= length;
      result.m[row][1] /= length;
      result.m[row][2] /= length;
    }
  }

  return result;
}
} // namespace

// ============================================================
// デストラクタ
// ============================================================
PlayerShield::~PlayerShield() { obbCollider_->~OBBCollider(); }

// ============================================================
// 初期化処理
// ============================================================
void PlayerShield::Initialize(YoRigine::Camera *camera) {

  camera_ = camera;
  // ------------------------------------------------------------
  // モデル初期化
  // ------------------------------------------------------------
  obj_ = std::make_unique<YoRigine::Object3d>();
  obj_->Initialize();
  obj_->SetModel("Shield_Heater.obj");
  wt_.Initialize();

  // ------------------------------------------------------------
  // プレイヤーの「手ジョイント」を探索して、盾を接続
  // ------------------------------------------------------------
  FindHandJointIndex();
  wt_.parent_ = nullptr;

  // ------------------------------------------------------------
  // コライダー・Json・パーティクル初期化
  // ------------------------------------------------------------
  InitCollision();
  InitJson();
}

// ============================================================
// 毎フレーム更新処理
// ============================================================
void PlayerShield::Update() {
  UpdateGuardImpact(YoRigine::GameTime::GetDeltaTime());

  if (obj3d_) {
    SetPlayerWeaponPosition();
  }
  obbCollider_->Update();
}

// ============================================================
// 受け止め演出の開始
// ============================================================
void PlayerShield::PlayGuardImpact(float amount, float duration,
                                   const Vector3 &axis, float bounce) {
  if (amount <= 0.0f || duration <= 0.0f)
    return;

  guardImpactAmount_ = amount;
  guardImpactDuration_ = duration;
  guardImpactTimer_ = duration;
  guardImpactAxis_ = axis;
  guardImpactBounce_ = std::max(0.1f, bounce);
}

// ============================================================
// 受け止め演出の更新
// ============================================================
void PlayerShield::UpdateGuardImpact(float deltaTime) {
  if (guardImpactTimer_ <= 0.0f)
    return;

  guardImpactTimer_ -= deltaTime;
  if (guardImpactTimer_ < 0.0f) {
    guardImpactTimer_ = 0.0f;
  }
}

// ============================================================
// 受け止め演出によるスケール倍率
//
// 減衰する波（sin × 残り時間）で「変形 → 跳ね返り → 静止」と動かす。
// 単純な山だと一度膨らんで戻るだけで目に留まりにくいため、
// bounce を 1.5 以上にして戻りぎわに逆向きの振れを入れている。
//
//   weight:  0 → +1（変形）→ 0 →（bounce>1なら）マイナス側へ小さく振れて 0
//
// axis は軸ごとの効き方。正で伸び、負で縮む。
//   {0.6, 0.6, -1.0} … 面方向に広がり厚みが潰れる（受け止めた形）
//   {1, 1, 1}        … 単純に大きくなる（漫画的なポップ）
// ============================================================
Vector3 PlayerShield::GetGuardImpactScale() const {
  if (guardImpactTimer_ <= 0.0f || guardImpactDuration_ <= 0.0f) {
    return {1.0f, 1.0f, 1.0f};
  }

  const float progress = 1.0f - (guardImpactTimer_ / guardImpactDuration_);
  const float wave =
      std::sin(progress * std::numbers::pi_v<float> * guardImpactBounce_);
  const float damping = 1.0f - progress; // 進むほど振れ幅を小さくする
  const float weight = wave * damping;

  // スケールが0以下に潰れると裏返って描画が壊れるので下限を設ける
  auto apply = [&](float axisWeight) {
    return std::max(0.05f, 1.0f + guardImpactAmount_ * axisWeight * weight);
  };

  return {apply(guardImpactAxis_.x), apply(guardImpactAxis_.y),
          apply(guardImpactAxis_.z)};
}

// ============================================================
// プレイヤーの手ジョイントを探索
// ============================================================
void PlayerShield::FindHandJointIndex() {
  if (!obj3d_)
    return;

  auto *skeleton = obj3d_->GetModel()->GetSkeleton();
  if (!skeleton)
    return;

  auto &jointMap = skeleton->GetJointMap();
  auto it = jointMap.find(handJointName_);

  if (it != jointMap.end()) {
    handleIndex_ = it->second;
    isValidJoint_ = true;
  } else {
    isValidJoint_ = false;

    std::vector<std::string> handCandidates = {"mixamorig:RightHand",
                                               "mixamorig:LeftHand",
                                               "RightHand",
                                               "LeftHand",
                                               "Hand_R",
                                               "Hand_L"};

    for (const auto &candidate : handCandidates) {
      auto candidateIt = jointMap.find(candidate);
      if (candidateIt != jointMap.end()) {
        handJointName_ = candidate;
        handleIndex_ = candidateIt->second;
        isValidJoint_ = true;
        break;
      }
    }
  }
}

// ============================================================
// 手のジョイント位置に盾を追従させる
// ============================================================
void PlayerShield::SetPlayerWeaponPosition() {
  if (!obj3d_ || !isValidJoint_)
    return;

  wt_.translate_ = offsetPos_;
  wt_.rotate_ = offsetRot_;
  // 受け止め演出中は基準スケールに倍率を掛ける
  const Vector3 impact = GetGuardImpactScale();
  wt_.scale_ = {offsetScale_.x * impact.x, offsetScale_.y * impact.y,
                offsetScale_.z * impact.z};

  YoRigine::WorldTransform &handWT = obj3d_->GetModel()
                                         ->GetSkeleton()
                                         ->GetJoints()[handleIndex_]
                                         .GetWorldTransform();

  const Matrix4x4 handNoScale = RemoveScaleFromMatrix(handWT.matWorld_);
  const Matrix4x4 shieldMatrix =
      MakeAffineMatrix(wt_.scale_, wt_.rotate_, wt_.translate_);
  wt_.matWorld_ = Multiply(shieldMatrix, handNoScale);
}

// ============================================================
// 描画処理
// ============================================================
void PlayerShield::Draw() {
  if (!isVisible_)
    return;
  if (obj_) {
    obj_->Draw(camera_, wt_);
  }
}

// ============================================================
// 影の描画
// ============================================================
void PlayerShield::DrawShadow() {
  if (!isVisible_)
    return;
  if (obj_) {
    obj_->DrawShadow(wt_);
  }
}

// ============================================================
// コライダー描画
// ============================================================
void PlayerShield::DrawCollision() {
  if (obbCollider_) {
    obbCollider_->Draw();
  }
}

// ============================================================
// 衝突開始時の処理
// ============================================================
void PlayerShield::OnEnterCollision([[maybe_unused]] BaseCollider *self,
                                    BaseCollider *other) {
  // ガードの成否判定はここでは行わない。
  // 盾のコライダーと本体のコライダーはどちらが先に処理されるか決まっておらず、
  // 両方で判定すると結果が不定になるため、判定は Player::ApplyDamage()
  // （＝ダメージ処理の入口）に一本化している。
  (void)other;
}

// ============================================================
// 衝突中の処理
// ============================================================
void PlayerShield::OnCollision([[maybe_unused]] BaseCollider *self,
                               BaseCollider *other) {
  // 上記と同じ理由でここでも判定しない。
  // 接触中は毎フレーム呼ばれるので、以前はガード失敗が連続発火していた。
  (void)other;
}

// ============================================================
// 衝突終了時の処理
// ============================================================
void PlayerShield::OnExitCollision([[maybe_unused]] BaseCollider *self,
                                   [[maybe_unused]] BaseCollider *other) {}

// ============================================================
// 衝突方向ごとの処理
// ============================================================
void PlayerShield::OnDirectionCollision([[maybe_unused]] BaseCollider *self,
                                        [[maybe_unused]] BaseCollider *other,
                                        [[maybe_unused]] HitDirection dir) {}

// ============================================================
// 衝突方向開始時の処理
// ============================================================
void PlayerShield::OnEnterDirectionCollision(
    [[maybe_unused]] BaseCollider *self, [[maybe_unused]] BaseCollider *other,
    [[maybe_unused]] HitDirection dir) {}

// ============================================================
// コライダー初期化
// ============================================================
void PlayerShield::InitCollision() {
  obbCollider_ = ColliderFactory::Create<OBBCollider>(
      this, &wt_, camera_,
      static_cast<uint32_t>(CollisionTypeIdDef::kPlayerShield));

  obbCollider_->SetEnablePenetration(false);
}

// ============================================================
// Json登録処理
// ============================================================
void PlayerShield::InitJson() {
  // ------------------------------------------------------------
  // メイン設定
  // ------------------------------------------------------------
  jsonManager_ = std::make_unique<YoRigine::JsonManager>(
      "PlayerShield", "Resources/Json/Weapon");
  jsonManager_->SetCategory("Objects");
  jsonManager_->SetSubCategory("PlayerShield");
  jsonManager_->Register("Translation", &wt_.translate_);
  jsonManager_->Register("Rotate", &wt_.rotate_);
  jsonManager_->Register("Scale", &wt_.scale_);
  jsonManager_->Register("Use Anchor Point", &wt_.useAnchorPoint_);
  jsonManager_->Register("AnchorPoint", &wt_.anchorPoint_);
  jsonManager_->Register("Hand Joint Name", &handJointName_);

  // ------------------------------------------------------------
  // オフセット設定（手元からの補正位置）
  // ------------------------------------------------------------
  jsonManager_->Register("Offset Position", &offsetPos_);
  jsonManager_->Register("Offset Rotation", &offsetRot_);
  jsonManager_->Register("Offset Scale", &offsetScale_);

  // ------------------------------------------------------------
  // コライダー設定
  // ------------------------------------------------------------
  jsonCollider_ = std::make_unique<YoRigine::JsonManager>(
      "PlayerShieldCollider", "Resources/Json/Colliders");
  obbCollider_->InitJson(jsonCollider_.get());
}
