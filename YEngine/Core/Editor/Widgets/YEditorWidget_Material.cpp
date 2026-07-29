// ===========================================================
// YEditorWidget_Material.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Material.h"
#include "Material/Material.h"
#include "YEditorWidget_ItemWidth.h"

#include <cstdio>

namespace YEditorWidget {

namespace {

// 「上書きしない」状態でベース値を淡色表示する共通処理
void DrawBaseValueHint(const char *text) {
  ImGui::SameLine();
  ImGui::TextDisabled("%s", text);
}

// チェックボックスの ID をラベルから分離する
// (同じ画面に同名ラベルが複数並ぶため ## で必ず一意化する)
std::string CheckboxId(const char *label) {
  return std::string("##ov_") + label;
}

} // namespace

bool OverrideFloat(const char *label, bool &enabled, float &value,
                   float baseValue, float vmin, float vmax, const char *fmt) {
  bool changed = false;

  const std::string id = CheckboxId(label);
  if (ImGui::Checkbox(id.c_str(), &enabled)) {
    // 上書きを ON にした瞬間はモデル本来の値から始めると編集しやすい
    if (enabled) {
      value = baseValue;
    }
    changed = true;
  }
  ImGui::SameLine();

  if (!enabled) {
    ImGui::BeginDisabled();
    float shown = baseValue;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat(label, &shown, vmin, vmax, fmt);
    ImGui::EndDisabled();
    DrawBaseValueHint("モデル本来の値");
    return changed;
  }

  ImGui::SetNextItemWidth(160.0f);
  if (ImGui::SliderFloat(label, &value, vmin, vmax, fmt)) {
    changed = true;
  }
  return changed;
}

bool OverrideColor3(const char *label, bool &enabled, Vector3 &value,
                    const Vector3 &baseValue) {
  bool changed = false;

  const std::string id = CheckboxId(label);
  if (ImGui::Checkbox(id.c_str(), &enabled)) {
    if (enabled) {
      value = baseValue;
    }
    changed = true;
  }
  ImGui::SameLine();

  if (!enabled) {
    ImGui::BeginDisabled();
    Vector3 shown = baseValue;
    ImGui::SetNextItemWidth(160.0f);
    ImGui::ColorEdit3(label, &shown.x);
    ImGui::EndDisabled();
    DrawBaseValueHint("モデル本来の値");
    return changed;
  }

  ImGui::SetNextItemWidth(160.0f);
  if (ImGui::ColorEdit3(label, &value.x)) {
    changed = true;
  }
  return changed;
}

//=============================================================================
// MaterialSlotEditor
//=============================================================================
MaterialSlotResult MaterialSlotEditor::Draw(const char *id,
                                            MeshMaterialOverride &slot,
                                            const Material *baseMaterial,
                                            const std::string &currentTexture) {
  MaterialSlotResult result;

  ImGui::PushID(id);

  // ── モデル本来のベースカラー ──
  const Vector3 baseColor =
      baseMaterial ? baseMaterial->GetKd() : Vector3{1.0f, 1.0f, 1.0f};

  if (OverrideColor3("ベースカラー", slot.overrideBaseColor, slot.baseColor,
                     baseColor)) {
    result.changed = true;
  }

  // ── テクスチャ差し替え ──
  // 実際のファイル選択は FileBrowser (パネル側が所有) が行うので、ここは
  // 現在のテクスチャ表示とボタンだけ。押されたら呼び出し側へ知らせる。
  ImGui::Separator();
  ImGui::TextDisabled("テクスチャ: %s",
                      currentTexture.empty() ? "(なし)" : currentTexture.c_str());

  if (ImGui::Button("テクスチャを開く")) {
    result.requestOpenTexture = true;
  }
  if (!slot.texturePath.empty()) {
    ImGui::SameLine();
    if (ImGui::SmallButton("元に戻す")) {
      result.clearTexture = true;
    }
  }

  ImGui::PopID();
  return result;
}

} // namespace YEditorWidget
#endif // USE_IMGUI
