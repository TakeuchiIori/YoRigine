#include "Line.h"
#include "DirectXCommon.h"
#include "LineManager.h"
#include "Systems./Camera/Camera.h"
#include "PipelineManager/YPipelineManager.h"

// Math
#include "MathFunc.h"
#include "Matrix4x4.h"
#include  <numbers>

/// <summary>
/// ライン描画用バッファ・リソースの初期化
/// </summary>
void Line::Initialize()
{
	index = 0u;

	lineManager_ = LineManager::GetInstance();
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();

	CrateMaterialResource();
	CrateVetexResource();

	CreateTransformResource();
}

/// <summary>
/// 蓄積されたラインリストをまとめて描画する
/// </summary>
void Line::DrawLine()
{

	// 描画する頂点数が 0 なら早期リターン
	if (index == 0) {
		return;
	}

	// カメラが指定されていれば WVP 更新
	if (camera_) {
		transformationMatrix_->WVP = camera_->GetViewProjectionMatrix();
	} else {
		transformationMatrix_->WVP = MakeIdentity4x4();
	}
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Line");

	// GPU にセットして描画
	auto commandList = dxCommon_->GetCommandList();
	commandList->SetGraphicsRootSignature(lineManager_->GetRootSignature().Get());
	commandList->SetPipelineState(lineManager_->GetGraphicsPiplineState().Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	commandList->SetGraphicsRootConstantBufferView(indices.at("gMaterial"), materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(indices.at("gTransformationMatrix"), transformationResource_->GetGPUVirtualAddress());

	// index / 2 本のライン（2 頂点 = 1 ライン）
	commandList->DrawInstanced(index, 1, 0, 0);

	// 描画後にクリア
	index = 0u;
}

/// <summary>
/// 始点と終点を指定して 1 本のラインを登録する
/// </summary>
void Line::RegisterLine(const Vector3& start, const Vector3& end)
{
	// 頂点バッファが満杯のときは描画を諦めて静かにスキップする。
	// 死亡時など大量のコライダー・視界コーンが同時に登録されるケースで
	// assert で止まらないようにするための防御。デバッグ描画が一部欠けるだけで
	// ゲーム進行には影響しない。
	if (index + 2 > kMaxNum) {
		return;
	}

	vertexData_[index++].position = { start.x, start.y, start.z, 1.0f };
	vertexData_[index++].position = { end.x,   end.y,   end.z,   1.0f };
}

/// <summary>
/// 球のワイヤーフレームを描画用に登録する
/// </summary>
void Line::DrawSphere(const Vector3& center, float radius, int resolution)
{
	for (int i = 0; i < resolution; ++i) {
		float theta1 = float(i) / resolution * 2.0f * std::numbers::pi_v<float>;
		float theta2 = float(i + 1) / resolution * 2.0f * std::numbers::pi_v<float>;

		// XY 平面
		RegisterLine(
			{ center.x + radius * cosf(theta1), center.y + radius * sinf(theta1), center.z },
			{ center.x + radius * cosf(theta2), center.y + radius * sinf(theta2), center.z }
		);

		// XZ 平面
		RegisterLine(
			{ center.x + radius * cosf(theta1), center.y, center.z + radius * sinf(theta1) },
			{ center.x + radius * cosf(theta2), center.y, center.z + radius * sinf(theta2) }
		);

		// YZ 平面
		RegisterLine(
			{ center.x, center.y + radius * cosf(theta1), center.z + radius * sinf(theta1) },
			{ center.x, center.y + radius * cosf(theta2), center.z + radius * sinf(theta2) }
		);
	}
}

void Line::DrawCircleXZ(const Vector3& center, float radius, int resolution)
{
	for (int i = 0; i < resolution; ++i) {
		float theta1 = float(i) / resolution * 2.0f * std::numbers::pi_v<float>;
		float theta2 = float(i + 1) / resolution * 2.0f * std::numbers::pi_v<float>;
		RegisterLine(
			{ center.x + radius * cosf(theta1), center.y, center.z + radius * sinf(theta1) },
			{ center.x + radius * cosf(theta2), center.y, center.z + radius * sinf(theta2) }
		);
	}
}

/// <summary>
/// AABB（軸平行バウンディングボックス）のワイヤーフレームを描画登録
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

	// 12本のエッジを線で描く
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
/// OBB（方向付きバウンディングボックス）のワイヤーフレームを描画登録
/// </summary>
/// <param name="center">中心座標</param>
/// <param name="rotationEuler">回転オイラー角（ラジアン）</param>
/// <param name="size">各軸方向のサイズ</param>
void Line::DrawOBB(const Vector3& center, const Vector3& rotationEuler, const Vector3& size)
{
	// オイラー角 → 回転行列
	Matrix4x4 rotMat = MakeRotateMatrixXYZ(rotationEuler);

	// 回転軸ベクトル × スケール
	Vector3 orientations[3] = {
		Transform({1, 0, 0}, rotMat) * size.x,
		Transform({0, 1, 0}, rotMat) * size.y,
		Transform({0, 0, 1}, rotMat) * size.z
	};

	// ローカル頂点（-1〜1）
	const Vector3 localOffsets[8] = {
		{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
		{-1, -1,  1}, {1, -1,  1}, {1, 1,  1}, {-1, 1,  1}
	};

	Vector3 corners[8];
	for (int i = 0; i < 8; ++i) {
		const Vector3& o = localOffsets[i];
		corners[i] = center +
			orientations[0] * o.x +
			orientations[1] * o.y +
			orientations[2] * o.z;
	}

	// エッジ（12本）
	const uint32_t edges[12][2] = {
		{0,1}, {1,2}, {2,3}, {3,0},
		{4,5}, {5,6}, {6,7}, {7,4},
		{0,4}, {1,5}, {2,6}, {3,7}
	};

	for (const auto& edge : edges) {
		RegisterLine(corners[edge[0]], corners[edge[1]]);
	}
}

/// <summary>
/// カプセル(両端が半球の円柱)のワイヤーフレームを描画登録
/// </summary>
void Line::DrawCapsule(const Vector3& start, const Vector3& end, float radius, int resolution)
{
	// 軸ベクトル
	Vector3 axis = end - start;
	float axisLen = Length(axis);
	if (axisLen < 1e-6f) {
		// 退化 → 球扱い
		DrawSphere(start, radius, resolution);
		return;
	}

	Vector3 dir = axis * (1.0f / axisLen); // 正規化軸

	// 軸に直交する任意ベクトル u を求める。
	// dir に依存しない安定な選び方: dir と最も平行でない世界軸を選ぶ
	Vector3 helper = (std::fabs(dir.y) < 0.9f) ? Vector3{ 0, 1, 0 } : Vector3{ 1, 0, 0 };
	Vector3 u = Normalize(Cross(helper, dir));
	Vector3 v = Cross(dir, u);

	// 端面の円(2つ) + 円柱側面リブ
	const float twoPi = 2.0f * std::numbers::pi_v<float>;

	for (int i = 0; i < resolution; ++i) {
		float t1 = float(i)     / resolution * twoPi;
		float t2 = float(i + 1) / resolution * twoPi;

		Vector3 r1 = u * (radius * cosf(t1)) + v * (radius * sinf(t1));
		Vector3 r2 = u * (radius * cosf(t2)) + v * (radius * sinf(t2));

		// start側 円 (軸に直交)
		RegisterLine(start + r1, start + r2);
		// end側 円 (軸に直交)
		RegisterLine(end + r1, end + r2);
	}

	// 円柱の側面リブ (4本)
	for (int i = 0; i < 4; ++i) {
		float t = float(i) / 4.0f * twoPi;
		Vector3 r = u * (radius * cosf(t)) + v * (radius * sinf(t));
		RegisterLine(start + r, end + r);
	}

	// 半球(両端) - 軸面ループを2平面分
	for (int i = 0; i < resolution; ++i) {
		float t1 = float(i)     / resolution * std::numbers::pi_v<float>; // 0..π (半球)
		float t2 = float(i + 1) / resolution * std::numbers::pi_v<float>;

		// start側 半球: -dir 方向に膨らむ
		Vector3 sa1 = start + (u * radius) * cosf(t1) + (dir * -radius) * sinf(t1);
		Vector3 sa2 = start + (u * radius) * cosf(t2) + (dir * -radius) * sinf(t2);
		Vector3 sb1 = start + (v * radius) * cosf(t1) + (dir * -radius) * sinf(t1);
		Vector3 sb2 = start + (v * radius) * cosf(t2) + (dir * -radius) * sinf(t2);
		RegisterLine(sa1, sa2);
		RegisterLine(sb1, sb2);

		// end側 半球: +dir 方向に膨らむ
		Vector3 ea1 = end + (u * radius) * cosf(t1) + (dir * radius) * sinf(t1);
		Vector3 ea2 = end + (u * radius) * cosf(t2) + (dir * radius) * sinf(t2);
		Vector3 eb1 = end + (v * radius) * cosf(t1) + (dir * radius) * sinf(t1);
		Vector3 eb2 = end + (v * radius) * cosf(t2) + (dir * radius) * sinf(t2);
		RegisterLine(ea1, ea2);
		RegisterLine(eb1, eb2);
	}
}

void Line::DrawCone(const Vector3& position, float rotationY, float viewDistance, float viewAngleDeg, int resolution) {

	float halfAngle = viewAngleDeg * 0.5f * (std::numbers::pi_v<float> / 180.0f);
	float leftAngle = rotationY - halfAngle;
	float rightAngle = rotationY + halfAngle;

	Vector3 origin = { position.x, position.y + 0.05f, position.z };

	// 端点を変数として確定させる
	Vector3 leftEnd = origin + Vector3{ sinf(leftAngle),  0.0f, cosf(leftAngle) } *viewDistance;
	Vector3 rightEnd = origin + Vector3{ sinf(rightAngle), 0.0f, cosf(rightAngle) } *viewDistance;

	// 境界線
	RegisterLine(origin, leftEnd);
	RegisterLine(origin, rightEnd);

	// 弧 --- leftEnd/rightEnd を直接始点・終点として使う
	for (int i = 0; i < resolution; ++i) {
		float t1 = leftAngle + (rightAngle - leftAngle) * float(i) / resolution;
		float t2 = leftAngle + (rightAngle - leftAngle) * float(i + 1) / resolution;

		// 両端は再計算せず確定済みの端点を使う
		Vector3 p1 = (i == 0) ? leftEnd
			: origin + Vector3{ sinf(t1), 0.0f, cosf(t1) } *viewDistance;
		Vector3 p2 = (i == resolution - 1) ? rightEnd
			: origin + Vector3{ sinf(t2), 0.0f, cosf(t2) } *viewDistance;

		RegisterLine(p1, p2);
	}

	// 原点マーカー
	float cross = 0.3f;
	RegisterLine(origin + Vector3{ -cross, 0, 0 }, origin + Vector3{ cross, 0, 0 });
	RegisterLine(origin + Vector3{ 0, 0, -cross }, origin + Vector3{ 0, 0, cross });
}

/// <summary>
/// 頂点バッファ（ライン用）の生成
/// </summary>
void Line::CrateVetexResource()
{
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * kMaxNum);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = UINT(sizeof(VertexData) * kMaxNum);
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
}

/// <summary>
/// ライン描画用マテリアル（色情報）リソースの生成
/// </summary>
void Line::CrateMaterialResource()
{
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Vector4));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

/// <summary>
/// WVP用の Transform バッファを生成
/// </summary>
void Line::CreateTransformResource()
{
	transformationResource_ = dxCommon_->CreateBufferResource(sizeof(Matrix4x4));
	transformationResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrix_));
	transformationMatrix_->WVP = MakeIdentity4x4();
}
