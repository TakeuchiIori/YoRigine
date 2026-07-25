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
	// ヘッダーは少しコンパクトにするか、好みに合わせて調整
	if (!ImGui::CollapsingHeader("BONES (アーマチュア)", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("編集するボーンを選択します\n選択後、右のパネルで値を変更できます");

	YoRigine::Object3d* target = context_->GetTargetObject();
	YoRigine::Model* model = target ? target->GetModel() : nullptr;
	if (!model || !model->GetSkeleton()) {
		ImGui::TextDisabled("スケルトンなし");
		return;
	}

	auto& joints = model->GetSkeleton()->GetJoints();

	// ============================================================
	// 1. 親子関係のマップを構築する (ツリー描画の準備)
	// ============================================================
	std::vector<std::vector<int>> childrenMap(joints.size());
	std::vector<int> rootJoints; // 親を持たないルートボーンのリスト

	for (int i = 0; i < joints.size(); ++i) {
		auto par = const_cast<Joint&>(joints[i]).GetParent();
		if (par.has_value()) {
			childrenMap[par.value()].push_back(i);
		}
		else {
			rootJoints.push_back(i);
		}
	}

	// ============================================================
	// 2. ツリーUIの描画
	// ============================================================
	// ツリー部分をスクロール可能な枠で囲むことで、エディタをさらにコンパクトに
	ImGui::BeginChild("BoneTreeRegion", ImVec2(0, ImGui::GetContentRegionAvail().y - 30.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

	for (int rootIdx : rootJoints) {
		DrawBoneNode(joints, rootIdx, childrenMap);
	}

	ImGui::EndChild();

	// 下部のボタン
	ImGui::Separator();
	if (ImGui::SmallButton("選択解除")) {
		context_->selBone = "";
		context_->hasLiveBoneOverride = false;
		context_->liveOverrideBone.clear();
		context_->hasLiveBoneOriginal = false;
		context_->liveOriginalBone.clear();
		context_->statusMsg = "ボーン選択を解除しました";
	}
#endif
}

#ifdef USE_IMGUI
void BoneListPanel::DrawBoneNode(const std::vector<Joint>& joints, int jointIndex, const std::vector<std::vector<int>>& childrenMap)
{
	const auto& joint = joints[jointIndex];
	const std::string& name = joint.GetName();
	bool isSel = (context_->selBone == name);
	const auto& children = childrenMap[jointIndex];

	// BlenderやUnity風のツリーノード設定
	// SpanAvailWidth: 行全体を選択可能（ハイライト）にする
	// OpenOnArrow: 矢印（▶）をクリックした時だけ開閉し、名前クリックは選択操作にする
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

	if (isSel) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	if (children.empty()) {
		// 子がない場合は葉（Leaf）として扱い、左の矢印を消す
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	else {
		// デフォルトで階層を開いておくかどうか。コンパクトにしたい場合は以下の行をコメントアウトする
		flags |= ImGuiTreeNodeFlags_DefaultOpen;
	}

	// 選択中の色を変える
	if (isSel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.2f, 1.0f));

	// ツリーノードの描画 (IDに一意の jointIndex を使用)
	bool isOpen = ImGui::TreeNodeEx((void*)(intptr_t)jointIndex, flags, "%s", name.c_str());

	if (isSel) ImGui::PopStyleColor();

	// アイテムがクリックされた時の選択処理 (開閉の矢印クリックは除く)
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		if (context_->selBone != name) {
			context_->hasLiveBoneOverride = false;
			context_->liveOverrideBone.clear();
			context_->hasLiveBoneOriginal = false;
			context_->liveOriginalBone.clear();
		}
		context_->selBone = name;
		context_->statusMsg = "ボーン選択: " + name;

		// 選択されたら即座にバッファへ同期させる
		if (context_->SyncJointToBuffer) {
			context_->SyncJointToBuffer();
		}
	}

	// 階層が開かれている ＆ 子が存在する場合は再帰的に子を描画
	if (isOpen && !children.empty()) {
		for (int childIdx : children) {
			DrawBoneNode(joints, childIdx, childrenMap);
		}
		ImGui::TreePop(); // ツリーのインデントを戻す
	}
}
#endif
