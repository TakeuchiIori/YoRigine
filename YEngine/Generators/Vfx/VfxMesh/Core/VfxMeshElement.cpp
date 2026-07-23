// ===========================================================
// VfxMeshElement.cpp
// ===========================================================
#include "VfxMeshElement.h"
#include "VfxEvalState.h"
#include "GeometryRegistry.h"
#include "MaterialRegistry.h"
#include <d3d12.h>

namespace YoRigine {

// ===========================================================
// 構築
// ===========================================================

VfxMeshElement::VfxMeshElement(
    std::unique_ptr<VfxGeometry> geometry,
    std::unique_ptr<VfxMaterial> material)
    : geometry_(std::move(geometry))
    , material_(std::move(material))
{
    InitBuffer(2048);
    vertices_.reserve(2048);
}

std::unique_ptr<VfxMeshElement> VfxMeshElement::CreateComposed(
    VfxGeometryType geomType, const VfxGeometryParams& geomParams,
    VfxMaterialType matType,  const VfxMaterialParams& matParams,
    Camera* camera)
{
    auto geometry = GeometryRegistry::Instance().Create(geomType, geomParams);
    auto material = MaterialRegistry::Instance().Create(matType, matParams);
    if (!geometry || !material) return nullptr;

    auto elem = std::make_unique<VfxMeshElement>(std::move(geometry), std::move(material));
    elem->SetCamera(camera);
    return elem;
}

// ===========================================================
// 毎フレーム更新
// ===========================================================

void VfxMeshElement::Drive(const VfxEvalState& state)
{
    prevGeomState_ = geomState_;
    geomState_.position = state.position;
    geomState_.scale    = state.scale;
    geomState_.rotation = state.rotation;
    geomState_.progress = state.progress;
}

void VfxMeshElement::Update(float deltaTime)
{
    time_ += deltaTime;
    geomState_.time = time_;

    if (forceRebuild_ || geometry_->IsDirty(prevGeomState_, geomState_)) {
        vertices_.clear();
        geometry_->Build(vertices_, geomState_);
        UploadVertices(vertices_);
        prevGeomState_ = geomState_;
        forceRebuild_  = false;
    }
}

// ===========================================================
// 描画
// ===========================================================

void VfxMeshElement::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!isVisible_) return;
    const uint32_t vertCount = GetVertexCount();
    if (vertCount < 3) return;

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    BindVertexBuffer(cmdList);
    cmdList->DrawInstanced(vertCount, 1, 0, 0);
}

// ===========================================================
// 定数バッファ
// ===========================================================

void VfxMeshElement::FillCB(void* mapped, const CBFillArgs& args) const
{
    // CBFillArgs（旧 API）から VfxMatFillArgs（新 API）に変換してマテリアルに委譲。
    // フレネル中心・ワールド半径のためジオメトリの現在トランスフォームも渡す。
    VfxMatFillArgs matArgs{
        args.tint,
        args.age,
        args.burstProgress,
        args.beamRadiusScale,
        args.beamGlowScale,
        geomState_.position,
        geomState_.scale
    };
    material_->FillCB(mapped, matArgs);
}

} // namespace YoRigine
