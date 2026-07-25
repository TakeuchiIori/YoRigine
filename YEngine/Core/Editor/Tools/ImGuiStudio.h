#pragma once

#ifdef USE_IMGUI

#include <imgui.h>

#include <string>
#include <vector>

/// ImThemes / ImRAD / ImStudio の主要ワークフローをYoRigine内に統合した
/// Dear ImGui向けテーマ・簡易レイアウトデザイナー。
///
/// 主な機能:
/// - ImGuiStyleをリアルタイム編集し、JSONへ保存・復元する
/// - ウィジェットを一覧へ追加して、順番とプロパティを編集する
/// - 編集中のレイアウトをその場でプレビューする
/// - テーマとレイアウトをDear ImGuiのC++コードとして出力する
///
/// エディタ専用機能なので、USE_IMGUIが有効な構成でのみ定義される。
/// 保存先はResources/Json/Editor以下で、ゲームの実行ディレクトリを基準とする。
class ImGuiStudio
{
public:
	/// エディタ登録箇所と描画箇所で同じ編集状態を共有する。
	static ImGuiStudio* GetInstance();

	/// 起動時のYoRigineテーマを退避し、保存済みデータがあれば読み込む。
	/// 複数回呼ばれても初期化は一度しか行わない。
	void Initialize();

	/// テーマ、UIレイアウト、参考ツールの各タブを描画する。
	void Draw();

private:
	/// レイアウトエディタで配置できるDear ImGuiウィジェットの種類。
	/// 列挙順は追加用コンボボックスとJSONのtype値に対応している。
	enum class WidgetType {
		Text,
		Button,
		Checkbox,
		SliderFloat,
		InputText,
		Separator,
		SameLine
	};

	/// レイアウト上のウィジェット1個分の編集データ。
	///
	/// idはImGui IDとドラッグ並べ替えの識別に使用する。
	/// boolValue、floatValue、textValueはライブプレビュー用の一時値であり、
	/// 生成コードではvariableに指定された変数名を参照する。
	struct WidgetNode {
		int id = 0;
		WidgetType type = WidgetType::Text;
		std::string label = "Text";
		std::string variable = "value";
		bool boolValue = false;
		float floatValue = 0.5f;
		std::string textValue;
	};

	ImGuiStudio() = default;

	// 各編集画面の描画
	void DrawThemeEditor();
	void DrawLayoutEditor();
	void DrawWidgetPreview(WidgetNode& node);

	// JSONへの永続化。読み込み時は存在する項目だけを現在値へ反映する。
	void SaveTheme();
	void LoadTheme();
	void SaveLayout();
	void LoadLayout();

	// 現在の編集内容を標準Dear ImGui C++としてクリップボードへ出力する。
	void CopyThemeCpp() const;
	void CopyLayoutCpp() const;

	// レイアウト操作とコード生成用の文字列補助
	void AddWidget(WidgetType type);
	static const char* WidgetTypeName(WidgetType type);
	static std::string SanitizeIdentifier(const std::string& value);
	static std::string EscapeCppString(const std::string& value);

	// 実行ディレクトリから見た保存先。親ディレクトリは保存時に自動作成する。
	static constexpr const char* kThemePath =
		"Resources/Json/Editor/ImGuiTheme.json";
	static constexpr const char* kLayoutPath =
		"Resources/Json/Editor/ImGuiLayout.json";

	// YoRigineプリセットへ戻すため、Initialize時点のスタイルを保持する。
	ImGuiStyle startupStyle_{};

	// レイアウトは描画順と同じ順番で保持する。
	std::vector<WidgetNode> nodes_;
	int selectedNode_ = -1;
	int nextNodeId_ = 1;
	int addTypeIndex_ = 0;
	bool initialized_ = false;

	// 保存・読み込み結果を各タブへ表示するためのメッセージ。
	std::string themeStatus_;
	std::string layoutStatus_;
};

#endif // USE_IMGUI
