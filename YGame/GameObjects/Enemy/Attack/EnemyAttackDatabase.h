#pragma once

#include "EnemyAttackAction.h"

#include <string>
#include <unordered_map>
#include <vector>

class BaseEnemy;
struct EnemyAIContext;
struct PerceptionParams;

// ============================================================
// 攻撃データの保管庫
//
// JSONから読んだ EnemyAttackAction を全部持ち、IDで引けるようにする。
// 攻撃Stateがデータを直接持たないので、エディタで数値を変えると
// 次に選ばれた攻撃から即座に反映される。
// ============================================================
class EnemyAttackDatabase {
public:
  static EnemyAttackDatabase &GetInstance();

  bool Load(const std::string &path = EnemyAttackActionIO::kDefaultPath);
  bool Save(const std::string &path = EnemyAttackActionIO::kDefaultPath) const;

  const EnemyAttackAction *Find(const std::string &id) const;

  const std::vector<EnemyAttackAction> &GetAll() const { return actions_; }
  std::vector<EnemyAttackAction> &GetAllMutable() { return actions_; }

  // データ駆動の攻撃を使うか。
  // false の間は従来の攻撃Stateクラスがそのまま使われる。
  // 移行中に「元の挙動と比べる」ためのスイッチ。
  static bool IsEnabled() { return enabled_; }
  static void SetEnabled(bool value) { enabled_ = value; }
  static bool *GetEnabledPtr() { return &enabled_; }

private:
  static bool enabled_;
  std::vector<EnemyAttackAction> actions_;
};

// ============================================================
// 攻撃の選択
//
// 従来は攻撃種別ごとに手書きしたフィールドと4連のswitchで重みを
// 出していたため、攻撃を1つ足すたびに共有コードを触る必要があった。
// 条件はすべて EnemyAttackAction 側のデータなので、ここは
// 「条件で絞って重みで抽選する」だけになっている。
// ============================================================
namespace EnemyAttackPicker {

/// <summary>
/// 条件を満たす攻撃の中から重み付きで1つ選ぶ。
/// 選べるものが無ければ nullptr。
/// </summary>
/// <param name="phase">ボスのフェーズ番号。フェーズ制でなければ0</param>
const EnemyAttackAction *Pick(const BaseEnemy &enemy, const EnemyAIContext &ctx,
                              const PerceptionParams &perception,
                              int phase = 0);

} // namespace EnemyAttackPicker
