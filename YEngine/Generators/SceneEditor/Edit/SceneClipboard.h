#pragma once

// Engine
#include "../Core/SceneEditorContext.h"

// Math
#include "Vector3.h"

// C++
#include <vector>

namespace YoRigine {

/// <summary>
/// 選択中オブジェクトのコピー / 貼り付けを担うクラス。
///
/// コピーは ID を控えるだけで、実体の複製は貼り付け時に行う。
/// 貼り付けではトランスフォーム・コライダー設定・マテリアル・親子関係まで
/// 引き継ぐ (どれか 1
/// つでも漏れると「見た目は同じなのに当たらない複製」が生まれるため)。
/// </summary>
class SceneClipboard {
public:
  explicit SceneClipboard(const SceneEditorContext &context)
      : context_(context) {}

  // 選択中のオブジェクト ID をコピーバッファへ取り込む
  void Copy();

  // コピーバッファの内容を offset ぶんずらして複製し、選択状態にする
  void Paste();

  bool IsEmpty() const { return copiedObjectIds_.empty(); }
  void Clear() { copiedObjectIds_.clear(); }

  void SetPasteOffset(const Vector3 &offset) { pasteOffset_ = offset; }
  const Vector3 &GetPasteOffset() const { return pasteOffset_; }

private:
  const SceneEditorContext &context_;

  std::vector<int> copiedObjectIds_;
  Vector3 pasteOffset_ = {1.0f, 0.0f, 0.0f};
};

} // namespace YoRigine
