#include "Line.h"
#include "DX./DirectXCommon.h"
#include "LineManager.h"
#include "Systems./Camera/Camera.h"

// Math
#include "MathFunc.h"
#include "Matrix4x4.h"
#include  <numbers>

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





void Line::RegisterLine(const Vector3& start, const Vector3& end)
{
	assert(index < kMaxNum);

	vertexData_[index++].position = { start.x, start.y, start.z, 1.0f }; // 始点
	vertexData_[index++].position = { end.x, end.y, end.z, 1.0f };       // 終点
}


/// <summary>
/// 球を描画
/// </summary>
void Line::DrawSphere(const Vector3& center, float radius, int resolution)
{
	for (int i = 0; i < resolution; ++i) {
		float theta1 = float(i) / resolution * 2.0f * std::numbers::pi_v<float>;
		float theta2 = float(i + 1) / resolution * 2.0f * std::numbers::pi_v<float>;

		// XY 平面
		RegisterLine(
			{ center.x + radius * cosf(theta1), center.y + radius * sinf(theta1), center.z },
			{ center.x + radius * cosf(theta2), center.y + radius * sinf(theta2), center.z });

		// XZ 平面
		RegisterLine(
			{ center.x + radius * cosf(theta1), center.y, center.z + radius * sinf(theta1) },
			{ center.x + radius * cosf(theta2), center.y, center.z + radius * sinf(theta2) });

		// YZ 平面
		RegisterLine(
			{ center.x, center.y + radius * cosf(theta1), center.z + radius * sinf(theta1) },
			{ center.x, center.y + radius * cosf(theta2), center.z + radius * sinf(theta2) });
	}
}


/// <summary>
/// AABBを描画
/// </summary>
void Line::DrawAABB(const Vector3& min, const Vector3& max)
{
	Vector3 corners[8] = {
		{min.x, min.y, min.z},
		{max.x, min.y, min.z},
		{max.x, max.y, min.z},
		{min.x, max.y, min.z},
		{min.x, min.y, max.z},
		{max.x, min.y, max.z},
		{max.x, max.y, max.z},
		{min.x, max.y, max.z},
	};

	// エッジを線で結ぶ
	uint32_t edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // 底面
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // 上面
		{0, 4}, {1, 5}, {2, 6}, {3, 7}  // 側面
	};

	for (auto& edge : edges) {
		RegisterLine(corners[edge[0]], corners[edge[1]]);
	}
}

/// <summary>
/// OBBを描画（rotation: Quaternion, size: Vector3）
/// </summary>
void Line::DrawOBB(const Vector3& center, const Vector3& rotationEuler, const Vector3& size) {
	// 回転行列をオイラー角から作成（ラジアン前提）
	Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotationEuler); // ここをVector3→Matrixに

	// 各軸方向 × サイズで orientations を生成（スケーリング込み）
	Vector3 orientations[3] = {
		Transform({1, 0, 0}, rotMat) * size.x, // X軸方向
		Transform({0, 1, 0}, rotMat) * size.y, // Y軸方向
		Transform({0, 0, 1}, rotMat) * size.z  // Z軸方向
	};

	// ローカル空間の頂点オフセット（-1〜1）
	const Vector3 localOffsets[8] = {
		{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
		{-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
	};

	Vector3 corners[8];
	for (int i = 0; i < 8; ++i) {
		const Vector3& o = localOffsets[i];

		// center + (回転した各軸ベクトル × -1 or +1)
		corners[i] = center +
			orientations[0] * o.x +
			orientations[1] * o.y +
			orientations[2] * o.z;
	}

	// 辺の接続情報（12本）
	const uint32_t edges[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0}, // 底面
		{4,5}, {5,6}, {6,7}, {7,4}, // 上面
		{0,4}, {1,5}, {2,6}, {3,7}  // 側面
	};

	// ライン登録
	for (const auto& edge : edges) {
		RegisterLine(corners[edge[0]], corners[edge[1]]);
	}
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
