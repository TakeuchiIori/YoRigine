#pragma once

#ifdef USE_IMGUI

// Engine
#include "ScenePanelContext.h"
#include <Core/Editor/Widgets/YEditorWidget.h>
#include <FileOperations/FileBrowser.h>

// C++
#include <memory>
#include <string>

namespace YoRigine {

/// <summary>
/// マテリアル編集パネル。
///
/// 選択中オブジェクトのマテリアルをメッシュ (マテリアルスロット)
/// 単位で編集する。 静的オブジェクト (インスタンシング描画)
/// でも動的オブジェクト (個別描画) でも 同じ経路で効く。
///
/// 各値は「上書きするか」のチェックとセットになっている。チェックを外すと
/// Blender で設定した粗さ・メタリックがそのまま使われる。
///
/// テクスチャ差し替えは共通の FileBrowser で行う。ブラウザのサムネイル表示が
/// 走査時にテクスチャを読み込む (＝ GPU
/// へアップロードする) ため、選択した瞬間には 既にバインド可能になっており、1
/// フレーム白くなる問題が起きない。
/// </summary>
class MaterialPanel {
public:
  void Draw(const ScenePanelContext &context);

private:
  // 1 スロットぶんの見出し + エディタ本体
  void DrawSlot(const ScenePanelContext &context,
                ObjectManager::PlacedObject &obj, size_t slotIndex);

  // モデル本来の値 (Blender からの読み込み結果) を読み取り専用で表示する
  void DrawSourceInfo(const ObjectManager::PlacedObject &obj);

  // テクスチャ選択ブラウザ (モーダル)
  // を描画し、選択結果を対象スロットへ反映する
  void DrawTextureBrowser(const ScenePanelContext &context,
                          ObjectManager::PlacedObject &obj);

  // FileBrowser を必要になった時点で生成する (コンストラクタは USE_IMGUI 前提)
  void EnsureBrowser();

  YEditorWidget::MaterialSlotEditor slotEditor_;

  // テクスチャ選択ブラウザ。フレーム跨ぎの状態 (現在フォルダ等)
  // を持つので保持。
  std::unique_ptr<FileBrowser> textureBrowser_;
  bool showTextureBrowser_ = false;
  int browserTargetSlot_ = -1;    // どのスロットへ適用するか
  std::string pickedTexturePath_; // コールバックが書き込む選択結果
};

} // namespace YoRigine

#endif // USE_IMGUI
