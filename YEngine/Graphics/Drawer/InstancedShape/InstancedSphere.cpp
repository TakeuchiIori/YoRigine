#include "InstancedSphere.h"

#include "DirectXCommon.h"
#include "DirectX/SrvManager.h"
#include "Systems/Camera/Camera.h"
#include "PipelineManager/YPipelineManager.h"
#include "MathFunc.h"

#include <cmath>
#include <numbers>

void InstancedSphere::Initialize()
{
	dxCommon_   = YoRigine::DirectXCommon::GetInstance();
	srvManager_ = SrvManager::GetInstance();

	BuildUnitSphereVertices();
	CreateInstanceBuffer();
	CreateVPBuffer();

	instanceCount_ = 0;
}

void InstancedSphere::BuildUnitSphereVertices()
{
	vbResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kVerticesCount);
	vbView_.BufferLocation = vbResource_->GetGPUVirtualAddress();
	vbView_.SizeInBytes    = UINT(sizeof(VertexData) * kVerticesCount);
	vbView_.StrideInBytes  = sizeof(VertexData);

	VertexData* mapped = nullptr;
	vbResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));

	const float twoPi = 2.0f * std::numbers::pi_v<float>;

	uint32_t idx = 0;
	for (uint32_t i = 0; i < kResolution; ++i) {
		float t1 = static_cast<float>(i)       / kResolution * twoPi;
		float t2 = static_cast<float>(i + 1)   / kResolution * twoPi;
		float c1 = std::cos(t1), s1 = std::sin(t1);
		float c2 = std::cos(t2), s2 = std::sin(t2);

		// XY 平面 (z=0)
		mapped[idx++].position = { c1, s1, 0.0f, 1.0f };
		mapped[idx++].position = { c2, s2, 0.0f, 1.0f };
		// XZ 平面 (y=0)
		mapped[idx++].position = { c1, 0.0f, s1, 1.0f };
		mapped[idx++].position = { c2, 0.0f, s2, 1.0f };
		// YZ 平面 (x=0)
		mapped[idx++].position = { 0.0f, c1, s1, 1.0f };
		mapped[idx++].position = { 0.0f, c2, s2, 1.0f };
	}
	vbResource_->Unmap(0, nullptr);
}

void InstancedSphere::CreateInstanceBuffer()
{
	const size_t bufSize = sizeof(InstanceData) * kMaxInstances;
	instanceResource_ = dxCommon_->CreateBufferResource(bufSize);
	instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	srvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		srvIndex_, instanceResource_.Get(), kMaxInstances, sizeof(InstanceData));
}

void InstancedSphere::CreateVPBuffer()
{
	vpResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
	vpResource_->Map(0, nullptr, reinterpret_cast<void**>(&vpData_));
	*vpData_ = MakeIdentity4x4();
}

void InstancedSphere::Begin()
{
	instanceCount_ = 0;
}

void InstancedSphere::AddSphereMat(const Matrix4x4& worldMat, const Vector4& color)
{
	if (instanceCount_ >= kMaxInstances) return;
	instanceData_[instanceCount_].worldMat = worldMat;
	instanceData_[instanceCount_].color    = color;
	++instanceCount_;
}

void InstancedSphere::AddSphere(const Vector3& center, float radius, const Vector4& color)
{
	if (instanceCount_ >= kMaxInstances) return;

	// scale(radius) * translate(center)
	Matrix4x4 m{};
	m.m[0][0] = radius;
	m.m[1][1] = radius;
	m.m[2][2] = radius;
	m.m[3][0] = center.x;
	m.m[3][1] = center.y;
	m.m[3][2] = center.z;
	m.m[3][3] = 1.0f;

	AddSphereMat(m, color);
}

void InstancedSphere::Flush()
{
	if (instanceCount_ == 0) return;

	*vpData_ = camera_ ? camera_->GetViewProjectionMatrix() : MakeIdentity4x4();

	auto pm = YPipelineManager::GetInstance();
	// InstancedCube パイプラインと同形シェーダー (VS入力位置のみ、SBはInstanceData)
	const auto& indices = pm->GetParameterIndices("InstancedCube");

	auto commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(pm->GetRootSignature("InstancedCube"));
	commandList->SetPipelineState(pm->GetPipeLineStateObject("InstancedCube"));
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &vbView_);

	commandList->SetGraphicsRootConstantBufferView(
		indices.at("gTransformationMatrix"),
		vpResource_->GetGPUVirtualAddress());

	srvManager_->SetGraphicsRootDescriptorTable(
		indices.at("gInstances"), srvIndex_);

	commandList->DrawInstanced(kVerticesCount, instanceCount_, 0, 0);

	instanceCount_ = 0;
}
