#include "YParticleRenderer.h"
#include "DirectXCommon.h"
#include "Matrix4x4.h"
#include "PipelineManager/YPipelineManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

void YParticleRenderer::Initialize(SrvManager* srvManager, uint32_t maxTotalParticles) {
    dxCommon_ = YoRigine::DirectXCommon::GetInstance();
    srvManager_ = srvManager;
    maxTotalParticles_ = maxTotalParticles;

    // インスタンシング用 StructuredBuffer
    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(TransformForGPU) * maxTotalParticles_);
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));

    srvIndex_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(srvIndex_, instancingResource_.Get(), maxTotalParticles_, sizeof(TransformForGPU));

    // 既存マテリアルクラスの初期化
    materialColor_ = std::make_unique<MaterialColor>();
    materialColor_->Initialize();
    materialLighting_ = std::make_unique<MaterialLighting>();
    materialLighting_->Initialize();
    materialUV_ = std::make_unique<MaterialUV>();
    materialUV_->Initialize();
}

void YParticleRenderer::BeginFrame() {
    currentInstanceOffset_ = 0;
    batchStartOffset_ = 0;
    batchDrawIndex_ = 0;
    batchEndOffsets_.clear();
}

void YParticleRenderer::ApplyLightSetting(const ParticleLightSetting& setting) {
    currentLightSetting_ = setting;
}

// ビルボード行列を作成（フル）
Matrix4x4 CreateFullBillboard(const Vector3& position, const Vector3& cameraPos, const Vector3& scale) {
    // カメラへの方向ベクトル
    Vector3 forward = Normalize(cameraPos - position);

    // ワールドのUp方向
    Vector3 worldUp = { 0.0f, 1.0f, 0.0f };

    // Right軸を計算
    Vector3 right = Normalize(Cross(worldUp, forward));

    // Up軸を再計算（直交性を保証）
    Vector3 up = Cross(forward, right);

    // スケールを適用
    right = right * scale.x;
    up = up * scale.y;
    forward = forward * scale.z;

    // ビルボード行列を構築
    Matrix4x4 result = {
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        position.x, position.y, position.z, 1.0f
    };

    return result;
}

// Y軸固定ビルボード行列を作成
Matrix4x4 CreateYAxisBillboard(const Vector3& position, const Vector3& cameraPos, const Vector3& scale) {
    // カメラへの方向（Y成分を無視）
    Vector3 toCameraXZ = { cameraPos.x - position.x, 0.0f, cameraPos.z - position.z };
    Vector3 forward = Normalize(toCameraXZ);

    // Y軸は固定
    Vector3 up = { 0.0f, 1.0f, 0.0f };

    // Right軸を計算
    Vector3 right = Normalize(Cross(up, forward));

    // スケール適用
    right = right * scale.x;
    up = up * scale.y;
    forward = forward * scale.z;

    Matrix4x4 result = {
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        forward.x, forward.y, forward.z, 0.0f,
        position.x, position.y, position.z, 1.0f
    };

    return result;
}


void YParticleRenderer::AddSystem(const YParticleSystem& system, Camera* camera) {
    if (!camera) return;
    camera_ = camera;

    const auto& attributes = system.GetAttributes();
    Matrix4x4 viewProj = camera_->GetViewProjectionMatrix();
    Vector3 cameraPos = camera_->GetTranslate();

    Matrix4x4 viewMat = camera_->GetViewMatrix();
    Matrix4x4 billboardRotation = viewMat;
    billboardRotation.m[3][0] = 0.0f;
    billboardRotation.m[3][1] = 0.0f;
    billboardRotation.m[3][2] = 0.0f;
    billboardRotation.m[3][3] = 1.0f;
    Matrix4x4 billboardBase = Inverse(billboardRotation);

    // 相対座標用の親行列
    Matrix4x4 parentMat = system.IsRelative() ? system.GetParentMatrix() : MakeIdentity4x4();
    BillboardType billboardType = system.GetBillboardType();
    uint32_t writtenCount = 0;
    for (const auto& attr : attributes) {
        if (!attr.isActive) continue;

        // --- 最終的な座標の計算 ---
        Vector3 worldPos = attr.position;
        if (system.IsRelative()) {
            Vector4 pos4 = { attr.position.x, attr.position.y, attr.position.z, 1.0f };
            pos4 = Transform(pos4, parentMat);
            worldPos = { pos4.x, pos4.y, pos4.z };
        }

        // --- 各種行列の作成 ---
        Matrix4x4 S = MakeScaleMatrix(attr.scale);
        Matrix4x4 T = MakeTranslateMatrix(worldPos);
        Matrix4x4 localWorld;

        if (billboardType == BillboardType::Full) {
            // 【フルビルボード】
            // Z軸回転（テクスチャの回転）を適用してから、ビルボード行列を掛ける
            Matrix4x4 Rz = MakeRotateMatrixZ(attr.rotation.z);
            localWorld = Multiply(S, Multiply(Rz, Multiply(billboardBase, T)));
        }
        else if (billboardType == BillboardType::YAxisFixed) {
            // 【Y軸固定ビルボード】
            // カメラの方向を向くY軸回転行列を別途計算
            Vector3 toCamera = cameraPos - worldPos;
            float angle = std::atan2(toCamera.x, toCamera.z);
            Matrix4x4 ry = MakeRotateMatrixY(angle);

            Matrix4x4 Rz = MakeRotateMatrixZ(attr.rotation.z);
            localWorld = Multiply(S, Multiply(Rz, Multiply(ry, T)));
        }
        else {
            // 【ビルボードなし】
            Matrix4x4 R = MakeRotateMatrixXYZ(attr.rotation);
            localWorld = Multiply(S, Multiply(R, T));
            // ビルボードなしで相対座標の場合、親の回転も乗るように最後に掛ける
            if (system.IsRelative()) {
            }
        }

        // インスタンシングデータに設定
        mappedData_[currentInstanceOffset_].World = localWorld;
        mappedData_[currentInstanceOffset_].WVP = Multiply(localWorld, viewProj);
        mappedData_[currentInstanceOffset_].color = attr.color;
        currentInstanceOffset_++;
        writtenCount++;

        char buf[256];
        sprintf_s(buf, "AddSystem: %s, written=%u, totalOffset=%u\n",
            system.GetName().c_str(), writtenCount, currentInstanceOffset_);
        OutputDebugStringA(buf);
    }
}
void YParticleRenderer::EndFrame(const std::shared_ptr<Mesh>& mesh, uint32_t textureIndex, BlendMode blendMode) {
    uint32_t start = (batchDrawIndex_ == 0) ? 0 : batchEndOffsets_[batchDrawIndex_ - 1];
    uint32_t end = batchEndOffsets_[batchDrawIndex_];
    uint32_t instanceCount = end - start;

    char buf[256];
    sprintf_s(buf, "EndFrame: drawIndex=%u, start=%u, end=%u, instanceCount=%u, blendMode=%d\n",
        batchDrawIndex_, start, end, instanceCount, (int)blendMode);
    OutputDebugStringA(buf);
    batchDrawIndex_++;

    if (instanceCount == 0 || !mesh) return;

    auto commandList = dxCommon_->GetCommandList();
    auto& meshRes = mesh->GetMeshResource();

    auto pm = YPipelineManager::GetInstance();
    const auto& indices = pm->GetParameterIndices("YParticle");
    commandList->SetPipelineState(pm->GetBlendModePSO("YParticle", blendMode));

    commandList->IASetVertexBuffers(0, 1, &meshRes.vertexBufferView);
    commandList->IASetIndexBuffer(&meshRes.indexBufferView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    srvManager_->SetGraphicsRootDescriptorTable(indices.at("gParticle"), srvIndex_);
    srvManager_->SetGraphicsRootDescriptorTable(indices.at("gTexture"), textureIndex);
    materialColor_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialColor"));
    materialUV_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialUV"));
    if (materialLighting_) {
        materialLighting_->SetEnableLighting(currentLightSetting_.enableDirectionalLight);
    }
    materialLighting_->RecordDrawCommands(commandList.Get(), indices.at("gMaterialLight"));
    commandList->SetGraphicsRootConstantBufferView(indices.at("gCamera"), camera_->GetCameraResource()->GetGPUVirtualAddress());

    commandList->DrawIndexedInstanced(
        static_cast<UINT>(mesh->GetIndexCount()),
        instanceCount,
        0, 0, start
    );
}

// バッチのオフセットだけ記録してDrawはしない
void YParticleRenderer::CommitBatch() {
    batchEndOffsets_.push_back(currentInstanceOffset_);
    batchStartOffset_ = currentInstanceOffset_;

    char buf[256];
    sprintf_s(buf, "CommitBatch: offset=%u\n", currentInstanceOffset_);
    OutputDebugStringA(buf);
}

void YParticleRenderer::ResetBatchOffset() {
    batchStartOffset_ = 0;
    batchDrawIndex_ = 0;
}