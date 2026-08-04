#include "EnemyAttackAction.h"

#include <Debugger/Logger.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>

using json = nlohmann::json;

// ============================================================
// enum <-> 文字列
// JSONを人が読める形にするための変換。
// 未知の文字列は既定値へ倒して、手打ちミスで落ちないようにする。
// ============================================================
const char *AttackPhaseTypeToString(AttackPhaseType type) {
  switch (type) {
  case AttackPhaseType::Anticipation:
    return "anticipation";
  case AttackPhaseType::Charge:
    return "charge";
  case AttackPhaseType::Dash:
    return "dash";
  case AttackPhaseType::Spin:
    return "spin";
  case AttackPhaseType::Leap:
    return "leap";
  case AttackPhaseType::Scripted:
    return "scripted";
  default:
    return "wait";
  }
}

AttackPhaseType AttackPhaseTypeFromString(const std::string &name) {
  if (name == "anticipation")
    return AttackPhaseType::Anticipation;
  if (name == "charge")
    return AttackPhaseType::Charge;
  if (name == "dash")
    return AttackPhaseType::Dash;
  if (name == "spin")
    return AttackPhaseType::Spin;
  if (name == "leap")
    return AttackPhaseType::Leap;
  if (name == "scripted")
    return AttackPhaseType::Scripted;
  return AttackPhaseType::Wait;
}

const char *OffsetCurveToString(OffsetCurve curve) {
  switch (curve) {
  case OffsetCurve::EaseOutQuad:
    return "easeOutQuad";
  case OffsetCurve::Hump:
    return "hump";
  case OffsetCurve::SineIn:
    return "sineIn";
  case OffsetCurve::Linear:
    return "linear";
  default:
    return "easeOutCubic";
  }
}

OffsetCurve OffsetCurveFromString(const std::string &name) {
  if (name == "easeOutQuad")
    return OffsetCurve::EaseOutQuad;
  if (name == "hump")
    return OffsetCurve::Hump;
  if (name == "sineIn")
    return OffsetCurve::SineIn;
  if (name == "linear")
    return OffsetCurve::Linear;
  return OffsetCurve::EaseOutCubic;
}

// ============================================================
// 合計時間
// ============================================================
float EnemyAttackAction::TotalDuration() const {
  float total = 0.0f;
  for (size_t i = 0; i < phases.size(); ++i) {
    total += phases[i].duration;
  }

  // 繰り返し区間はその回数分だけ余分に加算する
  if (loopBegin >= 0 && loopEnd > loopBegin && loopCount > 1) {
    float loopTime = 0.0f;
    const int end = std::min(loopEnd, static_cast<int>(phases.size()));
    for (int i = loopBegin; i < end; ++i) {
      loopTime += phases[i].duration;
    }
    total += loopTime * static_cast<float>(loopCount - 1);
  }
  return total;
}

namespace {

json Vec3ToJson(const Vector3 &v) {
  return json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
}
json Vec4ToJson(const Vector4 &v) {
  return json{{"x", v.x}, {"y", v.y}, {"z", v.z}, {"w", v.w}};
}

Vector3 Vec3FromJson(const json &j, const Vector3 &fallback) {
  if (!j.is_object())
    return fallback;
  return {j.value("x", fallback.x), j.value("y", fallback.y),
          j.value("z", fallback.z)};
}

Vector4 Vec4FromJson(const json &j, const Vector4 &fallback) {
  if (!j.is_object())
    return fallback;
  return {j.value("x", fallback.x), j.value("y", fallback.y),
          j.value("z", fallback.z), j.value("w", fallback.w)};
}

json PhaseToJson(const AttackPhase &p) {
  json j;
  j["type"] = AttackPhaseTypeToString(p.type);
  if (!p.label.empty())
    j["label"] = p.label;
  j["duration"] = p.duration;

  if (p.distance != 0.0f) {
    j["distance"] = p.distance;
    j["offsetAxis"] = Vec3ToJson(p.offsetAxis);
    j["offsetCurve"] = OffsetCurveToString(p.offsetCurve);
  }
  if (p.speedMultiplier != 0.0f)
    j["speedMultiplier"] = p.speedMultiplier;
  if (p.homing != 0.0f)
    j["homing"] = p.homing;
  if (p.height != 0.0f)
    j["height"] = p.height;
  if (p.rotations != 0.0f)
    j["rotations"] = p.rotations;

  if (p.faceTarget)
    j["faceTarget"] = true;
  if (p.invincible)
    j["invincible"] = true;
  if (p.damageWindow >= 0)
    j["damageWindow"] = p.damageWindow;

  j["scaleFrom"] = Vec3ToJson(p.scaleFrom);
  j["scaleTo"] = Vec3ToJson(p.scaleTo);
  if (p.scaleTime != 0.0f)
    j["scaleTime"] = p.scaleTime;

  if (p.useColor) {
    j["useColor"] = true;
    j["color"] = Vec4ToJson(p.color);
  }
  if (p.shake) {
    j["shake"] = true;
    j["shakePower"] = p.shakePower;
  }
  if (!p.vfxName.empty())
    j["vfxName"] = p.vfxName;
  if (!p.scriptedId.empty())
    j["scriptedId"] = p.scriptedId;
  return j;
}

AttackPhase PhaseFromJson(const json &j) {
  AttackPhase p;
  p.type = AttackPhaseTypeFromString(j.value("type", std::string("wait")));
  p.label = j.value("label", std::string());
  p.duration = j.value("duration", 0.5f);

  p.distance = j.value("distance", 0.0f);
  if (j.contains("offsetAxis"))
    p.offsetAxis = Vec3FromJson(j["offsetAxis"], p.offsetAxis);
  p.offsetCurve = OffsetCurveFromString(
      j.value("offsetCurve", std::string("easeOutCubic")));

  p.speedMultiplier = j.value("speedMultiplier", 0.0f);
  p.homing = j.value("homing", 0.0f);
  p.height = j.value("height", 0.0f);
  p.rotations = j.value("rotations", 0.0f);

  p.faceTarget = j.value("faceTarget", false);
  p.invincible = j.value("invincible", false);
  p.damageWindow = j.value("damageWindow", -1);

  if (j.contains("scaleFrom"))
    p.scaleFrom = Vec3FromJson(j["scaleFrom"], p.scaleFrom);
  if (j.contains("scaleTo"))
    p.scaleTo = Vec3FromJson(j["scaleTo"], p.scaleTo);
  p.scaleTime = j.value("scaleTime", 0.0f);

  p.useColor = j.value("useColor", false);
  if (j.contains("color"))
    p.color = Vec4FromJson(j["color"], p.color);

  p.shake = j.value("shake", false);
  p.shakePower = j.value("shakePower", 0.2f);

  p.vfxName = j.value("vfxName", std::string());
  p.scriptedId = j.value("scriptedId", std::string());
  return p;
}

} // namespace

// ============================================================
// 読み込み
// ============================================================
bool EnemyAttackActionIO::Load(const std::string &path,
                               std::vector<EnemyAttackAction> &out) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    Logger(("[EnemyAttackAction] ファイルが見つかりません: " + path + "\n")
               .c_str());
    return false;
  }

  try {
    json j = json::parse(ifs);
    if (!j.contains("actions") || !j["actions"].is_array()) {
      Logger("[EnemyAttackAction] 'actions' 配列がありません\n");
      return false;
    }

    out.clear();
    for (const auto &actionJson : j["actions"]) {
      EnemyAttackAction action;
      action.id = actionJson.value("id", std::string());
      if (action.id.empty())
        continue; // IDのないエントリは無視する

      action.displayName = actionJson.value("displayName", action.id);

      if (actionJson.contains("phases") && actionJson["phases"].is_array()) {
        for (const auto &phaseJson : actionJson["phases"]) {
          action.phases.push_back(PhaseFromJson(phaseJson));
        }
      }

      action.loopBegin = actionJson.value("loopBegin", -1);
      action.loopEnd = actionJson.value("loopEnd", -1);
      action.loopCount = actionJson.value("loopCount", 1);
      action.loopSpeedGain = actionJson.value("loopSpeedGain", 0.0f);

      action.minRange = actionJson.value("minRange", 0.0f);
      action.maxRange = actionJson.value("maxRange", 999.0f);
      action.weight = actionJson.value("weight", 1.0f);
      action.cooldown = actionJson.value("cooldown", 0.0f);
      action.selfHpBelow = actionJson.value("selfHpBelow", 1.0f);
      action.targetHpBelow = actionJson.value("targetHpBelow", 1.0f);
      action.phaseGate = actionJson.value("phaseGate", -1);

      action.parriable = actionJson.value("parriable", false);
      action.fast = actionJson.value("fast", true);

      out.push_back(std::move(action));
    }

    Logger(("[EnemyAttackAction] " + std::to_string(out.size()) +
            "件の攻撃を読み込みました\n")
               .c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(
        ("[EnemyAttackAction] 読み込みに失敗: " + std::string(e.what()) + "\n")
            .c_str());
    return false;
  }
}

// ============================================================
// 書き出し
// ============================================================
bool EnemyAttackActionIO::Save(const std::string &path,
                               const std::vector<EnemyAttackAction> &actions) {
  json j;
  json array = json::array();

  for (const auto &action : actions) {
    json a;
    a["id"] = action.id;
    a["displayName"] = action.displayName;

    json phases = json::array();
    for (const auto &phase : action.phases) {
      phases.push_back(PhaseToJson(phase));
    }
    a["phases"] = phases;

    if (action.loopBegin >= 0) {
      a["loopBegin"] = action.loopBegin;
      a["loopEnd"] = action.loopEnd;
      a["loopCount"] = action.loopCount;
      a["loopSpeedGain"] = action.loopSpeedGain;
    }

    a["minRange"] = action.minRange;
    a["maxRange"] = action.maxRange;
    a["weight"] = action.weight;
    a["cooldown"] = action.cooldown;
    a["selfHpBelow"] = action.selfHpBelow;
    a["targetHpBelow"] = action.targetHpBelow;
    a["phaseGate"] = action.phaseGate;
    a["parriable"] = action.parriable;
    a["fast"] = action.fast;

    array.push_back(a);
  }
  j["actions"] = array;

  try {
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
      std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      Logger(("[EnemyAttackAction] 保存用ファイルを開けません: " + path + "\n")
                 .c_str());
      return false;
    }
    ofs << std::setw(4) << j;
    Logger(("[EnemyAttackAction] 保存しました: " + path + "\n").c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[EnemyAttackAction] 保存に失敗: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}
