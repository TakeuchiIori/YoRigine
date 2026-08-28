#pragma once

#include "IAttackMotion.h"

#include <json.hpp>
#include <memory>

// ============================================================
// 経路の生成と保存
//
// 新しい経路の種類を足すときはここに1行追加するだけで、
// 攻撃データ側やランナー側は触らなくてよい。
// ============================================================
namespace AttackMotionFactory {

/// <summary>
/// JSON から経路を作る。type が未知なら nullptr。
/// </summary>
std::unique_ptr<IAttackMotion> Create(const nlohmann::json &j);

/// <summary>
/// 経路を JSON へ書き出す
/// </summary>
nlohmann::json ToJson(const IAttackMotion &motion);

/// <summary>
/// 指定の種類の経路を既定値で作る（エディタで新規追加するとき用）
/// </summary>
std::unique_ptr<IAttackMotion> CreateDefault(const std::string &typeName);

/// <summary>
/// 選択肢として出せる経路の種類名
/// </summary>
const std::vector<std::string> &GetTypeNames();

} // namespace AttackMotionFactory
