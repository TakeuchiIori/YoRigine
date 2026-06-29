#include "OutlineSettings.h"

#include "DirectXCommon.h"
#include "Loaders/Json/JsonManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

OutlineSettings* OutlineSettings::GetInstance()
{
	static OutlineSettings instance;
	return &instance;
}

void OutlineSettings::Initialize()
{
	// シェーダーが直接読む定数バッファを確保（MaterialLighting と同じ作法）。
	resource_ = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(sizeof(OutlineCB));
	resource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped_));

	// JSON に登録すると Register 時点で保存値が自動ロードされる。
	json_ = std::make_unique<YoRigine::JsonManager>("Outline", "Resources/Json/Render/");
	json_->SetCategory("Render");
	json_->Register("輪郭線有効", &enable_);
	json_->Register("太さ", &width_);
	json_->Register("色", &color_);
	json_->Register("距離フェード有効", &useDistanceFade_);
	json_->Register("フェード開始", &fadeStart_);
	json_->Register("フェード終了", &fadeEnd_);

	UpdateCB();
}

void OutlineSettings::UpdateCB()
{
	if (!mapped_) return;
	mapped_->color[0] = color_.x;
	mapped_->color[1] = color_.y;
	mapped_->color[2] = color_.z;
	mapped_->color[3] = 1.0f;
	mapped_->width = width_;
	mapped_->enable = enable_;
	mapped_->useDistanceFade = useDistanceFade_;
	mapped_->distanceFadeStart = fadeStart_;
	mapped_->distanceFadeEnd = fadeEnd_;
	mapped_->_pad[0] = mapped_->_pad[1] = 0.0f;
}

void OutlineSettings::ImGui()
{
#ifdef USE_IMGUI
	ImGui::TextUnformatted("全オブジェクト共通の輪郭線（インバートハル）設定");
	ImGui::Separator();

	bool enable = enable_ != 0;
	if (ImGui::Checkbox("輪郭線有効", &enable)) enable_ = enable ? 1 : 0;

	ImGui::SliderFloat("太さ", &width_, 0.0f, 0.05f, "%.4f");
	ImGui::ColorEdit3("色", &color_.x);

	ImGui::SeparatorText("距離フェード（遠景の真っ黒化防止）");
	bool useFade = useDistanceFade_ != 0;
	if (ImGui::Checkbox("距離フェード有効", &useFade)) useDistanceFade_ = useFade ? 1 : 0;
	if (useDistanceFade_) {
		ImGui::DragFloat("フェード開始", &fadeStart_, 0.5f, 0.0f, 1000.0f);
		ImGui::DragFloat("フェード終了", &fadeEnd_, 0.5f, 0.0f, 1000.0f);
	}

	ImGui::Separator();
	if (ImGui::Button("保存")) {
		if (json_) json_->Save();
	}
#endif
}
