#ifdef USE_IMGUI

#include "ImGuiStudio.h"

#include "Core/Editor/Widgets/YEditorWidget.h"

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

using json = nlohmann::json;

// ImGuiのベクトル型を、内容が確認しやすいJSON配列へ変換する。
json ToJson(const ImVec2& value)
{
	return json::array({ value.x, value.y });
}

json ToJson(const ImVec4& value)
{
	return json::array({ value.x, value.y, value.z, value.w });
}

// 不正な要素数の配列では現在値を壊さず、その項目だけ読み飛ばす。
void LoadVec2(const json& j, ImVec2& value)
{
	if (!j.is_array() || j.size() != 2) return;
	value = { j[0].get<float>(), j[1].get<float>() };
}

void LoadVec4(const json& j, ImVec4& value)
{
	if (!j.is_array() || j.size() != 4) return;
	value = {
		j[0].get<float>(), j[1].get<float>(),
		j[2].get<float>(), j[3].get<float>()
	};
}

// 初回保存時にResources/Json/Editorがなくても保存できるようにする。
void EnsureParentDirectory(const char* filepath)
{
	const std::filesystem::path path(filepath);
	if (path.has_parent_path()) {
		std::filesystem::create_directories(path.parent_path());
	}
}

} // namespace

ImGuiStudio* ImGuiStudio::GetInstance()
{
	static ImGuiStudio instance;
	return &instance;
}

void ImGuiStudio::Initialize()
{
	if (initialized_) return;
	initialized_ = true;

	// カスタムテーマを読む前の値をYoRigineプリセットとして残しておく。
	startupStyle_ = ImGui::GetStyle();

	// 初回起動ではファイルが存在しないため、エラー表示せず初期値を使う。
	if (std::filesystem::exists(kThemePath)) LoadTheme();
	if (std::filesystem::exists(kLayoutPath)) LoadLayout();
}

void ImGuiStudio::Draw()
{
	if (!initialized_) Initialize();

	if (YEditorWidget::TabBar tabs{ "##ImGuiStudioTabs" }) {
		if (YEditorWidget::Tab tab{ "テーマ" }) {
			DrawThemeEditor();
		}
		if (YEditorWidget::Tab tab{ "UIレイアウト" }) {
			DrawLayoutEditor();
		}
		if (YEditorWidget::Tab tab{ "参考ツール" }) {
			ImGui::TextWrapped(
				"このStudioは ImThemes のリアルタイムテーマ編集と、"
				"ImRAD / ImStudio の配置・プロパティ・プレビュー・"
				"C++生成ワークフローを、YoRigine向けに統合したものです。");
			ImGui::Separator();
			ImGui::BulletText("ImThemes: テーマ設計とリアルタイムプレビュー");
			ImGui::BulletText("ImRAD: 継続編集可能な標準Dear ImGui C++生成");
			ImGui::BulletText("ImStudio: ウィジェット階層とプロパティ編集");
		}
	}
}

void ImGuiStudio::DrawThemeEditor()
{
	// プリセットの適用はプレビューだけを即時変更する。
	// 「テーマ保存」を押すまではJSONへ書き込まない。
	YEditorWidget::SectionHeader("プリセット");
	if (ImGui::Button("YoRigine")) {
		ImGui::GetStyle() = startupStyle_;
		themeStatus_ = "YoRigineテーマを適用しました（未保存）";
	}
	ImGui::SameLine();
	if (ImGui::Button("Dark")) {
		ImGui::StyleColorsDark();
		themeStatus_ = "Darkテーマを適用しました（未保存）";
	}
	ImGui::SameLine();
	if (ImGui::Button("Light")) {
		ImGui::StyleColorsLight();
		themeStatus_ = "Lightテーマを適用しました（未保存）";
	}
	ImGui::SameLine();
	if (ImGui::Button("Classic")) {
		ImGui::StyleColorsClassic();
		themeStatus_ = "Classicテーマを適用しました（未保存）";
	}

	YEditorWidget::SectionHeader("保存・出力");
	if (ImGui::Button("テーマ保存")) SaveTheme();
	ImGui::SameLine();
	if (ImGui::Button("テーマ読込")) LoadTheme();
	ImGui::SameLine();
	if (ImGui::Button("C++をコピー")) CopyThemeCpp();
	YEditorWidget::ItemTooltip("現在の全カラーを ImGuiStyle C++コードとしてクリップボードへコピー");
	ImGui::TextDisabled("%s", kThemePath);
	if (!themeStatus_.empty()) ImGui::TextWrapped("%s", themeStatus_.c_str());

	YEditorWidget::SectionHeader("リアルタイム編集");
	// Dear ImGui標準のStyleEditorを利用することで、全カラーと余白・丸みを
	// 編集したフレームからエディタ全体へ即時反映する。
	ImGui::ShowStyleEditor(&ImGui::GetStyle());
}

void ImGuiStudio::DrawLayoutEditor()
{
	// WidgetTypeと同じ順番を維持すること。
	static const char* kTypeNames[] = {
		"Text", "Button", "Checkbox", "SliderFloat",
		"InputText", "Separator", "SameLine"
	};

	if (YEditorWidget::Combo(
		"追加するウィジェット##ImGuiStudio", addTypeIndex_,
		kTypeNames, IM_ARRAYSIZE(kTypeNames))) {
		addTypeIndex_ = std::clamp(addTypeIndex_, 0, IM_ARRAYSIZE(kTypeNames) - 1);
	}
	ImGui::SameLine();
	if (ImGui::Button("追加")) {
		AddWidget(static_cast<WidgetType>(addTypeIndex_));
	}
	ImGui::SameLine();
	if (ImGui::Button("保存")) SaveLayout();
	ImGui::SameLine();
	if (ImGui::Button("読込")) LoadLayout();
	ImGui::SameLine();
	if (ImGui::Button("C++をコピー")) CopyLayoutCpp();

	ImGui::TextDisabled("%s", kLayoutPath);
	if (!layoutStatus_.empty()) ImGui::TextWrapped("%s", layoutStatus_.c_str());
	ImGui::Separator();

	const float hierarchyWidth = 230.0f;
	ImGui::BeginChild("##ImGuiStudioHierarchy", ImVec2(hierarchyWidth, 0), true);
	ImGui::TextUnformatted("ウィジェット階層");
	ImGui::Separator();

	// 並べ替えは描画ループ中にvectorを変更せず、ドラッグ元と移動先を
	// 一旦記録してループ終了後に反映する。
	int dragFrom = -1;
	int dragTo = -1;
	for (int i = 0; i < static_cast<int>(nodes_.size()); ++i) {
		auto& node = nodes_[i];
		ImGui::PushID(node.id);
		const std::string title =
			std::string(WidgetTypeName(node.type)) + "  " + node.label;
		if (ImGui::Selectable(title.c_str(), selectedNode_ == i)) {
			selectedNode_ = i;
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("IMGUI_STUDIO_NODE", &i, sizeof(i));
			ImGui::TextUnformatted(title.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("IMGUI_STUDIO_NODE")) {
				dragFrom = *static_cast<const int*>(payload->Data);
				dragTo = i;
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopID();
	}
	if (dragFrom >= 0 && dragTo >= 0 && dragFrom != dragTo) {
		WidgetNode moved = std::move(nodes_[dragFrom]);
		nodes_.erase(nodes_.begin() + dragFrom);
		nodes_.insert(nodes_.begin() + dragTo, std::move(moved));
		selectedNode_ = dragTo;
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginGroup();
	const float propertyWidth = 280.0f;

	// 選択中ノードの種類に必要な項目だけをプロパティとして表示する。
	ImGui::BeginChild("##ImGuiStudioProperties", ImVec2(propertyWidth, 0), true);
	ImGui::TextUnformatted("プロパティ");
	ImGui::Separator();
	if (selectedNode_ >= 0 && selectedNode_ < static_cast<int>(nodes_.size())) {
		auto& node = nodes_[selectedNode_];
		ImGui::Text("種類: %s", WidgetTypeName(node.type));
		YEditorWidget::InputText("ラベル", node.label);
		if (node.type == WidgetType::Checkbox ||
			node.type == WidgetType::SliderFloat ||
			node.type == WidgetType::InputText) {
			YEditorWidget::InputText("変数名", node.variable);
		}
		if (node.type == WidgetType::Checkbox) {
			YEditorWidget::Checkbox("プレビュー値", node.boolValue);
		}
		if (node.type == WidgetType::SliderFloat) {
			YEditorWidget::SliderFloat("プレビュー値", node.floatValue, 0.0f, 1.0f);
		}
		if (node.type == WidgetType::InputText) {
			YEditorWidget::InputText("プレビュー値", node.textValue);
		}

		if (ImGui::Button("上へ") && selectedNode_ > 0) {
			std::swap(nodes_[selectedNode_], nodes_[selectedNode_ - 1]);
			--selectedNode_;
		}
		ImGui::SameLine();
		if (ImGui::Button("下へ") &&
			selectedNode_ + 1 < static_cast<int>(nodes_.size())) {
			std::swap(nodes_[selectedNode_], nodes_[selectedNode_ + 1]);
			++selectedNode_;
		}
		ImGui::SameLine();
		if (ImGui::Button("削除")) {
			nodes_.erase(nodes_.begin() + selectedNode_);
			if (selectedNode_ >= static_cast<int>(nodes_.size())) {
				selectedNode_ = static_cast<int>(nodes_.size()) - 1;
			}
		}
	}
	else {
		ImGui::TextDisabled("左の階層から選択してください");
	}
	ImGui::EndChild();

	ImGui::SameLine();
	// nodes_の並びが、そのままプレビューと生成コードの描画順になる。
	ImGui::BeginChild("##ImGuiStudioPreview", ImVec2(0, 0), true);
	ImGui::TextUnformatted("ライブプレビュー");
	ImGui::Separator();
	for (auto& node : nodes_) DrawWidgetPreview(node);
	ImGui::EndChild();
	ImGui::EndGroup();
}

void ImGuiStudio::DrawWidgetPreview(WidgetNode& node)
{
	// ラベルが重複しても操作状態が衝突しないよう、永続IDをスコープに積む。
	ImGui::PushID(node.id);
	switch (node.type) {
	case WidgetType::Text:
		ImGui::TextUnformatted(node.label.c_str());
		break;
	case WidgetType::Button:
		ImGui::Button(node.label.c_str());
		break;
	case WidgetType::Checkbox:
		ImGui::Checkbox(node.label.c_str(), &node.boolValue);
		break;
	case WidgetType::SliderFloat:
		ImGui::SliderFloat(node.label.c_str(), &node.floatValue, 0.0f, 1.0f);
		break;
	case WidgetType::InputText:
		YEditorWidget::InputText(node.label.c_str(), node.textValue);
		break;
	case WidgetType::Separator:
		ImGui::Separator();
		break;
	case WidgetType::SameLine:
		ImGui::SameLine();
		break;
	}
	ImGui::PopID();
}

void ImGuiStudio::SaveTheme()
{
	try {
		const ImGuiStyle& style = ImGui::GetStyle();
		json j;

		// ImGuiStyleは自動シリアライズ対象ではないため、ユーザーが変更できる
		// レイアウト値と全カラーを明示的にJSONへ展開する。
		j["alpha"] = style.Alpha;
		j["disabledAlpha"] = style.DisabledAlpha;
		j["windowPadding"] = ToJson(style.WindowPadding);
		j["windowRounding"] = style.WindowRounding;
		j["windowBorderSize"] = style.WindowBorderSize;
		j["framePadding"] = ToJson(style.FramePadding);
		j["frameRounding"] = style.FrameRounding;
		j["frameBorderSize"] = style.FrameBorderSize;
		j["itemSpacing"] = ToJson(style.ItemSpacing);
		j["itemInnerSpacing"] = ToJson(style.ItemInnerSpacing);
		j["cellPadding"] = ToJson(style.CellPadding);
		j["indentSpacing"] = style.IndentSpacing;
		j["scrollbarSize"] = style.ScrollbarSize;
		j["scrollbarRounding"] = style.ScrollbarRounding;
		j["grabMinSize"] = style.GrabMinSize;
		j["grabRounding"] = style.GrabRounding;
		j["tabRounding"] = style.TabRounding;
		j["tabBorderSize"] = style.TabBorderSize;
		j["dockingSeparatorSize"] = style.DockingSeparatorSize;

		for (int i = 0; i < ImGuiCol_COUNT; ++i) {
			j["colors"][ImGui::GetStyleColorName(i)] = ToJson(style.Colors[i]);
		}

		EnsureParentDirectory(kThemePath);
		std::ofstream file(kThemePath);
		file << j.dump(4);
		themeStatus_ = "テーマを保存しました";
	}
	catch (const std::exception& e) {
		themeStatus_ = "テーマ保存失敗: " + std::string(e.what());
	}
}

void ImGuiStudio::LoadTheme()
{
	try {
		std::ifstream file(kThemePath);
		if (!file.is_open()) {
			themeStatus_ = "テーマファイルが見つかりません";
			return;
		}
		json j;
		file >> j;
		ImGuiStyle& style = ImGui::GetStyle();

		// valueの既定値に現在のスタイルを使い、将来JSON項目を追加しても
		// 古いテーマファイルを読み込めるようにする。
		style.Alpha = j.value("alpha", style.Alpha);
		style.DisabledAlpha = j.value("disabledAlpha", style.DisabledAlpha);
		if (j.contains("windowPadding")) LoadVec2(j["windowPadding"], style.WindowPadding);
		style.WindowRounding = j.value("windowRounding", style.WindowRounding);
		style.WindowBorderSize = j.value("windowBorderSize", style.WindowBorderSize);
		if (j.contains("framePadding")) LoadVec2(j["framePadding"], style.FramePadding);
		style.FrameRounding = j.value("frameRounding", style.FrameRounding);
		style.FrameBorderSize = j.value("frameBorderSize", style.FrameBorderSize);
		if (j.contains("itemSpacing")) LoadVec2(j["itemSpacing"], style.ItemSpacing);
		if (j.contains("itemInnerSpacing")) LoadVec2(j["itemInnerSpacing"], style.ItemInnerSpacing);
		if (j.contains("cellPadding")) LoadVec2(j["cellPadding"], style.CellPadding);
		style.IndentSpacing = j.value("indentSpacing", style.IndentSpacing);
		style.ScrollbarSize = j.value("scrollbarSize", style.ScrollbarSize);
		style.ScrollbarRounding = j.value("scrollbarRounding", style.ScrollbarRounding);
		style.GrabMinSize = j.value("grabMinSize", style.GrabMinSize);
		style.GrabRounding = j.value("grabRounding", style.GrabRounding);
		style.TabRounding = j.value("tabRounding", style.TabRounding);
		style.TabBorderSize = j.value("tabBorderSize", style.TabBorderSize);
		style.DockingSeparatorSize =
			j.value("dockingSeparatorSize", style.DockingSeparatorSize);

		if (j.contains("colors")) {
			for (int i = 0; i < ImGuiCol_COUNT; ++i) {
				const char* name = ImGui::GetStyleColorName(i);
				if (j["colors"].contains(name)) {
					LoadVec4(j["colors"][name], style.Colors[i]);
				}
			}
		}
		themeStatus_ = "テーマを読み込みました";
	}
	catch (const std::exception& e) {
		themeStatus_ = "テーマ読込失敗: " + std::string(e.what());
	}
}

void ImGuiStudio::SaveLayout()
{
	try {
		json j;
		j["widgets"] = json::array();

		// vectorの順番を維持して保存する。読み込み後もこの順番が
		// プレビューとC++生成結果の描画順として復元される。
		for (const auto& node : nodes_) {
			j["widgets"].push_back({
				{ "id", node.id },
				{ "type", static_cast<int>(node.type) },
				{ "label", node.label },
				{ "variable", node.variable },
				{ "boolValue", node.boolValue },
				{ "floatValue", node.floatValue },
				{ "textValue", node.textValue }
			});
		}
		EnsureParentDirectory(kLayoutPath);
		std::ofstream file(kLayoutPath);
		file << j.dump(4);
		layoutStatus_ = "レイアウトを保存しました";
	}
	catch (const std::exception& e) {
		layoutStatus_ = "レイアウト保存失敗: " + std::string(e.what());
	}
}

void ImGuiStudio::LoadLayout()
{
	try {
		std::ifstream file(kLayoutPath);
		if (!file.is_open()) {
			layoutStatus_ = "レイアウトファイルが見つかりません";
			return;
		}
		json j;
		file >> j;
		nodes_.clear();
		nextNodeId_ = 1;

		// 不正なtype値は有効範囲へ丸め、欠けた項目には安全な初期値を使う。
		for (const auto& item : j.value("widgets", json::array())) {
			WidgetNode node;
			node.id = item.value("id", nextNodeId_);
			const int type = std::clamp(
				item.value("type", 0), 0, static_cast<int>(WidgetType::SameLine));
			node.type = static_cast<WidgetType>(type);
			node.label = item.value("label", std::string(WidgetTypeName(node.type)));
			node.variable = item.value("variable", "value");
			node.boolValue = item.value("boolValue", false);
			node.floatValue = item.value("floatValue", 0.5f);
			node.textValue = item.value("textValue", "");

			// 追加するノードのIDが読み込み済みIDと重複しないよう更新する。
			nextNodeId_ = (std::max)(nextNodeId_, node.id + 1);
			nodes_.push_back(std::move(node));
		}
		selectedNode_ = nodes_.empty() ? -1 : 0;
		layoutStatus_ = "レイアウトを読み込みました";
	}
	catch (const std::exception& e) {
		layoutStatus_ = "レイアウト読込失敗: " + std::string(e.what());
	}
}

void ImGuiStudio::CopyThemeCpp() const
{
	const ImGuiStyle& style = ImGui::GetStyle();
	std::ostringstream out;

	// 外部のテーマ形式へ依存しない、Dear ImGuiだけで使えるコードを生成する。
	out << "ImGuiStyle& style = ImGui::GetStyle();\n";
	out << "style.Alpha = " << style.Alpha << "f;\n";
	out << "style.WindowRounding = " << style.WindowRounding << "f;\n";
	out << "style.FrameRounding = " << style.FrameRounding << "f;\n";
	out << "style.GrabRounding = " << style.GrabRounding << "f;\n";
	out << "style.TabRounding = " << style.TabRounding << "f;\n";
	for (int i = 0; i < ImGuiCol_COUNT; ++i) {
		const ImVec4& c = style.Colors[i];
		out << "style.Colors[" << i << "] = ImVec4("
			<< c.x << "f, " << c.y << "f, "
			<< c.z << "f, " << c.w << "f); // "
			<< ImGui::GetStyleColorName(i) << "\n";
	}
	ImGui::SetClipboardText(out.str().c_str());
}

void ImGuiStudio::CopyLayoutCpp() const
{
	std::ostringstream out;
	out << "void DrawGeneratedPanel()\n{\n";

	// 編集中の順番で標準Dear ImGui呼び出しへ変換する。
	// Checkbox等が参照する変数の宣言は利用側の責務とし、ここでは描画関数だけを出力する。
	for (const auto& node : nodes_) {
		const std::string label = EscapeCppString(node.label);
		const std::string var = SanitizeIdentifier(node.variable);
		switch (node.type) {
		case WidgetType::Text:
			out << "\tImGui::TextUnformatted(\"" << label << "\");\n";
			break;
		case WidgetType::Button:
			out << "\tif (ImGui::Button(\"" << label << "\")) {\n\t\t// event\n\t}\n";
			break;
		case WidgetType::Checkbox:
			out << "\tImGui::Checkbox(\"" << label << "\", &" << var << ");\n";
			break;
		case WidgetType::SliderFloat:
			out << "\tImGui::SliderFloat(\"" << label << "\", &" << var
				<< ", 0.0f, 1.0f);\n";
			break;
		case WidgetType::InputText:
			out << "\tImGui::InputText(\"" << label << "\", " << var
				<< ", sizeof(" << var << "));\n";
			break;
		case WidgetType::Separator:
			out << "\tImGui::Separator();\n";
			break;
		case WidgetType::SameLine:
			out << "\tImGui::SameLine();\n";
			break;
		}
	}
	out << "}\n";
	ImGui::SetClipboardText(out.str().c_str());
}

void ImGuiStudio::AddWidget(WidgetType type)
{
	WidgetNode node;

	// ImGui ID用の番号は削除後も再利用せず、同一セッション中の衝突を防ぐ。
	node.id = nextNodeId_++;
	node.type = type;
	node.label = WidgetTypeName(type);
	node.variable = "value" + std::to_string(node.id);
	nodes_.push_back(std::move(node));
	selectedNode_ = static_cast<int>(nodes_.size()) - 1;
}

const char* ImGuiStudio::WidgetTypeName(WidgetType type)
{
	switch (type) {
	case WidgetType::Text: return "Text";
	case WidgetType::Button: return "Button";
	case WidgetType::Checkbox: return "Checkbox";
	case WidgetType::SliderFloat: return "SliderFloat";
	case WidgetType::InputText: return "InputText";
	case WidgetType::Separator: return "Separator";
	case WidgetType::SameLine: return "SameLine";
	}
	return "Unknown";
}

std::string ImGuiStudio::SanitizeIdentifier(const std::string& value)
{
	// 入力された表示名を、そのまま生成C++の識別子として利用できる形へ整える。
	std::string result;
	result.reserve(value.size() + 1);
	for (const unsigned char c : value) {
		result.push_back(std::isalnum(c) || c == '_' ? static_cast<char>(c) : '_');
	}
	if (result.empty()) result = "value";
	if (std::isdigit(static_cast<unsigned char>(result.front()))) {
		result.insert(result.begin(), '_');
	}
	return result;
}

std::string ImGuiStudio::EscapeCppString(const std::string& value)
{
	// ラベルをC++文字列リテラルへ埋め込むため、最低限必要な文字をエスケープする。
	std::string result;
	for (const char c : value) {
		if (c == '\\' || c == '"') result.push_back('\\');
		if (c == '\n') {
			result += "\\n";
		}
		else {
			result.push_back(c);
		}
	}
	return result;
}

#endif // USE_IMGUI
