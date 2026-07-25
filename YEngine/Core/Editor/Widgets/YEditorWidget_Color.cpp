// ===========================================================
// YEditorWidget_Color.cpp
// ===========================================================
#ifdef USE_IMGUI
#include "YEditorWidget_Color.h"

namespace YEditorWidget {

static constexpr ImGuiColorEditFlags kHDRFlags =
    ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;

bool ColorHDR(const char* label, Vector4& color)
{
    return ImGui::ColorEdit4(label, &color.x, kHDRFlags);
}

bool ColorHDR3(const char* label, Vector3& color)
{
    return ImGui::ColorEdit3(label, &color.x, kHDRFlags);
}

bool Color(const char* label, Vector4& color)
{
    return ImGui::ColorEdit4(label, &color.x);
}

} // namespace YEditorWidget
#endif // USE_IMGUI
