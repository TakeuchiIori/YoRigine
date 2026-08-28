#pragma once

#include "../Data/EnemyAttack.h"

#include <string>
#include <unordered_map>
#include <vector>

class BaseEnemy;
struct EnemyAIContext;
struct PerceptionParams;

// ============================================================
// 攻撃データの保管庫
//
// カーブで定義された攻撃を全部持ち、IDで引けるようにする。
// 攻撃ステートはデータを所有せず毎回ここから引くので、
// エディタで数値を変えると次の攻撃から即座に反映される。
// ============================================================
class EnemyAttackLibrary {
public:
	static EnemyAttackLibrary &GetInstance();

	bool Load(const std::string &path = EnemyAttackIO::kDefaultPath);
	bool Save(const std::string &path = EnemyAttackIO::kDefaultPath) const;

	const EnemyAttack *Find(const std::string &id) const;

	std::vector<EnemyAttack> &GetAll() { return attacks_; }
	const std::vector<EnemyAttack> &GetAll() const { return attacks_; }

	// 新規追加・複製・削除（エディタ用）
	int Add(const std::string &id);
	int Duplicate(int index);
	void Remove(int index);
	std::string MakeUniqueId(const std::string &base) const;

	// カーブ定義の攻撃を使うか。
	// false の間は従来のハードコード攻撃ステートが使われる。
	// 移行中に元の挙動と比べるためのスイッチ。
	static bool IsEnabled() { return enabled_; }
	static void SetEnabled(bool value) { enabled_ = value; }
	static bool *GetEnabledPtr() { return &enabled_; }

private:
	static bool enabled_;
	std::vector<EnemyAttack> attacks_;
};

// ============================================================
// 攻撃の選択
//
// 「その敵が持っている技」の中から、条件を満たすものを重みで抽選する。
// 敵ごとの個性は数値の違いではなく、持っている技の違いで表現する。
// ============================================================
namespace EnemyAttackPicker {

/// <summary>
/// 条件を満たす攻撃から重み付きで1つ選ぶ。無ければ nullptr。
/// </summary>
/// <param name="attackIds">この敵が使える攻撃ID。空なら全攻撃が候補</param>
const EnemyAttack *Pick(const BaseEnemy &enemy, const std::vector<std::string> &attackIds,
                        const EnemyAIContext &ctx, const PerceptionParams &perception);

} // namespace EnemyAttackPicker
