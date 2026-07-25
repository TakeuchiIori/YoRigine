#pragma once
// ===========================================================
// VfxMeshRegistry.h
//
// VfxElementType → ProceduralMeshBase 生成 + エディタ UI のマッピング。
//
// 新しいエフェクト型を追加するときは:
//   1. XxxMesh.h/.cpp を作る
//   2. XxxMesh::Describe() を実装する
//   3. VfxMeshSpawner::Initialize() で reg.Register(XxxMesh::Describe()) を1行追加
//   それ以外のファイルは一切触らない。
// ===========================================================
#include <functional>
#include <memory>
#include <vector>
#include "VfxEffectAsset.h"
#include "ProceduralMeshBase.h"

class Camera;

namespace YoRigine {

// ===========================================================
// VfxElementDesc
//
// 1つのエフェクト型の「全情報」を持つ記述子。
// 各メッシュクラスの static Describe() がこれを返す。
// ===========================================================
struct VfxElementDesc
{
    VfxElementType type        = {};
    const char*    displayName = "";

    // ── ファクトリ ────────────────────────────────────────────
    // def.type に対応する ProceduralMeshBase を生成して返す関数
    using CreateFn = std::function<
        std::unique_ptr<ProceduralMeshBase>(
            const VfxElement& def,
            const Vector3&    basePos,
            float             scale,
            Camera*           camera)>;
    CreateFn create;

    // ── デフォルト値設定 ──────────────────────────────────────
    // DrawElementsSection でエレメントを追加するときに呼ばれる。
    // モジュールのデフォルト設定などをここで行う（省略可）
    std::function<void(VfxElement&)> applyDefaults;

#ifdef USE_IMGUI
    // ── エディタ UI ──────────────────────────────────────────
    // CommitFn: before スナップショットとラベルを受け取り UndoRedo に積む
    using CommitFn = std::function<void(const VfxEffectAsset& before, const char* label)>;
    // DrawUIFn: エレメントパラメータの ImGui UI を描画する
    using DrawUIFn = std::function<
        void(VfxElement& sub, const VfxEffectAsset& asset, const CommitFn& commit)>;
    DrawUIFn drawUI;
#endif
};

// ===========================================================
// VfxMeshRegistry
//
// シングルトン。VfxElementDesc を型ごとに管理し、
// Spawner と Editor の両方が型の具体クラスを知らずに
// 生成・UI描画を行えるようにする。
// ===========================================================
class VfxMeshRegistry
{
public:
    static VfxMeshRegistry& Instance();

    /// VfxElementDesc を登録する（Spawner の Initialize で呼ぶ）
    void Register(VfxElementDesc desc);

    /// def.type に対応する Mesh を生成して返す。未登録なら nullptr
    std::unique_ptr<ProceduralMeshBase> Create(
        const VfxElement& def, const Vector3& basePos, float scale, Camera* camera) const;

    /// 登録済み全記述子（エディタのメニュー生成などに使う）
    const std::vector<VfxElementDesc>& GetAll() const { return descs_; }

    /// type に対応する記述子を返す。未登録なら nullptr
    const VfxElementDesc* FindDesc(VfxElementType type) const;

#ifdef USE_IMGUI
    /// type に対応する drawUI を呼ぶ。未登録なら何もしない
    void DrawUI(VfxElementType type, VfxElement& sub,
                const VfxEffectAsset& asset,
                const VfxElementDesc::CommitFn& commit) const;
#endif

private:
    std::vector<VfxElementDesc> descs_;
};

} // namespace YoRigine
