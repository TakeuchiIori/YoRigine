#pragma once
// ===========================================================
// YEditorWidget_AssetPicker.h
//
// アセット選択まわりの共通ウィジェット。
// 検索ボックス、部分一致フィルタ、フォルダ走査を 1 箇所にまとめる。
//
// 使い方:
//   YEditorWidget::SearchBox searchBox;
//   searchBox.Draw("##modelSearch");
//   for (auto& path : paths) {
//       if (!searchBox.Matches(path)) continue;
//       ...
//   }
// ===========================================================
#ifdef USE_IMGUI
#include <imgui.h>
#include <string>
#include <string_view>
#include <vector>

namespace YEditorWidget {

// ── 検索ボックス ──────────────────────────────────────────────
// 入力文字列を保持し、任意の文字列がヒットするか判定できる。
class SearchBox {
public:
  // 検索欄を描画する。文字列が変化したら true。
  bool Draw(const char *id, const char *hint = "検索...", float width = -1.0f);

  // 現在の検索語に text がマッチするか（大文字小文字を無視した部分一致）。
  // 検索語が空なら常に true。
  bool Matches(std::string_view text) const;

  bool IsEmpty() const { return query_.empty(); }
  const std::string &GetQuery() const { return query_; }
  void Clear() { query_.clear(); }

private:
  std::string query_;
  char buffer_[128] = {};
};

// 大文字小文字を無視した部分一致。SearchBox を使わない場所からも呼べる。
bool MatchesFilter(std::string_view text, std::string_view query);

// ── アセット選択コンボ ────────────────────────────────────────
// candidates から 1 つ選ぶ。選択が変わったら true。
// allowEmpty が true なら「(なし)」で currentPath を空に戻せる。
bool AssetCombo(const char *label, std::string &currentPath,
                const std::vector<std::string> &candidates,
                bool allowEmpty = true, const char *emptyLabel = "(なし)");

// 検索付きのアセット選択ポップアップ。候補が多いテクスチャ選択向け。
// 選択が確定したら true。
class AssetPickerPopup {
public:
  // ボタン + ポップアップをまとめて描画する。
  // buttonLabel を押すと候補一覧が開き、選ぶと currentPath が書き換わる。
  bool Draw(const char *id, const char *buttonLabel, std::string &currentPath,
            const std::vector<std::string> &candidates, bool allowEmpty = true);

private:
  SearchBox searchBox_;
};

// ── フォルダ走査 ──────────────────────────────────────────────
// folder 以下を再帰的に探し、extensions
// のいずれかで終わるファイルパスを返す。 extensions は ".png"
// のようにドット込み・小文字で渡す。
std::vector<std::string>
ScanAssetFiles(const std::string &folder,
               const std::vector<std::string> &extensions);

} // namespace YEditorWidget
#endif // USE_IMGUI
