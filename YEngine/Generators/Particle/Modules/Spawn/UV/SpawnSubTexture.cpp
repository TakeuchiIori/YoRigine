#include "SpawnSubTexture.h"
#include <cmath>

void SpawnSubTexture::OnSpawn(ParticleAttribute* attrs, uint32_t index)
{
    int totalFrames = cols_ * rows_;
    if (totalFrames <= 0) return;

    int frame = randomStart_
        ? static_cast<int>(ParticleMath::RandomRange(0.0f, static_cast<float>(totalFrames)))
        : 0;
    frame = frame % totalFrames;

    attrs[index].flipbookFrame = static_cast<float>(frame);

    // UV を該当フレームに設定
    float invCols = 1.0f / cols_;
    float invRows = 1.0f / rows_;
    int col = frame % cols_;
    int row = frame / cols_;
    attrs[index].uvOffset = { col * invCols, row * invRows };
    attrs[index].uvScale  = { invCols, invRows };
}

void SpawnSubTexture::DrawEditor()
{
#ifdef USE_IMGUI
    ImGui::DragInt("列数 (X)", &cols_, 1, 1, 16);
    ImGui::DragInt("行数 (Y)", &rows_, 1, 1, 16);
    ImGui::Checkbox("ランダム開始フレーム", &randomStart_);
    ImGui::TextDisabled("総フレーム: %d", cols_ * rows_);
#endif
}

void SpawnSubTexture::SaveToJson(nlohmann::json& json) const
{
    json = { {"cols",cols_},{"rows",rows_},{"randomStart",randomStart_} };
}

void SpawnSubTexture::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("cols"))        cols_        = json["cols"];
    if (json.contains("rows"))        rows_        = json["rows"];
    if (json.contains("randomStart")) randomStart_ = json["randomStart"];
}
