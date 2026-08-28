#pragma once
// ===========================================================
// YEditorWidget_Material.h
//
// マテリアル編集ウィジェット。
//
// このエンジンではモデル (＝ Material) が複数オブジェクトで共有されるため、
// 個別のオブジェクトで色や粗さを変えるには「上書き」という考え方になる。
// Unreal のマテリアルインスタンスと同じで、各項目は
//   チェックOFF … モデル本来の値 (Blender で設定した値) をそのまま使う
//   チェックON  … このオブジェクト専用の値を使う
// という UI にする。ここではその 1 項目ぶんの見た目を共通化する。
//
// テクスチャ選択そのものは FileBrowser (フレーム跨ぎの状態を持つ) が担うので
// ここでは持たない。ボタンが押されたことだけを呼び出し側へ返し、
// ブラウザの生成・表示はパネル側 (MaterialPanel) が行う。
// ===========================================================
#ifdef USE_IMGUI
#include "Material/MaterialOverrideSet.h"
#include "Vector3.h"
#include <imgui.h>
#include <string>

class Material;

namespace YEditorWidget {

// ── 上書きトグル付きの入力 ────────────────────────────────────
// チェックボックス + 値。チェックが OFF の間はベース値を灰色表示する。
// いずれかが変化したら true。

bool OverrideFloat(const char *label, bool &enabled, float &value,
                   float baseValue, float vmin, float vmax,
                   const char *fmt = "%.3f");

bool OverrideColor3(const char *label, bool &enabled, Vector3 &value,
                    const Vector3 &baseValue);

// ── マテリアルスロット 1 枚ぶんの描画結果 ─────────────────────
struct MaterialSlotResult {
  bool changed = false; // 色・粗さ・メタリック・放射のどれかが変わった
  bool requestOpenTexture = false; // 「テクスチャを開く」が押された
  bool clearTexture = false;       // 「元に戻す」が押された
};

// ── マテリアルスロット 1 枚ぶんのエディタ ─────────────────────
class MaterialSlotEditor {
public:
  // 1 スロットぶんを描画する。
  // baseMaterial はモデル本来の値の表示用（null 可）。
  // currentTexture は現在バインドされているテクスチャの表示用。
  MaterialSlotResult Draw(const char *id, MeshMaterialOverride &slot,
                          const Material *baseMaterial,
                          const std::string &currentTexture);
};

} // namespace YEditorWidget
#endif // USE_IMGUI
