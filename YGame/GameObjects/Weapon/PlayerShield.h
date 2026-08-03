#pragma once
#include "Collision/Core/CollisionDirection.h"
#include "Collision/OBB/OBBCollider.h"
#include "Loaders/Json/JsonManager.h"
#include "Object3D/Object3d.h"
#include "Object3d/BaseObject.h"
#include "Systems/Camera/Camera.h"
#include "WorldTransform/WorldTransform.h"

class Player;

// ============================================================
// プレイヤー盾クラス
// プレイヤーの腕のボーンに追従し、ガードやパリィ判定のコライダーを提供する
// ============================================================
class PlayerShield : public YoRigine::BaseObject {
public:
  // ============================================================
  // 初期化と更新処理
  // ============================================================
  ~PlayerShield();
  void Initialize(YoRigine::Camera *camera) override;

  void Update() override;

  void Draw() override;
  void DrawShadow();
  void DrawCollision() override;

  // ============================================================
  // アクセッサ・状態操作
  // ============================================================
  void SetPlayer(Player *player) { player_ = player; }
  void SetObject(YoRigine::Object3d *obj3d) { obj3d_ = obj3d; }
  bool IsJointValid() const { return isValidJoint_; }
  YoRigine::WorldTransform &GetWorldTransform() { return wt_; }

  void SetEnableCollider(bool enable) {
    obbCollider_->SetCollisionEnabled(enable);
  }

  // 盾の表示切替。魔法スタイル時は防御できないため非表示にする。
  void SetVisible(bool visible) { isVisible_ = visible; }

  /// <summary>
  /// ガード／パリィで受け止めたときのスケール変化。
  /// 一度大きく変形してから跳ね返って戻る「パンチ」として動かす。
  /// 単純に膨らませて戻すだけでは目に留まらないため、減衰する波を使う。
  /// </summary>
  /// <param name="amount">変化量（0で無効）</param>
  /// <param name="duration">元に戻るまでの秒数</param>
  /// <param name="axis">軸ごとの効き方。正で伸び、負で縮む</param>
  /// <param
  /// name="bounce">波の回数。1.5以上で戻りぎわに逆向きの跳ね返りが入る</param>
  void PlayGuardImpact(float amount, float duration, const Vector3 &axis,
                       float bounce);
  bool IsVisible() const { return isVisible_; }

  // ============================================================
  // 当たり判定コールバック
  // ============================================================
  void OnEnterCollision([[maybe_unused]] BaseCollider *self,
                        BaseCollider *other);
  void OnCollision([[maybe_unused]] BaseCollider *self, BaseCollider *other);
  void OnExitCollision([[maybe_unused]] BaseCollider *self,
                       [[maybe_unused]] BaseCollider *other);
  void OnDirectionCollision([[maybe_unused]] BaseCollider *self,
                            [[maybe_unused]] BaseCollider *other,
                            [[maybe_unused]] HitDirection dir);
  void OnEnterDirectionCollision([[maybe_unused]] BaseCollider *self,
                                 BaseCollider *other,
                                 [[maybe_unused]] HitDirection dir);

private:
  // ============================================================
  // 内部処理
  // ============================================================
  void InitCollision() override;
  void InitJson() override;

  void FindHandJointIndex();
  void SetPlayerWeaponPosition();

  // 受け止め演出の経過を進める
  void UpdateGuardImpact(float deltaTime);

  // 受け止め演出によるスケール倍率（演出中でなければ等倍）
  Vector3 GetGuardImpactScale() const;

private:
  // ============================================================
  // メンバ変数
  // ============================================================

  // ------------------------------------------------------------
  // システム連携・参照
  // ------------------------------------------------------------
  YoRigine::Camera *camera_ = nullptr; // 描画に使用するカメラ
  Player *player_ = nullptr;           // 盾を装備しているプレイヤー本体
  YoRigine::Object3d *obj3d_ =
      nullptr; // プレイヤーの3Dモデル（ジョイント探索用）

  // ------------------------------------------------------------
  // パーティクル
  // ------------------------------------------------------------

  // ------------------------------------------------------------
  // ジョイントアタッチ関連
  // ------------------------------------------------------------
  std::string handJointName_ =
      "mixamorig:LeftHand";   // アタッチ先の手ジョイント名
  int handleIndex_ = 0;       // ジョイントの配列インデックス
  bool isValidJoint_ = false; // 正しいジョイントが見つかったか

  Vector3 offsetPos_{};                   // 手からの位置オフセット
  Vector3 offsetRot_{};                   // 手からの回転オフセット
  Vector3 offsetScale_{1.0f, 1.0f, 1.0f}; // 盾のスケール

  bool isVisible_ = true; // false の間は描画しない（魔法スタイル時に外す）

  // ------------------------------------------------------------
  // 受け止め演出（ガード／パリィ）
  // ------------------------------------------------------------
  float guardImpactAmount_ = 0.0f;            // 変化量
  float guardImpactTimer_ = 0.0f;             // 残り時間
  float guardImpactDuration_ = 0.0f;          // 全体時間
  Vector3 guardImpactAxis_{1.0f, 1.0f, 1.0f}; // 軸ごとの効き方
  float guardImpactBounce_ = 1.6f;            // 波の回数
};