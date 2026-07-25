#pragma once
// ===========================================================
// GeometryRegistry.h
//
// VfxGeometryType → VfxGeometryDesc のマッピングを管理するシングルトン。
// 型の具体クラスを知らずにジオメトリを生成・UI 描画できる。
// MaterialRegistry と対になる。
// ===========================================================
#include <vector>
#include "VfxGeometryTypes.h"

namespace YoRigine {

    class GeometryRegistry
    {
    public:
        static GeometryRegistry& Instance();

        /// VfxGeometryDesc を登録する（同一 type は上書き）
        void Register(VfxGeometryDesc desc);

        /// type + params から VfxGeometry を生成する。未登録なら nullptr
        std::unique_ptr<VfxGeometry> Create(
            VfxGeometryType type, const VfxGeometryParams& params) const;

        /// 登録済み全記述子（エディタのドロップダウン生成用）
        const std::vector<VfxGeometryDesc>& GetAll() const { return descs_; }

        /// type に対応する記述子を返す。未登録なら nullptr
        const VfxGeometryDesc* FindDesc(VfxGeometryType type) const;

        /// type のデフォルトパラメータを返す（型切り替え時の初期値）
        VfxGeometryParams MakeDefault(VfxGeometryType type) const;

#ifdef USE_IMGUI
        /// type に対応する drawUI を呼ぶ。変更があれば true
        bool DrawUI(VfxGeometryType type, VfxGeometryParams& params) const;
#endif

    private:
        std::vector<VfxGeometryDesc> descs_;
    };

} // namespace YoRigine
