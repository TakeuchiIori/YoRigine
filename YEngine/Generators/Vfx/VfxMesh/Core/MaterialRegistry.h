#pragma once
// ===========================================================
// MaterialRegistry.h
//
// VfxMaterialType → VfxMaterialDesc のマッピングを管理するシングルトン。
// GeometryRegistry と対になる。
// ===========================================================
#include <vector>
#include "VfxMaterialTypes.h"

namespace YoRigine {

    class MaterialRegistry
    {
    public:
        static MaterialRegistry& Instance();

        /// VfxMaterialDesc を登録する（同一 type は上書き）
        void Register(VfxMaterialDesc desc);

        /// type + params から VfxMaterial を生成する。未登録なら nullptr
        std::unique_ptr<VfxMaterial> Create(
            VfxMaterialType type, const VfxMaterialParams& params) const;

        /// 登録済み全記述子（エディタのドロップダウン生成用）
        const std::vector<VfxMaterialDesc>& GetAll() const { return descs_; }

        /// type に対応する記述子を返す。未登録なら nullptr
        const VfxMaterialDesc* FindDesc(VfxMaterialType type) const;

        /// type のデフォルトパラメータを返す
        VfxMaterialParams MakeDefault(VfxMaterialType type) const;

#ifdef USE_IMGUI
        /// type に対応する drawUI を呼ぶ。変更があれば true
        bool DrawUI(VfxMaterialType type, VfxMaterialParams& params) const;
#endif

    private:
        std::vector<VfxMaterialDesc> descs_;
    };

} // namespace YoRigine
