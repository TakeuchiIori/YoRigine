// ===========================================================
// YEditorWidget_Value.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Value.h"
#include "YEditorWidget_ItemWidth.h"

namespace YEditorWidget {

// ── Float ────────────────────────────────────────────────────

bool DragFloat(const char* label, float& v,
               float speed, float vmin, float vmax, const char* fmt)
{
    SetNextItemWidthForLabel(label);
    return ImGui::DragFloat(label, &v, speed, vmin, vmax, fmt);
}

bool SliderFloat(const char* label, float& v, float vmin, float vmax,
                 const char* fmt)
{
    SetNextItemWidthForLabel(label);
    return ImGui::SliderFloat(label, &v, vmin, vmax, fmt);
}

bool AngleSlider(const char* label, float& radians, float minDeg, float maxDeg)
{
    float deg = radians * (180.f / 3.14159265358979f);
    SetNextItemWidthForLabel(label);
    if (ImGui::SliderFloat(label, &deg, minDeg, maxDeg, "%.1f deg")) {
        radians = deg * (3.14159265358979f / 180.f);
        return true;
    }
    return false;
}

// ── Int ──────────────────────────────────────────────────────

bool DragInt(const char* label, int& v, float speed, int vmin, int vmax)
{
    SetNextItemWidthForLabel(label);
    return ImGui::DragInt(label, &v, speed, vmin, vmax);
}

bool SliderInt(const char* label, int& v, int vmin, int vmax)
{
    SetNextItemWidthForLabel(label);
    return ImGui::SliderInt(label, &v, vmin, vmax);
}

// ── Vector ───────────────────────────────────────────────────

bool DragVec2(const char* label, Vector2& v,
              float speed, float vmin, float vmax)
{
    SetNextItemWidthForLabel(label);
    return ImGui::DragFloat2(label, &v.x, speed, vmin, vmax, "%.3f");
}

bool DragVec3(const char* label, Vector3& v,
              float speed, float vmin, float vmax, const char* fmt)
{
    SetNextItemWidthForLabel(label);
    return ImGui::DragFloat3(label, &v.x, speed, vmin, vmax, fmt);
}

bool DirectionVec3(const char* label, Vector3& dir, float speed)
{
    bool changed = false;
    ImGui::PushID(label);

    const float buttonWidth = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(ImGui::CalcItemWidth() - buttonWidth - ImGui::GetStyle().ItemInnerSpacing.x);
    if (ImGui::DragFloat3(label, &dir.x, speed, 0.f, 0.f, "%.3f"))
        changed = true;

    ImGui::SameLine(0.f, ImGui::GetStyle().ItemInnerSpacing.x);

    if (ImGui::Button("N", ImVec2(buttonWidth, 0))) {
        float len = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
        if (len > 1e-8f) {
            dir = dir.Normalize();
            changed = true;
        }
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Normalize");

    ImGui::PopID();
    return changed;
}

bool RangeFloat(const char* label, float& vmin, float& vmax, float speed)
{
    float buf[2] = { vmin, vmax };
    if (ImGui::DragFloat2(label, buf, speed, 0.f, 0.f, "%.3f")) {
        vmin = buf[0] < buf[1] ? buf[0] : buf[1];
        vmax = buf[0] < buf[1] ? buf[1] : buf[0];
        return true;
    }
    return false;
}

} // namespace YEditorWidget
#endif // USE_IMGUI
