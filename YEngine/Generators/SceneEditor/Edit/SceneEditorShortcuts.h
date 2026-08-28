#pragma once

#ifdef USE_IMGUI

// C++
#include <functional>

namespace YoRigine {

/// <summary>
/// シーンエディタのキーボードショートカットを一手に引き受けるクラス。
///
/// 「どのキーで何が起きるか」がこのファイルだけを見れば分かる状態を保つのが
/// 目的。各操作の中身は呼び出し側からコールバックで注入する。
///
/// 現在の割り当て:
///   Ctrl+S : 保存        Ctrl+C / Ctrl+V : コピー / 貼り付け
///   Ctrl+G : 地面に吸着  Ctrl+D          : 選択中を複製
///   Delete : 選択中を削除 B              : スタンプ配置を開始
///   F      : 選択中にフォーカス           Esc : スタンプ配置を終了
/// </summary>
class SceneEditorShortcuts {
public:
  struct Actions {
    std::function<void()> save;
    std::function<void()> copy;
    std::function<void()> paste;
    std::function<void()> duplicate;
    std::function<void()> deleteSelection;
    std::function<void()> snapToSurface;
    std::function<void()> focusSelection;
    std::function<void()> startStamp;
    std::function<void()> exitStamp;
  };

  void SetActions(Actions actions) { actions_ = std::move(actions); }

  // エディタが有効なフレームで毎回呼ぶ。
  // isEditorActive が false のときは Ctrl+S だけを受け付ける
  // (シーンを開いていれば保存はいつでもできてほしいため)。
  // hasSelection / isStamping
  // は「今そのショートカットが意味を持つか」の判定用。
  void Update(bool isEditorActive, bool hasSelection, bool isStamping);

private:
  Actions actions_;
};

} // namespace YoRigine

#endif // USE_IMGUI
