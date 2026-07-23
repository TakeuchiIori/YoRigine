#pragma once
// ===========================================================
// VfxGeometryTypes.h
//
// ジオメトリの「型・パラメータ・記述子」を1か所に集約するヘッダ。
// variant で使う全パラメータ構造体をここに置くことで、
// 各 Geometry クラスとの循環インクルードを避ける。
//
// 新しいジオメトリを追加するとき:
//   1. XxxGeomParams をここに追加し VfxGeometryParams variant に足す
//   2. VfxGeometryType に列挙子を足す
//   3. XxxGeometry::Describe() を実装
//   4. GeometryRegistry に Register する（Spawner 初期化で1行）
// ===========================================================
#include <variant>
#include <functional>
#include <memory>
#include "VfxGeometryParams.h" // 型 ID + パラメータ variant（軽量）
#include "VfxGeometry.h"       // std::function<unique_ptr<VfxGeometry>(...)> は完全型が必要

namespace YoRigine {

    // ===========================================================
    // VfxGeometryDesc
    // 1つのジオメトリ型の全情報（生成・デフォルト・エディタ UI）。
    // 各 Geometry クラスの static Describe() がこれを返す。
    // ===========================================================
    struct VfxGeometryDesc
    {
        VfxGeometryType   type        = {};
        const char*       displayName = "";
        VfxGeometryParams defaultParams; ///< エディタで型を切り替えたときの初期値

        /// params（variant）から VfxGeometry を生成する
        std::function<std::unique_ptr<VfxGeometry>(const VfxGeometryParams&)> create;

#ifdef USE_IMGUI
        /// params の ImGui UI を描画する。変更があれば true を返す
        std::function<bool(VfxGeometryParams&)> drawUI;
#endif
    };

} // namespace YoRigine
