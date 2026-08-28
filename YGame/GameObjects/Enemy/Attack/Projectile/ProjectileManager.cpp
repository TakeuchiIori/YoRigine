#include "ProjectileManager.h"

#include "../Motion/AttackMotionFactory.h"
#include "Player/Player.h"

#include <Debugger/Logger.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>

using json = nlohmann::json;

ProjectileManager *ProjectileManager::current_ = nullptr;

const char *ProjectileMotionModeToString(ProjectileMotionMode mode) {
  return (mode == ProjectileMotionMode::Path) ? "path" : "straight";
}

ProjectileMotionMode ProjectileMotionModeFromString(const std::string &name) {
  return (name == "path") ? ProjectileMotionMode::Path
                          : ProjectileMotionMode::Straight;
}

// ============================================================
// 初期化・終了
// ============================================================
void ProjectileManager::Initialize(YoRigine::Camera *camera, Player *player) {
  camera_ = camera;
  player_ = player;
  projectiles_.clear();
  current_ = this;
}

void ProjectileManager::Finalize() {
  projectiles_.clear();
  camera_ = nullptr;
  player_ = nullptr;
  if (current_ == this)
    current_ = nullptr;
}

// ============================================================
// 更新
// ============================================================
void ProjectileManager::Update(float deltaTime) {
  for (auto &projectile : projectiles_) {
    projectile->Update(deltaTime);
  }

  CheckHits();
  RemoveDead();
}

void ProjectileManager::Draw() {
  for (auto &projectile : projectiles_) {
    projectile->Draw(camera_);
  }
}

// ============================================================
// 当たり判定
//
// 弾は数が多く形も球なので、コライダーを登録せず距離だけで判定する。
// ============================================================
void ProjectileManager::CheckHits() {
  if (!player_ || !player_->IsAlive())
    return;

  // 胴の高さを狙う。足元の座標のままだと当たりにくい。
  Vector3 playerCenter = player_->GetWorldPosition();
  playerCenter.y += 1.0f;

  for (auto &projectile : projectiles_) {
    if (!projectile->IsAlive())
      continue;

    const float hitDistance = projectile->GetRadius() + playerHitRadius_;
    if (Length(projectile->GetPosition() - playerCenter) > hitDistance)
      continue;

    // ダメージ処理はプレイヤー側の入口へ通す。
    // こうするとガード・パリィがそのまま弾にも効く。
    player_->ApplyDamage(projectile->GetDamage(), projectile->GetPosition());
    projectile->OnHit();
  }
}

void ProjectileManager::RemoveDead() {
  projectiles_.erase(
      std::remove_if(projectiles_.begin(), projectiles_.end(),
                     [](const std::unique_ptr<EnemyProjectile> &p) {
                       return !p->IsAlive();
                     }),
      projectiles_.end());
}

// ============================================================
// 発射
// ============================================================
void ProjectileManager::Spawn(const std::string &id, const Vector3 &position,
                              const Vector3 &direction) {
  const ProjectileDefinition *definition = FindDefinition(id);
  if (!definition) {
    Logger(
        ("[ProjectileManager] 弾の定義が見つかりません: " + id + "\n").c_str());
    return;
  }

  Vector3 dir = direction;
  if (Length(dir) < 0.0001f) {
    dir = {0.0f, 0.0f, 1.0f};
  }
  dir = Normalize(dir);

  projectiles_.push_back(
      std::make_unique<EnemyProjectile>(*definition, position, dir, player_));
}

const ProjectileDefinition *
ProjectileManager::FindDefinition(const std::string &id) const {
  for (const auto &definition : definitions_) {
    if (definition.id == id)
      return &definition;
  }
  return nullptr;
}

// ============================================================
// 定義の読み込み
// ============================================================
bool ProjectileManager::LoadDefinitions(const std::string &path) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    Logger(("[ProjectileManager] 弾定義ファイルがありません: " + path + "\n")
               .c_str());
    return false;
  }

  try {
    json j = json::parse(ifs);
    if (!j.contains("projectiles") || !j["projectiles"].is_array())
      return false;

    definitions_.clear();
    for (const auto &pj : j["projectiles"]) {
      ProjectileDefinition d;
      d.id = pj.value("id", std::string());
      if (d.id.empty())
        continue;

      d.mode = ProjectileMotionModeFromString(
          pj.value("mode", std::string("straight")));
      d.speed = pj.value("speed", 12.0f);
      d.gravity = pj.value("gravity", 0.0f);
      d.homing = pj.value("homing", 0.0f);

      if (pj.contains("path")) {
        d.path = AttackMotionFactory::Create(pj["path"]);
      }

      d.lifeTime = pj.value("lifeTime", 3.0f);
      d.radius = pj.value("radius", 0.5f);
      d.damage = pj.value("damage", 10);
      d.destroyOnHit = pj.value("destroyOnHit", true);

      d.modelPath = pj.value("modelPath", std::string());
      if (pj.contains("scale")) {
        const auto &s = pj["scale"];
        d.scale = {s.value("x", 0.5f), s.value("y", 0.5f), s.value("z", 0.5f)};
      }
      if (pj.contains("color")) {
        const auto &c = pj["color"];
        d.color = {c.value("x", 1.0f), c.value("y", 0.6f), c.value("z", 0.2f),
                   c.value("w", 1.0f)};
      }
      d.trailVfxName = pj.value("trailVfxName", std::string());
      d.hitVfxName = pj.value("hitVfxName", std::string());
      d.faceVelocity = pj.value("faceVelocity", true);

      definitions_.push_back(std::move(d));
    }

    Logger(("[ProjectileManager] " + std::to_string(definitions_.size()) +
            "件の弾定義を読み込みました\n")
               .c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[ProjectileManager] 読み込み失敗: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}

// ============================================================
// 定義の書き出し
// ============================================================
bool ProjectileManager::SaveDefinitions(const std::string &path) const {
  json j;
  json array = json::array();

  for (const auto &d : definitions_) {
    json pj;
    pj["id"] = d.id;
    pj["mode"] = ProjectileMotionModeToString(d.mode);
    pj["speed"] = d.speed;
    pj["gravity"] = d.gravity;
    pj["homing"] = d.homing;
    if (d.path)
      pj["path"] = AttackMotionFactory::ToJson(*d.path);

    pj["lifeTime"] = d.lifeTime;
    pj["radius"] = d.radius;
    pj["damage"] = d.damage;
    pj["destroyOnHit"] = d.destroyOnHit;

    pj["modelPath"] = d.modelPath;
    pj["scale"] = {{"x", d.scale.x}, {"y", d.scale.y}, {"z", d.scale.z}};
    pj["color"] = {
        {"x", d.color.x}, {"y", d.color.y}, {"z", d.color.z}, {"w", d.color.w}};
    pj["trailVfxName"] = d.trailVfxName;
    pj["hitVfxName"] = d.hitVfxName;
    pj["faceVelocity"] = d.faceVelocity;

    array.push_back(pj);
  }
  j["projectiles"] = array;

  try {
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
      std::filesystem::create_directories(filePath.parent_path());
    }
    std::ofstream ofs(path);
    if (!ofs.is_open())
      return false;
    ofs << std::setw(4) << j;
    return true;
  } catch (const std::exception &e) {
    Logger(("[ProjectileManager] 保存失敗: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}
