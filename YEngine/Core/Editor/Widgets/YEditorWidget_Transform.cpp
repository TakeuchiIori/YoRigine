// ===========================================================
// YEditorWidget_Transform.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Transform.h"
#include "YEditorWidget_ItemWidth.h"

#include <cmath>

namespace YEditorWidget {

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;
} // namespace

bool DragEulerDegrees(const char *label, Vector3 &radians, float speed) {
  Vector3 deg{radians.x * kRadToDeg, radians.y * kRadToDeg,
              radians.z * kRadToDeg};
  SetNextItemWidthForLabel(label);
  if (ImGui::DragFloat3(label, &deg.x, speed, 0.0f, 0.0f, "%.1f")) {
    radians = {deg.x * kDegToRad, deg.y * kDegToRad, deg.z * kDegToRad};
    return true;
  }
  return false;
}

bool TransformFields::DrawScale(Vector3 &scale) {
  // 一様スケール時は X の変化量を比率として他軸へ伝播させる。
  // 0 除算を避けるため、基準軸が 0 のときは比率ではなく差分で合わせる。
  if (!uniformScale_) {
    SetNextItemWidthForLabel("スケール");
    return ImGui::DragFloat3("スケール", &scale.x, scaleSpeed_, 0.001f, 1000.0f,
                             "%.3f");
  }

  float uniform = scale.x;
  SetNextItemWidthForLabel("スケール");
  if (!ImGui::DragFloat("スケール", &uniform, scaleSpeed_, 0.001f, 1000.0f,
                        "%.3f")) {
    return false;
  }
  scale = {uniform, uniform, uniform};
  return true;
}

bool TransformFields::Draw(Vector3 &position, Vector3 &rotationRadians,
                           Vector3 &scale) {
  bool changed = false;

  SetNextItemWidthForLabel("位置");
  if (ImGui::DragFloat3("位置", &position.x, positionSpeed_)) {
    changed = true;
  }

  if (DragEulerDegrees("回転 (度)", rotationRadians, rotationSpeed_)) {
    changed = true;
  }

  if (DrawScale(scale)) {
    changed = true;
  }
  ImGui::SameLine();
  ImGui::Checkbox("一様##uniformScale", &uniformScale_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("XYZ を同じ倍率で編集する");
  }

  return changed;
}

bool TransformFields::DrawResetButtons(Vector3 &position,
                                       Vector3 &rotationRadians,
                                       Vector3 &scale) {
  bool changed = false;

  if (ImGui::Button("位置リセット")) {
    position = {};
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("回転リセット")) {
    rotationRadians = {};
    changed = true;
  }
  ImGui::SameLine();
  if (ImGui::Button("スケールリセット")) {
    scale = {1.0f, 1.0f, 1.0f};
    changed = true;
  }

  return changed;
}

} // namespace YEditorWidget
#endif // USE_IMGUI
