#include "ToonSettings.h"

#include "Loaders/Json/JsonManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

ToonSettings* ToonSettings::GetInstance()
{
	static ToonSettings instance;
	return &instance;
}

void ToonSettings::Initialize()
{
	// JsonManager に登録すると Register 時点で保存値が自動ロードされる。
	json_ = std::make_unique<YoRigine::JsonManager>("Toon", "Resources/Json/Render/");
	json_->SetCategory("Render");
	json_->Register("トゥーン有効", &params_.enableToon);
	json_->Register("階調数", &params_.toonBands);
	json_->Register("バンド境界ソフト", &params_.toonEdgeSoftness);
	json_->Register("リムライト有効", &params_.enableRim);
	json_->Register("リムの鋭さ", &params_.rimPower);
	json_->Register("リムの強さ", &params_.rimIntensity);
	json_->Register("トゥーンスペキュラ有効", &params_.enableToonSpecular);
	json_->Register("スペキュラしきい値", &params_.toonSpecularThreshold);
	json_->Register("影の強さ", &params_.shadowStrength);
	json_->Register("影の色", &params_.shadowColor);
	json_->Register("ハイライトの色", &params_.highlightColor);
}

void ToonSettings::ImGui()
{
#ifdef USE_IMGUI
	ImGui::TextUnformatted("全オブジェクト共通のトゥーン設定");
	ImGui::Separator();

	bool enableToon = params_.enableToon != 0;
	if (ImGui::Checkbox("トゥーン有効", &enableToon)) params_.enableToon = enableToon ? 1 : 0;

	ImGui::SliderInt("階調数", &params_.toonBands, 1, 8);
	ImGui::SliderFloat("バンド境界ソフト", &params_.toonEdgeSoftness, 0.0f, 0.2f);

	ImGui::SeparatorText("影色（2D / アニメ塗り）");
	ImGui::SliderFloat("影の強さ", &params_.shadowStrength, 0.0f, 1.0f);
	ImGui::ColorEdit3("影の色", &params_.shadowColor.x);

	ImGui::SeparatorText("リムライト");
	bool enableRim = params_.enableRim != 0;
	if (ImGui::Checkbox("リムライト有効", &enableRim)) params_.enableRim = enableRim ? 1 : 0;
	ImGui::SliderFloat("リムの鋭さ", &params_.rimPower, 0.5f, 16.0f);
	ImGui::SliderFloat("リムの強さ", &params_.rimIntensity, 0.0f, 4.0f);

	ImGui::SeparatorText("トゥーンスペキュラ");
	bool enableToonSpec = params_.enableToonSpecular != 0;
	if (ImGui::Checkbox("トゥーンスペキュラ有効", &enableToonSpec)) params_.enableToonSpecular = enableToonSpec ? 1 : 0;
	ImGui::SliderFloat("スペキュラしきい値", &params_.toonSpecularThreshold, 0.0f, 1.0f);
	ImGui::ColorEdit3("ハイライトの色", &params_.highlightColor.x);

	ImGui::Separator();
	if (ImGui::Button("保存")) {
		if (json_) json_->Save();
	}
#endif
}
