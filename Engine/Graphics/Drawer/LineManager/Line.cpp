#include "Line.h"
#include "DX./DirectXCommon.h"
#include "LineManager.h"
#include "Systems./Camera/Camera.h"
void Line::Initialize()
{
	index = 0u;

	lineManager_ = LineManager::GetInstance();
	dxCommon_ = DirectXCommon::GetInstance();

	CrateMaterialResource();

	CrateVetexResource();

	CreateTransformResource();

}

void Line::DrawLine()
{
	// 描画する頂点数が0なら早期リターン
	if (index == 0) {
		return;
	}

	// WVP行列を更新
	if (camera_) {
		transformationMatrix_->WVP = camera_->GetViewProjectionMatrix();
	}
	else {
		transformationMatrix_->WVP = MakeIdentity4x4();
	}

	// GPUに頂点バッファを設定して描画
	auto commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(lineManager_->GetRootSignature().Get());
	commandList->SetPipelineState(lineManager_->GetGraphicsPiplineState().Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationResource_->GetGPUVirtualAddress());
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->DrawInstanced(index, index / 2, 0, 0); // ラインは2つの頂点で構成
	
	index = 0u;
}

void Line::UpdateVertices(const Vector3& start, const Vector3& end)
{
	assert(index < kMaxNum);

	vertexData_[index++].position = { start.x, start.y, start.z, 1.0f }; // 始点
	vertexData_[index++].position = { end.x, end.y, end.z, 1.0f };       // 終点
}

void Line::CrateVetexResource()
{
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kMaxNum);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * kMaxNum);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
}

void Line::CrateMaterialResource()
{

	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Vector4));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

void Line::CreateTransformResource()
{
	// WVP行列リソースを作成
	
	transformationResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
	transformationResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrix_));
	transformationMatrix_->WVP = MakeIdentity4x4();
}
