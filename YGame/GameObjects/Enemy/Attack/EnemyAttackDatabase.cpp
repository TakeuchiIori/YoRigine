#include "EnemyAttackDatabase.h"

#include "../AI/EnemyAIContext.h"
#include "../BaseEnemy.h"

#include <random>

bool EnemyAttackDatabase::enabled_ = false;

EnemyAttackDatabase &EnemyAttackDatabase::GetInstance() {
  static EnemyAttackDatabase instance;
  return instance;
}

bool EnemyAttackDatabase::Load(const std::string &path) {
  return EnemyAttackActionIO::Load(path, actions_);
}

bool EnemyAttackDatabase::Save(const std::string &path) const {
  return EnemyAttackActionIO::Save(path, actions_);
}

const EnemyAttackAction *
EnemyAttackDatabase::Find(const std::string &id) const {
  for (const auto &action : actions_) {
    if (action.id == id)
      return &action;
  }
  return nullptr;
}

namespace {

float RandomRange(float min, float max) {
  if (max <= min)
    return min;
  static std::random_device rd;
  static std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dist(min, max);
  return dist(gen);
}

} // namespace

const EnemyAttackAction *
EnemyAttackPicker::Pick(const BaseEnemy &enemy, const EnemyAIContext &ctx,
                        const PerceptionParams &perception, int phase) {
  const auto &actions = EnemyAttackDatabase::GetInstance().GetAll();
  if (actions.empty())
    return nullptr;

  // ------------------------------------------------------------
  // 条件で候補を絞り、同時に重みを計算する
  // ------------------------------------------------------------
  std::vector<const EnemyAttackAction *> candidates;
  std::vector<float> weights;
  float total = 0.0f;

  const bool hasOpening = ctx.HasOpening(perception);

  for (const auto &action : actions) {
    // 重み0の攻撃は抽選対象外（カウンターのように専用経路で出すもの）
    if (action.weight <= 0.0f)
      continue;

    if (action.phaseGate >= 0 && action.phaseGate != phase)
      continue;
    if (ctx.distance < action.minRange)
      continue;
    if (ctx.distance > action.maxRange)
      continue;
    if (ctx.selfHpRatio > action.selfHpBelow)
      continue;
    if (ctx.playerHpRatio > action.targetHpBelow)
      continue;

    float weight = action.weight;

    // プレイヤーの状態による補正。
    // 隙を見つけたら発生の速い技で差し込み、溜め技は避ける。
    if (perception.enabled && ctx.hasPlayer) {
      if (hasOpening) {
        weight *= action.fast ? perception.openingFastAttackWeight
                              : perception.openingSlowAttackWeight;
      } else if (ctx.playerIsDodging && !action.fast) {
        // 回避を空振りさせるため、あえて溜め技を選ぶ
        weight *= perception.baitSlowAttackWeight;
      }
    }

    if (weight <= 0.0f)
      continue;

    candidates.push_back(&action);
    weights.push_back(weight);
    total += weight;
  }

  if (candidates.empty() || total <= 0.0f) {
    (void)enemy;
    return nullptr;
  }

  // ------------------------------------------------------------
  // 重み付き抽選
  // ------------------------------------------------------------
  float roll = RandomRange(0.0f, total);
  for (size_t i = 0; i < candidates.size(); ++i) {
    roll -= weights[i];
    if (roll <= 0.0f)
      return candidates[i];
  }
  return candidates.back();
}
