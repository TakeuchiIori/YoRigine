#ifdef USE_IMGUI
#include "AreaEditor.h"
#include "AreaManager.h"
#include "../CircleArea.h"
#include "../PolygonArea.h"
#include <imgui.h>
#include <json.hpp>

static const char* kAreaTypes[] = { "Circle", "Polygon" };
static const int   kAreaTypeCount = 2;

AreaEditor* AreaEditor::GetInstance()
{
	static AreaEditor instance;
	return &instance;
}

// ============================================================
// メインエントリ：毎フレーム呼ぶ
// ============================================================
void AreaEditor::Update()
{
	ImGui::SetNextWindowSize(ImVec2(750, 520), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Area Editor")) { ImGui::End(); return; }

	// ── 左ペイン：エリアリスト ──────────────────────────
	ImGui::BeginChild("##list", ImVec2(200.0f, -42.0f), true);
	DrawAreaList();
	ImGui::EndChild();

	ImGui::SameLine();

	// ── 右ペイン：プロパティ ────────────────────────────
	ImGui::BeginChild("##props", ImVec2(0.0f, -42.0f), true);
	DrawAreaProperties();
	ImGui::EndChild();

	// ── 下部：ファイル操作 ──────────────────────────────
	DrawFilePanel();

	// ── モーダル ────────────────────────────────────────
	DrawAddModal();

	ImGui::End();
}

// ============================================================
// 左ペイン：エリアの一覧・追加・削除・複製
// ============================================================
void AreaEditor::DrawAreaList()
{
	auto* mgr = AreaManager::GetInstance();

	ImGui::Text("Areas (%zu)", mgr->GetAreas().size());
	ImGui::Separator();

	for (const auto& [name, area] : mgr->GetAreas())
	{
		bool selected = (name == selectedName_);

		// タイプに応じてラベルに prefix を付ける
		std::string label = "[" + area->GetTypeString() + "] " + name;
		if (ImGui::Selectable(label.c_str(), selected))
		{
			selectedName_ = name;
			strncpy_s(renameBuf_, name.c_str(), sizeof(renameBuf_) - 1);
			isRenaming_   = false;
		}
	}

	ImGui::Separator();

	if (ImGui::Button("+ Add"))
	{
		showAddModal_ = true;
	}

	ImGui::SameLine();

	// 削除
	if (ImGui::Button("- Remove"))
	{
		if (!selectedName_.empty())
		{
			mgr->RemoveArea(selectedName_);
			selectedName_.clear();
		}
	}

	// 複製（パラメータを AutoJson 経由で JSON にして新規エリアへコピー）
	if (ImGui::Button("Duplicate"))
	{
		if (!selectedName_.empty())
		{
			auto src = mgr->GetArea(selectedName_);
			if (src)
			{
				auto newArea = AreaManager::CreateArea(src->GetTypeString());
				if (newArea)
				{
					// AutoJson で保存→読み込みすることでパラメータをコピー
					nlohmann::json tmp;
					AutoJson copyRoot;
					copyRoot.AddGroup("data", src->GetAutoJson());
					copyRoot.SaveToFile("__tmp_copy__.json");

					AutoJson pasteRoot;
					pasteRoot.AddGroup("data", newArea->GetAutoJson());
					pasteRoot.LoadFromFile("__tmp_copy__.json");

					newArea->SetPurpose(src->GetPurpose());

					std::string newName = selectedName_ + "_copy";
					mgr->AddArea(newName, newArea);
					selectedName_ = newName;
				}
			}
		}
	}
}

// ============================================================
// 右ペイン：選択中エリアのプロパティ
// ============================================================
void AreaEditor::DrawAreaProperties()
{
	if (selectedName_.empty())
	{
		ImGui::TextDisabled("エリアを選択してください");
		return;
	}

	auto area = AreaManager::GetInstance()->GetArea(selectedName_);
	if (!area)
	{
		ImGui::TextDisabled("(エリアが見つかりません)");
		selectedName_.clear();
		return;
	}

	// ── 名前（リネーム） ──────────────────────────────
	if (!isRenaming_)
	{
		ImGui::Text("Name: %s", selectedName_.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Rename"))
		{
			strncpy_s(renameBuf_, selectedName_.c_str(), sizeof(renameBuf_) - 1);
			isRenaming_ = true;
		}
	}
	else
	{
		ImGui::SetNextItemWidth(180.0f);
		if (ImGui::InputText("##rename", renameBuf_, sizeof(renameBuf_),
			ImGuiInputTextFlags_EnterReturnsTrue))
		{
			ApplyRename(std::string(renameBuf_));
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("OK"))     ApplyRename(std::string(renameBuf_));
		ImGui::SameLine();
		if (ImGui::SmallButton("Cancel")) isRenaming_ = false;
	}

	// ── タイプ（読み取り専用） ───────────────────────
	ImGui::Text("Type: %s", area->GetTypeString().c_str());

	// ── Purpose コンボ ──────────────────────────────
	const char* purposes[] = { "Boundary", "Trigger" };
	int purposeIdx = (area->GetPurpose() == AreaPurpose::Trigger) ? 1 : 0;
	if (ImGui::Combo("Purpose", &purposeIdx, purposes, 2))
	{
		area->SetPurpose(purposeIdx == 1 ? AreaPurpose::Trigger : AreaPurpose::Boundary);
	}

	ImGui::Separator();

	// ── 共通パラメータ（active / debugDraw / 各形状パラメータ）──
	// AutoJson::ShowImGui が active, debugDraw, center, radius … を自動表示する
	area->GetAutoJson().ShowImGui(selectedName_);

	// ── Polygon 専用：頂点エディタ ──────────────────
	if (area->GetAreaType() == AreaType::Polygon)
	{
		ImGui::Separator();
		DrawPolygonProperties(dynamic_cast<PolygonArea*>(area.get()));
	}
}

// ============================================================
// PolygonArea 専用：頂点エディタ
// ============================================================
void AreaEditor::DrawPolygonProperties(PolygonArea* poly)
{
	if (!poly) return;

	auto& verts = poly->GetVertices();

	ImGui::Text("Vertices  (%zu)", verts.size());
	ImGui::SameLine();
	if (ImGui::SmallButton("+ Vertex"))
	{
		// 末尾頂点を複製して追加（空の場合は原点）
		poly->AddVertex(verts.empty() ? Vector3{ 0.0f, 0.0f, 0.0f } : verts.back());
	}

	int removeIdx = -1;

	if (ImGui::BeginTable("##verts", 5,
		ImGuiTableFlags_Borders      |
		ImGuiTableFlags_RowBg        |
		ImGuiTableFlags_SizingFixedFit |
		ImGuiTableFlags_ScrollY,
		ImVec2(0.0f, 200.0f)))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("No",  ImGuiTableColumnFlags_WidthFixed,   28.0f);
		ImGui::TableSetupColumn("X",   ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Y",   ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Z",   ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Del", ImGuiTableColumnFlags_WidthFixed,   28.0f);
		ImGui::TableHeadersRow();

		for (int i = 0; i < static_cast<int>(verts.size()); ++i)
		{
			ImGui::TableNextRow();
			ImGui::PushID(i);

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", i);

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##x", &verts[i].x, 0.1f, 0.0f, 0.0f, "%.1f");

			ImGui::TableSetColumnIndex(2);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##y", &verts[i].y, 0.1f, 0.0f, 0.0f, "%.1f");

			ImGui::TableSetColumnIndex(3);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat("##z", &verts[i].z, 0.1f, 0.0f, 0.0f, "%.1f");

			ImGui::TableSetColumnIndex(4);
			if (ImGui::SmallButton("x")) removeIdx = i;

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	// テーブル外で削除（イテレータ破壊を防ぐ）
	if (removeIdx >= 0)
	{
		poly->RemoveVertex(removeIdx);
	}
}

// ============================================================
// 下部：ファイルパス入力・Save / Load ボタン
// ============================================================
void AreaEditor::DrawFilePanel()
{
	ImGui::Separator();

	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 136.0f);
	ImGui::InputText("##filepath", filePathBuf_, sizeof(filePathBuf_));

	ImGui::SameLine();
	if (ImGui::Button("Save", ImVec2(64.0f, 0.0f)))
	{
		AreaManager::GetInstance()->SaveAllToFile(filePathBuf_);
	}

	ImGui::SameLine();
	if (ImGui::Button("Load", ImVec2(64.0f, 0.0f)))
	{
		AreaManager::GetInstance()->LoadAllFromFile(filePathBuf_);
		selectedName_.clear();
	}
}

// ============================================================
// 新規エリア追加モーダル
// ============================================================
void AreaEditor::DrawAddModal()
{
	if (showAddModal_)
	{
		ImGui::OpenPopup("Add New Area");
		showAddModal_ = false;
	}

	ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f));
	if (!ImGui::BeginPopupModal("Add New Area", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::Combo("Type", &newTypeIndex_, kAreaTypes, kAreaTypeCount);
	ImGui::InputText("Name", newNameBuf_, sizeof(newNameBuf_));

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
	{
		std::string name(newNameBuf_);
		std::string type(kAreaTypes[newTypeIndex_]);

		if (!name.empty())
		{
			auto area = AreaManager::CreateArea(type);
			if (area)
			{
				AreaManager::GetInstance()->AddArea(name, area);
				selectedName_ = name;
			}
		}
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
	{
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

// ============================================================
// リネーム確定処理
// ============================================================
void AreaEditor::ApplyRename(const std::string& newName)
{
	if (!newName.empty() && newName != selectedName_)
	{
		auto* mgr = AreaManager::GetInstance();
		auto  a   = mgr->GetArea(selectedName_);
		if (a)
		{
			mgr->RemoveArea(selectedName_);
			mgr->AddArea(newName, a);
			selectedName_ = newName;
		}
	}
	isRenaming_ = false;
}

#endif // USE_IMGUI
