#include "EnemyAttack.h"

#include <Debugger/Logger.h>

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>

using json = nlohmann::json;

bool EnemyAttack::HasHitbox() const {
  for (const auto &modifier : modifiers) {
    if (modifier.type == AttackModifierType::Hitbox)
      return true;
  }
  return false;
}

namespace {

// ── 補間モード ──
const char *InterpModeToString(InterpolationMode mode) {
  switch (mode) {
  case InterpolationMode::Step:
    return "step";
  case InterpolationMode::CatmullRom:
    return "catmullRom";
  case InterpolationMode::Bezier:
    return "bezier";
  default:
    return "linear";
  }
}

InterpolationMode InterpModeFromString(const std::string &name) {
  if (name == "step")
    return InterpolationMode::Step;
  if (name == "catmullRom")
    return InterpolationMode::CatmullRom;
  if (name == "bezier")
    return InterpolationMode::Bezier;
  return InterpolationMode::Linear;
}

// ── カーブ1本 ──
// キーが無いチャンネルは書き出さない。JSONが9本分の空配列で埋まるのを防ぐ。
json CurveToJson(const CurveChannel &curve) {
  json keys = json::array();
  for (const auto &key : curve.GetKeys()) {
    json k;
    k["time"] = key.time;
    k["value"] = key.value;
    k["mode"] = InterpModeToString(key.interpMode);
    if (key.interpMode == InterpolationMode::Bezier) {
      k["inTangent"] = key.inTangent;
      k["outTangent"] = key.outTangent;
    }
    keys.push_back(k);
  }
  return keys;
}

void CurveFromJson(const json &keysJson, CurveChannel &curve) {
  curve.Clear();
  if (!keysJson.is_array())
    return;

  for (const auto &k : keysJson) {
    curve.AddKey(k.value("time", 0.0f), k.value("value", 0.0f),
                 InterpModeFromString(k.value("mode", std::string("linear"))),
                 k.value("inTangent", 0.0f), k.value("outTangent", 0.0f));
  }
}

// ── モディファイア ──
json ModifierToJson(const AttackModifier &m) {
  json j;
  j["type"] = AttackModifierTypeToString(m.type);
  j["startTime"] = m.startTime;
  if (!m.IsInstant())
    j["endTime"] = m.endTime;

  switch (m.type) {
  case AttackModifierType::FaceTarget:
  case AttackModifierType::HomingOffset:
    j["strength"] = m.strength;
    break;
  case AttackModifierType::Hitbox:
    j["damageWindow"] = m.damageWindow;
    break;
  case AttackModifierType::EmitProjectile:
    j["projectileId"] = m.projectileId;
    j["count"] = m.count;
    j["spreadDeg"] = m.spreadDeg;
    j["offset"] = {{"x", m.offset.x}, {"y", m.offset.y}, {"z", m.offset.z}};
    j["aimAtTarget"] = m.aimAtTarget;
    break;
  default:
    break;
  }
  return j;
}

AttackModifier ModifierFromJson(const json &j) {
  AttackModifier m;
  m.type = AttackModifierTypeFromString(j.value("type", std::string("hitbox")));
  m.startTime = j.value("startTime", 0.0f);
  m.endTime = j.value("endTime", m.startTime);

  m.strength = j.value("strength", 6.0f);
  m.damageWindow = j.value("damageWindow", 0);

  m.projectileId = j.value("projectileId", std::string());
  m.count = j.value("count", 1);
  m.spreadDeg = j.value("spreadDeg", 0.0f);
  if (j.contains("offset")) {
    const auto &o = j["offset"];
    m.offset = {o.value("x", 0.0f), o.value("y", 1.0f), o.value("z", 0.0f)};
  }
  m.aimAtTarget = j.value("aimAtTarget", true);
  return m;
}

} // namespace

// ============================================================
// 読み込み
// ============================================================
bool EnemyAttackIO::Load(const std::string &path,
                         std::vector<EnemyAttack> &out) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    Logger(("[EnemyAttack] ファイルがありません: " + path + "\n").c_str());
    return false;
  }

  try {
    json j = json::parse(ifs);
    if (!j.contains("attacks") || !j["attacks"].is_array()) {
      Logger("[EnemyAttack] 'attacks' 配列がありません\n");
      return false;
    }

    out.clear();
    for (const auto &aj : j["attacks"]) {
      EnemyAttack attack;
      attack.id = aj.value("id", std::string());
      if (attack.id.empty())
        continue; // IDの無いエントリは無視

      attack.displayName = aj.value("displayName", attack.id);
      attack.duration = aj.value("duration", 1.5f);

      attack.positionSource = AttackPositionSourceFromString(
          aj.value("positionSource", std::string("curves")));
      attack.pathName = aj.value("pathName", std::string());

      // カーブ。書かれているチャンネルだけ読む。
      if (aj.contains("tracks")) {
        const auto &tracksJson = aj["tracks"];
        for (size_t i = 0; i < static_cast<size_t>(AttackChannel::Count); ++i) {
          const auto channel = static_cast<AttackChannel>(i);
          const char *name = AttackChannelToString(channel);
          if (!tracksJson.contains(name))
            continue;
          CurveFromJson(tracksJson[name], attack.tracks.GetChannel(channel));
        }
      }

      if (aj.contains("modifiers") && aj["modifiers"].is_array()) {
        for (const auto &mj : aj["modifiers"]) {
          attack.modifiers.push_back(ModifierFromJson(mj));
        }
      }

      attack.minRange = aj.value("minRange", 0.0f);
      attack.maxRange = aj.value("maxRange", 999.0f);
      attack.weight = aj.value("weight", 1.0f);
      attack.cooldown = aj.value("cooldown", 0.0f);
      attack.selfHpBelow = aj.value("selfHpBelow", 1.0f);
      attack.targetHpBelow = aj.value("targetHpBelow", 1.0f);
      attack.parriable = aj.value("parriable", false);
      attack.fast = aj.value("fast", true);

      out.push_back(std::move(attack));
    }

    Logger(("[EnemyAttack] " + std::to_string(out.size()) +
            "件の攻撃を読み込みました\n")
               .c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[EnemyAttack] 読み込み失敗: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}

// ============================================================
// 書き出し
// ============================================================
bool EnemyAttackIO::Save(const std::string &path,
                         const std::vector<EnemyAttack> &attacks) {
  json j;
  json array = json::array();

  for (const auto &attack : attacks) {
    json aj;
    aj["id"] = attack.id;
    aj["displayName"] = attack.displayName;
    aj["duration"] = attack.duration;
    aj["positionSource"] = AttackPositionSourceToString(attack.positionSource);
    if (!attack.pathName.empty())
      aj["pathName"] = attack.pathName;

    // キーのあるチャンネルだけ書く
    json tracksJson = json::object();
    for (size_t i = 0; i < static_cast<size_t>(AttackChannel::Count); ++i) {
      const auto channel = static_cast<AttackChannel>(i);
      if (!attack.tracks.HasKeys(channel))
        continue;
      tracksJson[AttackChannelToString(channel)] =
          CurveToJson(attack.tracks.GetChannel(channel));
    }
    aj["tracks"] = tracksJson;

    json modifiers = json::array();
    for (const auto &modifier : attack.modifiers) {
      modifiers.push_back(ModifierToJson(modifier));
    }
    aj["modifiers"] = modifiers;

    aj["minRange"] = attack.minRange;
    aj["maxRange"] = attack.maxRange;
    aj["weight"] = attack.weight;
    aj["cooldown"] = attack.cooldown;
    aj["selfHpBelow"] = attack.selfHpBelow;
    aj["targetHpBelow"] = attack.targetHpBelow;
    aj["parriable"] = attack.parriable;
    aj["fast"] = attack.fast;

    array.push_back(aj);
  }
  j["attacks"] = array;

  try {
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
      std::filesystem::create_directories(filePath.parent_path());
    }
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      Logger(
          ("[EnemyAttack] 保存用ファイルを開けません: " + path + "\n").c_str());
      return false;
    }
    ofs << std::setw(4) << j;
    Logger(("[EnemyAttack] 保存しました: " + path + "\n").c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[EnemyAttack] 保存失敗: " + std::string(e.what()) + "\n").c_str());
    return false;
  }
}
