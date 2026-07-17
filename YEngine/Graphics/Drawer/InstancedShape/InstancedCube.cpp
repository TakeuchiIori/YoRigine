#include "InstancedCube.h"

#include "DirectXCommon.h"
#include "DirectX/SrvManager.h"
#include "Systems/Camera/Camera.h"
#include "PipelineManager/YPipelineManager.h"
#include "MathFunc.h"

#include <cassert>

// 単位立方体ワイヤーフレーム頂点 (-1..+1)
// 12 edges, each 2 vertices = 24 vertices for LineList topology
static const Vector3 kCubeCorners[8] = {
	{-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
	{-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
};
static const uint32_t kCubeEdges[12][2] = {
	{0,1}, {1,2}, {2,3}, {3,0},
	{4,5}, {5,6}, {6,7}, {7,4},
	{0,4}, {1,5}, {2,6}, {3,7},
};

void InstancedCube::Initialize()
{
	dxCommon_   = YoRigine::DirectXCommon::GetInstance();
	srvManager_ = YoRigine::SrvManager::GetInstance();

	CreateVertexBuffer();
	CreateInstanceBuffer();
	CreateVPBuffer();

	instanceCount_ = 0;
}

void InstancedCube::CreateVertexBuffer()
{
	vbResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kCubeVertices);
	vbView_.BufferLocation = vbResource_->GetGPUVirtualAddress();
	vbView_.SizeInBytes    = UINT(sizeof(VertexData) * kCubeVertices);
	vbView_.StrideInBytes  = sizeof(VertexData);

	VertexData* mapped = nullptr;
	vbResource_->Map(0, nullptr, reinterpret_cast<void**>(&mapped));

	uint32_t idx = 0;
	for (auto& edge : kCubeEdges) {
		const Vector3& a = kCubeCorners[edge[0]];
		const Vector3& b = kCubeCorners[edge[1]];
		mapped[idx++].position = { a.x, a.y, a.z, 1.0f };
		mapped[idx++].position = { b.x, b.y, b.z, 1.0f };
	}
	vbResource_->Unmap(0, nullptr);
}

void InstancedCube::CreateInstanceBuffer()
{
	const size_t bufSize = sizeof(InstanceData) * kMaxInstances;
	instanceResource_ = dxCommon_->CreateBufferResource(bufSize);
	instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));

	srvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforStructuredBuffer(
		srvIndex_, instanceResource_.Get(), kMaxInstances, sizeof(InstanceData));
}

void InstancedCube::CreateVPBuffer()
{
	vpResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
	vpResource_->Map(0, nullptr, reinterpret_cast<void**>(&vpData_));
	*vpData_ = MakeIdentity4x4();
}

void InstancedCube::Begin()
{
	instanceCount_ = 0;
}

void InstancedCube::AddCube(const Matrix4x4& worldMat, const Vector4& color)
{
	if (instanceCount_ >= kMaxInstances) return;
	instanceData_[instanceCount_].worldMat = worldMat;
	instanceData_[instanceCount_].color    = color;
	++instanceCount_;
}

void InstancedCube::AddAABB(const Vector3& mn, const Vector3& mx, const Vector4& color)
{
	if (instanceCount_ >= kMaxInstances) return;

	Vector3 center   = (mn + mx) * 0.5f;
	Vector3 halfSize = (mx - mn) * 0.5f;

	// scale (halfSize) * translate(center) を作る
	Matrix4x4 m{};
	m.m[0][0] = halfSize.x;
	m.m[1][1] = halfSize.y;
	m.m[2][2] = halfSize.z;
	m.m[3][0] = center.x;
	m.m[3][1] = center.y;
	m.m[3][2] = center.z;
	m.m[3][3] = 1.0f;

	AddCube(m, color);
}

void InstancedCube::AddOBB(const Vector3& center, const Vector3& eulerRot, const Vector3& size, const Vector4& color)
{
	if (instanceCount_ >= kMaxInstances) return;

	Matrix4x4 rot   = MakeRotateMatrixXYZ(eulerRot);
	Matrix4x4 sc    = MakeScaleMatrix(size);   // size を半サイズとして扱う (単位立方体は ±1)
	Matrix4x4 trans = MakeTranslateMatrix(center);

	Matrix4x4 world = Multiply(sc, Multiply(rot, trans));
	AddCube(world, color);
}

void InstancedCube::Flush()
{
	if (instanceCount_ == 0) return;

	// VP 更新
	*vpData_ = camera_ ? camera_->GetViewProjectionMatrix() : MakeIdentity4x4();

	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("InstancedCube");

	auto commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(pm->GetRootSignature("InstancedCube"));
	commandList->SetPipelineState(pm->GetPipeLineStateObject("InstancedCube"));
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &vbView_);

	// CBV (VP)
	commandList->SetGraphicsRootConstantBufferView(
		indices.at("gTransformationMatrix"),
		vpResource_->GetGPUVirtualAddress());

	// SRV (Instances)
	srvManager_->SetGraphicsRootDescriptorTable(
		indices.at("gInstances"), srvIndex_);

	// 1 DrawInstanced で全部描画
	commandList->DrawInstanced(kCubeVertices, instanceCount_, 0, 0);

	instanceCount_ = 0;
}
