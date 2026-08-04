#pragma once

#include "EnemyProjectile.h"
#include "ProjectileDefinition.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace YoRigine {
class Camera;
}
class Player;

// ============================================================
// 敵の弾の管理
//
// 弾の定義（どう飛ぶか）と実体（今飛んでいるもの）を持つ。
// 弾は敵とは独立して飛ぶので、撃った敵が死んでも残る。
//
// 当たり判定は CollisionManager を通さず、プレイヤーとの
// 距離判定だけで済ませている。弾は数が多く形も単純なので、
// コライダーを毎回作るより軽く、寿命管理も単純になる。
// ============================================================
class ProjectileManager {
public:
  // 現在アクティブなマネージャ（攻撃ステートから撃つために引く）
  static ProjectileManager *GetCurrent() { return current_; }
  static void SetCurrent(ProjectileManager *manager) { current_ = manager; }

  void Initialize(YoRigine::Camera *camera, Player *player);
  void Finalize();

  void Update(float deltaTime);
  void Draw();

  // ── 弾の定義 ──
  bool LoadDefinitions(const std::string &path);
  bool SaveDefinitions(const std::string &path) const;
  const ProjectileDefinition *FindDefinition(const std::string &id) const;

  std::vector<ProjectileDefinition> &GetDefinitions() { return definitions_; }
  const std::vector<ProjectileDefinition> &GetDefinitions() const {
    return definitions_;
  }

  /// <summary>
  /// 弾を1発撃つ
  /// </summary>
  /// <param name="id">弾の定義ID</param>
  /// <param name="position">発射位置</param>
  /// <param name="direction">発射方向（正規化されていなくてよい）</param>
  void Spawn(const std::string &id, const Vector3 &position,
             const Vector3 &direction);

  // 全部消す（戦闘終了時など）
  void Clear() { projectiles_.clear(); }

  size_t GetActiveCount() const { return projectiles_.size(); }

private:
  // プレイヤーとの当たり判定
  void CheckHits();

  // 寿命切れ・着弾済みを片付ける
  void RemoveDead();

private:
  static ProjectileManager *current_;

  YoRigine::Camera *camera_ = nullptr;
  Player *player_ = nullptr;

  std::vector<ProjectileDefinition> definitions_;
  std::vector<std::unique_ptr<EnemyProjectile>> projectiles_;

  // 弾がプレイヤーに当たったと見なす距離に足す余裕
  float playerHitRadius_ = 0.8f;
};
