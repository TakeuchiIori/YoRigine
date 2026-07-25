#include "UpdateUVScroll.h"

void UpdateUVScroll::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt){
    attrs[index].uvOffset.x += scrollSpeed.x * dt;
    attrs[index].uvOffset.y += scrollSpeed.y * dt;

    // 値が大きくなりすぎないようにループ
    attrs[index].uvOffset.x = fmodf(attrs[index].uvOffset.x, 1.0f);
    attrs[index].uvOffset.y = fmodf(attrs[index].uvOffset.y, 1.0f);
}

void UpdateUVScroll::DrawEditor(){
#ifdef USE_IMGUI
    ImGui::DragFloat2("スクロール速度", &scrollSpeed.x, 0.01f);
#endif
}

void UpdateUVScroll::SaveToJson(nlohmann::json& json) const{
    json = {
        {"scrollSpeedX", scrollSpeed.x},
        {"scrollSpeedY", scrollSpeed.y}
	};
}

void UpdateUVScroll::LoadFromJson(const nlohmann::json& json){
    if (json.contains("scrollSpeedX")) scrollSpeed.x = json["scrollSpeedX"];
	if (json.contains("scrollSpeedY")) scrollSpeed.y = json["scrollSpeedY"];
}

// JSON ロード用ファクトリ登録（従来これが無く、保存後の再ロードで復元されなかった）
REGISTER_UPDATE_MODULE(UpdateUVScroll)
