#pragma once
// ===========================================================
// VfxMaterialTypes.h
//
// マテリアルの「型・パラメータ・記述子」を1か所に集約するヘッダ。
// ジオメトリ側（VfxGeometryTypes.h）と対になる構成。
//
// 新しいマテリアルを追加するとき:
//   1. XxxMatParams をここに追加し VfxMaterialParams variant に足す
//   2. VfxMaterialType に列挙子を足す
//   3. XxxMaterial::Describe() を実装
//   4. MaterialRegistry に Register する（Spawner 初期化で1行）
// ===========================================================
#include <variant>
#include <functional>
#include <memory>
#include "MathFunc.h"
#include "VfxMaterialParams.h" // 型 ID + パラメータ variant（軽量）
#include "VfxMaterial.h"       // std::function<unique_ptr<VfxMaterial>(...)> は完全型が必要

namespace YoRigine {

    // ===========================================================
    // VfxMaterialDesc
    // 1つのマテリアル型の全情報（生成・デフォルト・エディタ UI）。
    // ===========================================================
    struct VfxMaterialDesc
    {
        VfxMaterialType   type        = {};
        const char*       displayName = "";
        VfxMaterialParams defaultParams;

        /// params（variant）から VfxMaterial を生成する
        std::function<std::unique_ptr<VfxMaterial>(const VfxMaterialParams&)> create;

#ifdef USE_IMGUI
        /// params の ImGui UI を描画する。変更があれば true を返す
        std::function<bool(VfxMaterialParams&)> drawUI;
#endif
    };

} // namespace YoRigine
