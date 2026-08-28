#ifdef USE_IMGUI

#include "SceneEditorShortcuts.h"

#include <imgui.h>

namespace YoRigine {

namespace {
// コールバックが設定されていれば呼ぶ
void Invoke(const std::function<void()> &action) {
  if (action) {
    action();
  }
}
} // namespace

void SceneEditorShortcuts::Update(bool isEditorActive, bool hasSelection,
                                  bool isStamping) {
  ImGuiIO &io = ImGui::GetIO();

  // 「実際にテキスト入力中のときだけ」ブロックしたいので WantTextInput を使う。
  // WantCaptureKeyboard は ImGui ウィンドウにフォーカスがあるだけで true
  // になり、 シーンエディタ表示中はほぼ常に true
  // になって全ショートカットを潰してしまう。
  if (io.WantTextInput) {
    return;
  }

  // 保存だけはエディタ非アクティブでも受け付ける
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
    Invoke(actions_.save);
  }

  if (!isEditorActive) {
    return;
  }

  if (io.KeyCtrl) {
    if (ImGui::IsKeyPressed(ImGuiKey_C)) {
      Invoke(actions_.copy);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_V)) {
      Invoke(actions_.paste);
    }
    if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_D)) {
      Invoke(actions_.duplicate);
    }
    if (hasSelection && ImGui::IsKeyPressed(ImGuiKey_G)) {
      Invoke(actions_.snapToSurface);
    }
    return;
  }

  // ── 修飾キーなし ──
  if (isStamping) {
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      Invoke(actions_.exitStamp);
    }
    return;
  }

  if (hasSelection) {
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
      Invoke(actions_.deleteSelection);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
      Invoke(actions_.focusSelection);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_B)) {
      Invoke(actions_.startStamp);
    }
  }
}

} // namespace YoRigine

#endif // USE_IMGUI
