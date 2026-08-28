#pragma once

#ifdef USE_IMGUI

// Engine
#include "ScenePanelContext.h"
#include <Core/Editor/Widgets/YEditorWidget.h>

// C++
#include <string>
#include <vector>

namespace YoRigine {

/// <summary>
/// アウトライナ (オブジェクト一覧) パネル。
///
/// Unity の Hierarchy 相当。以前の「ID 順バラバラの Selectable
/// 羅列」から、
/// 検索・表示トグル・選択ロック・階層インデント・右クリックメニューを
/// 備えたものに置き換えている。
///
/// 表示順は ObjectManager
/// の内部が unordered_map で不定なため、必ず ID 昇順に 並べ替えてから描く
/// (フレームごとに行が入れ替わると選択操作が成立しない)。
/// </summary>
class OutlinerPanel {
public:
  void Draw(const ScenePanelContext &context);

private:
  // 親を持たないオブジェクトを根として、階層順に 1 行ずつ描く
  void DrawRow(const ScenePanelContext &context,
               ObjectManager::PlacedObject &obj, int depth);

  // 検索中は階層を無視してフラットに並べる (絞り込み結果が見やすいため)
  void DrawFlatList(const ScenePanelContext &context);
  void DrawHierarchy(const ScenePanelContext &context);

  void DeleteSelection(const ScenePanelContext &context);

  YEditorWidget::SearchBox searchBox_;

  // 名前変更中のオブジェクト (-1 で非編集)
  int renamingId_ = -1;
  char renameBuffer_[128] = {};

  // 折りたたみ状態。ID をキーに保持する。
  std::vector<int> collapsedIds_;

  bool IsCollapsed(int id) const;
  void SetCollapsed(int id, bool collapsed);
};

} // namespace YoRigine

#endif // USE_IMGUI
