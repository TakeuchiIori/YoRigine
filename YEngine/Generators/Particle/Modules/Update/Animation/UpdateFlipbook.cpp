#include "UpdateFlipbook.h"
#include <cmath>

void UpdateFlipbook::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    auto& a = attrs[index];
    int totalFrames = cols_ * rows_;
    if (totalFrames <= 0) return;

    a.flipbookFrame += fps_ * dt;

    if (loopEnd_ && a.flipbookFrame >= totalFrames) {
        a.isActive = false;
        return;
    }

    int frame = static_cast<int>(a.flipbookFrame) % totalFrames;
    float invCols = 1.0f / cols_;
    float invRows = 1.0f / rows_;
    int col = frame % cols_;
    int row = frame / cols_;

    a.uvOffset = { col * invCols, row * invRows };
    a.uvScale  = { invCols, invRows };
}

void UpdateFlipbook::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragInt("列数 (X)",    &cols_,    1, 1, 16);
    ImGui::DragInt("行数 (Y)",    &rows_,    1, 1, 16);
    ImGui::DragFloat("FPS",       &fps_,     0.5f, 1.0f, 60.0f);
    ImGui::Checkbox("最終フレームで消滅", &loopEnd_);
    ImGui::TextDisabled("総フレーム: %d", cols_ * rows_);
    ImGui::TextDisabled("SpawnSubTexture と組み合わせて使用");
#endif
}

void UpdateFlipbook::SaveToJson(nlohmann::json& json) const
{
    json = { {"cols",cols_},{"rows",rows_},{"fps",fps_},{"loopEnd",loopEnd_} };
}

void UpdateFlipbook::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("cols"))    cols_    = json["cols"];
    if (json.contains("rows"))    rows_    = json["rows"];
    if (json.contains("fps"))     fps_     = json["fps"];
    if (json.contains("loopEnd")) loopEnd_ = json["loopEnd"];
}
