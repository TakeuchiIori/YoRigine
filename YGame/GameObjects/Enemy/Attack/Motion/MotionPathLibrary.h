#pragma once

#include "IAttackMotion.h"

#include <memory>
#include <string>
#include <vector>

// ============================================================
// 名前付きの経路ライブラリ
//
// 経路を攻撃データの中に直接書くと、同じ動きを別の攻撃で
// 使い回せず、エディタで一覧することもできない。
// 「突進」「回り込み」「山なりの跳躍」を名前で登録しておき、
// 攻撃や弾はその名前を参照する形にする。
// ============================================================
struct MotionPathEntry {
  std::string name;
  std::shared_ptr<IAttackMotion> motion;
};

class MotionPathLibrary {
public:
  static MotionPathLibrary &GetInstance();

  bool Load(const std::string &path = kDefaultPath);
  bool Save(const std::string &path = kDefaultPath) const;

  // 名前で経路を引く（見つからなければ nullptr）
  const IAttackMotion *Find(const std::string &name) const;
  std::shared_ptr<IAttackMotion> FindShared(const std::string &name) const;

  std::vector<MotionPathEntry> &GetAll() { return entries_; }
  const std::vector<MotionPathEntry> &GetAll() const { return entries_; }

  // 新しい経路を追加して、そのインデックスを返す
  int Add(const std::string &name, const std::string &typeName);
  int Duplicate(int index);
  void Remove(int index);

  // 名前の重複を避けた一意な名前を作る
  std::string MakeUniqueName(const std::string &base) const;

  static inline const char *kDefaultPath =
      "Resources/Json/BattleEnemies/motion_paths.json";

private:
  std::vector<MotionPathEntry> entries_;
};
