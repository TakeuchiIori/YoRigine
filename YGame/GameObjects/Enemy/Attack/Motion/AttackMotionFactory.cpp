#include "AttackMotionFactory.h"

#include "OrbitMotion.h"
#include "SplineMotion.h"

using json = nlohmann::json;

namespace {

Vector3 Vec3FromJson(const json &j, const Vector3 &fallback = {}) {
  if (!j.is_object())
    return fallback;
  return {j.value("x", fallback.x), j.value("y", fallback.y),
          j.value("z", fallback.z)};
}

json Vec3ToJson(const Vector3 &v) {
  return json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
}

// ── スプライン ──
std::unique_ptr<IAttackMotion> CreateSpline(const json &j) {
  auto motion = std::make_unique<SplineMotion>();
  motion->space =
      MotionSpaceFromString(j.value("space", std::string("selfLocal")));
  motion->constantSpeed = j.value("constantSpeed", true);

  if (j.contains("points") && j["points"].is_array()) {
    for (const auto &point : j["points"]) {
      motion->points.push_back(Vec3FromJson(point));
    }
  }
  return motion;
}

json SplineToJson(const SplineMotion &motion) {
  json j;
  j["type"] = motion.GetTypeName();
  j["space"] = MotionSpaceToString(motion.space);
  j["constantSpeed"] = motion.constantSpeed;

  json points = json::array();
  for (const auto &point : motion.points) {
    points.push_back(Vec3ToJson(point));
  }
  j["points"] = points;
  return j;
}

// ── 円運動 ──
std::unique_ptr<IAttackMotion> CreateOrbit(const json &j) {
  auto motion = std::make_unique<OrbitMotion>();
  motion->space =
      MotionSpaceFromString(j.value("space", std::string("targetRelative")));
  motion->startRadius = j.value("startRadius", 6.0f);
  motion->endRadius = j.value("endRadius", 2.0f);
  motion->startAngleDeg = j.value("startAngleDeg", 0.0f);
  motion->sweepDeg = j.value("sweepDeg", 180.0f);
  motion->startHeight = j.value("startHeight", 0.0f);
  motion->endHeight = j.value("endHeight", 0.0f);
  return motion;
}

json OrbitToJson(const OrbitMotion &motion) {
  return json{
      {"type", motion.GetTypeName()},
      {"space", MotionSpaceToString(motion.space)},
      {"startRadius", motion.startRadius},
      {"endRadius", motion.endRadius},
      {"startAngleDeg", motion.startAngleDeg},
      {"sweepDeg", motion.sweepDeg},
      {"startHeight", motion.startHeight},
      {"endHeight", motion.endHeight},
  };
}

} // namespace

std::unique_ptr<IAttackMotion> AttackMotionFactory::Create(const json &j) {
  if (!j.is_object())
    return nullptr;

  const std::string type = j.value("type", std::string());
  if (type == "spline")
    return CreateSpline(j);
  if (type == "orbit")
    return CreateOrbit(j);
  return nullptr;
}

json AttackMotionFactory::ToJson(const IAttackMotion &motion) {
  if (auto *spline = dynamic_cast<const SplineMotion *>(&motion)) {
    return SplineToJson(*spline);
  }
  if (auto *orbit = dynamic_cast<const OrbitMotion *>(&motion)) {
    return OrbitToJson(*orbit);
  }
  return json::object();
}

std::unique_ptr<IAttackMotion>
AttackMotionFactory::CreateDefault(const std::string &typeName) {
  if (typeName == "spline") {
    // 既定は「その場から前方へ8m進む」＝単純な突進
    auto motion = std::make_unique<SplineMotion>();
    motion->points = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 8.0f}};
    return motion;
  }
  if (typeName == "orbit") {
    return std::make_unique<OrbitMotion>();
  }
  return nullptr;
}

const std::vector<std::string> &AttackMotionFactory::GetTypeNames() {
  static const std::vector<std::string> names = {"spline", "orbit"};
  return names;
}
