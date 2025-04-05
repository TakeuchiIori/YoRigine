#include "Mesh.h"
#include "../Core/DX/DirectXCommon.h"

void Mesh::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();
	meshData_.vertices.clear();
	meshData_.indices.clear();

	InitializeResource();
}

void Mesh::InitializeResource()
{
	CreateResources();
	TransferData();
}

void Mesh::CreateResources()
{
	CreateObjVertexResource();
	CreateObjIndexResource();
	CreateObjVertexBufferView();
	CreateObjIndexBufferView();
	VertexMap();
	IndexMap();
}

void Mesh::CreateObjVertexResource()
{
	meshResources_.vertexResource = dxCommon_->CreateBufferResource(sizeof(VertexData) * meshData_.vertices.size());
}

void Mesh::CreateObjIndexResource()
{
	meshResources_.indexResource = dxCommon_->CreateBufferResource(sizeof(uint32_t) * meshData_.indices.size());
}

void Mesh::CreateObjVertexBufferView()
{
	meshResources_.vertexBufferView.BufferLocation = meshResources_.vertexResource->GetGPUVirtualAddress();
	meshResources_.vertexBufferView.SizeInBytes = UINT(sizeof(VertexData) * meshData_.vertices.size());
	meshResources_.vertexBufferView.StrideInBytes = sizeof(VertexData);
}

void Mesh::CreateObjIndexBufferView()
{
	meshResources_.indexBufferView.BufferLocation = meshResources_.vertexResource->GetGPUVirtualAddress();
	meshResources_.indexBufferView.SizeInBytes = sizeof(uint32_t) * static_cast<UINT>(meshData_.vertices.size());
	meshResources_.indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}

void Mesh::TransferData()
{
	std::memcpy(vertexData_, meshData_.vertices.data(), sizeof(VertexData) * meshData_.vertices.size());
	meshResources_.vertexResource->Unmap(0, nullptr);
	
	std::memcpy(indexData_, meshData_.indices.data(), sizeof(uint32_t) * meshData_.indices.size());
	meshResources_.indexResource->Unmap(0, nullptr);
}

void Mesh::VertexMap()
{
	meshResources_.vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
}

void Mesh::IndexMap()
{
	meshResources_.indexResource->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
}
