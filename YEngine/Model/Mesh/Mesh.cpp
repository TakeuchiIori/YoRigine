#include "Mesh.h"
#include "DirectXCommon.h"
#include "../Skeleton/SkinCluster.h"

void Mesh::Initialize() {
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	meshData_.vertices.clear();
	meshData_.indices.clear();
}

void Mesh::Initialize(const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices)
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();
	meshData_.vertices = vertices;
	meshData_.indices = indices;
	InitResources();
}

void Mesh::RecordDrawCommands(ID3D12GraphicsCommandList* command)
{
	command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command->IASetVertexBuffers(0, 1, &meshResources_.vertexBufferView);
	command->IASetIndexBuffer(&meshResources_.indexBufferView);
}

void Mesh::RecordDrawCommands(ID3D12GraphicsCommandList* command, D3D12_VERTEX_BUFFER_VIEW vbv)
{

	D3D12_VERTEX_BUFFER_VIEW vbvs[2] = {
		meshResources_.vertexBufferView,
		vbv
	};
	command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command->IASetVertexBuffers(0, 2, vbvs);
	command->IASetIndexBuffer(&meshResources_.indexBufferView);

}

void Mesh::RecordDrawCommands(ID3D12GraphicsCommandList* command, SkinCluster& skinCluster)
{
	D3D12_VERTEX_BUFFER_VIEW vbv = skinCluster.GetOutputBufferView();

	command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	command->IASetVertexBuffers(0, 1, &vbv);
	command->IASetIndexBuffer(&meshResources_.indexBufferView);
}

void Mesh::InitResources() {
	size_t vertexSize = sizeof(VertexData) * meshData_.vertices.size();
	size_t indexSize = sizeof(uint32_t) * meshData_.indices.size();

	// バッファ作成
	meshResources_.vertexResource = dxCommon_->CreateBufferResource(vertexSize);
	meshResources_.indexResource = dxCommon_->CreateBufferResource(indexSize);

	// VBV設定
	meshResources_.vertexBufferView.BufferLocation = meshResources_.vertexResource->GetGPUVirtualAddress();
	meshResources_.vertexBufferView.SizeInBytes = static_cast<UINT>(vertexSize);
	meshResources_.vertexBufferView.StrideInBytes = sizeof(VertexData);

	// IBV設定
	meshResources_.indexBufferView.BufferLocation = meshResources_.indexResource->GetGPUVirtualAddress();
	meshResources_.indexBufferView.SizeInBytes = static_cast<UINT>(indexSize);
	meshResources_.indexBufferView.Format = DXGI_FORMAT_R32_UINT;

	//TransferData();
}

void Mesh::TransferData()
{
	InitResources();

	size_t vertexSize = sizeof(VertexData) * meshData_.vertices.size();
	size_t indexSize = sizeof(uint32_t) * meshData_.indices.size();

	// 頂点データの転送
	meshResources_.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	std::memcpy(vertexData_, meshData_.vertices.data(), vertexSize);
	meshResources_.vertexResource->Unmap(0, nullptr);

	// インデックスデータの転送
	meshResources_.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
	std::memcpy(indexData_, meshData_.indices.data(), indexSize);
	meshResources_.indexResource->Unmap(0, nullptr);
}




































































//void Mesh::Initialize()
//{
//	dxCommon_ = DirectXCommon::GetInstance();
//	meshData_.vertices.clear();
//	meshData_.indices.clear();
//
//	InitializeResource();
//}
//
//void Mesh::InitializeResource() {
//	// リソース生成
//	size_t vertexSize = sizeof(VertexData) * meshData_.vertices.size();
//	size_t indexSize = sizeof(uint32_t) * meshData_.indices.size();
//
//	meshResources_.vertexResource = dxCommon_->CreateBufferResource(vertexSize);
//	meshResources_.indexResource = dxCommon_->CreateBufferResource(indexSize);
//
//	// Mapしてデータ転送
//	meshResources_.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
//	std::memcpy(vertexData_, meshData_.vertices.data(), vertexSize);
//	meshResources_.vertexResource->Unmap(0, nullptr);
//
//	meshResources_.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
//	std::memcpy(indexData_, meshData_.indices.data(), indexSize);
//	meshResources_.indexResource->Unmap(0, nullptr);
//
//	// VBV設定
//	meshResources_.vertexBufferView.BufferLocation = meshResources_.vertexResource->GetGPUVirtualAddress();
//	meshResources_.vertexBufferView.SizeInBytes = static_cast<UINT>(vertexSize);
//	meshResources_.vertexBufferView.StrideInBytes = sizeof(VertexData);
//
//	// IBV設定
//	meshResources_.indexBufferView.BufferLocation = meshResources_.indexResource->GetGPUVirtualAddress();
//	meshResources_.indexBufferView.SizeInBytes = static_cast<UINT>(indexSize);
//	meshResources_.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
//}
//
//
//void Mesh::CreateResources()
//{
//	CreateObjVertexResource();
//	CreateObjIndexResource();
//	CreateObjVertexBufferView();
//	CreateObjIndexBufferView();
//	VertexMap();
//	IndexMap();
//}
//
//void Mesh::CreateObjVertexResource()
//{
//	meshResources_.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * meshData_.vertices.size());
//}
//
//void Mesh::CreateObjIndexResource()
//{
//	meshResources_.indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * meshData_.indices.size());
//}
//
//void Mesh::CreateObjVertexBufferView()
//{
//	meshResources_.vertexBufferView.BufferLocation = meshResources_.vertexResource->GetGPUVirtualAddress();
//	meshResources_.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * meshData_.vertices.size());
//	meshResources_.vertexBufferView.StrideInBytes = sizeof(VertexData);
//}
//
//void Mesh::CreateObjIndexBufferView()
//{
//	meshResources_.indexBufferView.BufferLocation = meshResources_.vertexResource->GetGPUVirtualAddress();
//	meshResources_.indexBufferView.SizeInBytes = sizeof(uint32_t) * static_cast<UINT>(meshData_.vertices.size());
//	meshResources_.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
//}
//
//void Mesh::TransferData()
//{
//	std::memcpy(vertexData_, meshData_.vertices.data(), sizeof(VertexData) * meshData_.vertices.size());
//	meshResources_.vertexResource->Unmap(0, nullptr);
//	
//	std::memcpy(indexData_, meshData_.indices.data(), sizeof(uint32_t) * meshData_.indices.size());
//	meshResources_.indexResource->Unmap(0, nullptr);
//}
//
//void Mesh::VertexMap()
//{
//	meshResources_.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
//}
//
//void Mesh::IndexMap()
//{
//	meshResources_.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
//}

