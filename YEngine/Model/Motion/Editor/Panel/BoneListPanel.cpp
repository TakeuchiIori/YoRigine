#include "BoneListPanel.h"
#include "Object3D/ObjectManager.h"
#include "Skeleton/Skeleton.h"
#include "Model.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ============================================================
// 初期化
// ============================================================
void BoneListPanel::Initialize(MotionEditorContext* context)
{
	context_ = context;
}

void BoneListPanel::Update()
{
	// ロジック更新が必要であればここに記述
}

// ============================================================
// ImGui描画
// ============================================================
void BoneListPanel::DrawImGui()
{
#ifdef USE_IMGUI
	if (!ImGui::CollapsingHeader("BONES (アーマチュア)", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("編集するボーンを選択します\n選択後、右のパネルで値を変更できます");
	ImGui::Separator();

	Object3d* target = context_->GetTargetObject();
	Model* model = target ? target->GetModel() : nullptr;
	if (!model || !model->GetSkeleton()) {
		ImGui::TextDisabled("スケルトンなし");
		return;
	}

	auto& joints = model->GetSkeleton()->GetJoints();

	for (const auto& joint : joints) {
		const std::string& name = joint.GetName();
		bool isSel = (context_->selBone == name);

		// 深さ計算などは既存ロジックそのまま
		int depth = 0;
		{
			auto par = const_cast<Joint&>(joint).GetParent();
			while (par.has_value()) {
				++depth;
				par = joints[par.value()].GetParent();
			}
		}

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + depth * 12.0f);
		if (isSel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));

		std::string label = (depth > 0 ? "|  " : "") + name + "##b";
		if (ImGui::Selectable(label.c_str(), isSel, 0, ImVec2(0, 0))) {
			context_->selBone = name;
			context_->statusMsg = "ボーン選択: " + name;

			// 必要に応じて同期イベントを発火させる
			// context_->SyncJointToBuffer(name);
		}

		if (isSel) ImGui::PopStyleColor();
	}

	ImGui::Separator();
	if (ImGui::SmallButton("選択解除")) {
		context_->selBone = "";
		context_->statusMsg = "ボーン選択を解除しました";
	}
#endif
}