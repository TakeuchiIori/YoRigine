#include "MeshPrimitive.h"
#include <algorithm>
#include <numbers>


std::shared_ptr<Mesh> MeshPrimitive::CreatePlane(float w, float h)
{
	auto mesh = std::make_shared<Mesh>();

	float halfW = w;
	float halfH = h;

	std::vector<Mesh::VertexData> vertices = {
		{ {  halfW,  halfH, 0.0f, 1.0f },{ 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }, // 0: 右上
		{ { -halfW,  halfH, 0.0f, 1.0f },{ 0.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }, // 1: 左上
		{ { -halfW, -halfH, 0.0f, 1.0f },{ 0.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } }, // 2: 左下
		{ {  halfW, -halfH, 0.0f, 1.0f },{ 1.0f, 1.0f },{ 0.0f, 0.0f, 1.0f } }, // 3: 右下
	};

	std::vector<uint32_t> indices = {
		0, 1, 2,
		0, 2, 3,
	};

	mesh->Initialize(vertices, indices);
	mesh->TransferData();
	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateBox(float w, float h, float d) {
	auto mesh = std::make_shared<Mesh>();

	float hw = w * 0.5f;
	float hh = h * 0.5f;
	float hd = d * 0.5f;

	using V = Mesh::VertexData;
	std::vector<V> vertices = {
		// 前面
		{{-hw, -hh, -hd, 1}, {0, 1}, {0, 0, -1}},
		{{ hw, -hh, -hd, 1}, {1, 1}, {0, 0, -1}},
		{{ hw,  hh, -hd, 1}, {1, 0}, {0, 0, -1}},
		{{-hw,  hh, -hd, 1}, {0, 0}, {0, 0, -1}},

		// 背面
		{{-hw, -hh,  hd, 1}, {1, 1}, {0, 0, 1}},
		{{-hw,  hh,  hd, 1}, {1, 0}, {0, 0, 1}},
		{{ hw,  hh,  hd, 1}, {0, 0}, {0, 0, 1}},
		{{ hw, -hh,  hd, 1}, {0, 1}, {0, 0, 1}},

		// 左
		{{-hw, -hh,  hd, 1}, {0, 1}, {-1, 0, 0}},
		{{-hw, -hh, -hd, 1}, {1, 1}, {-1, 0, 0}},
		{{-hw,  hh, -hd, 1}, {1, 0}, {-1, 0, 0}},
		{{-hw,  hh,  hd, 1}, {0, 0}, {-1, 0, 0}},

		// 右
		{{ hw, -hh, -hd, 1}, {0, 1}, {1, 0, 0}},
		{{ hw, -hh,  hd, 1}, {1, 1}, {1, 0, 0}},
		{{ hw,  hh,  hd, 1}, {1, 0}, {1, 0, 0}},
		{{ hw,  hh, -hd, 1}, {0, 0}, {1, 0, 0}},

		// 上
		{{-hw,  hh, -hd, 1}, {0, 1}, {0, 1, 0}},
		{{ hw,  hh, -hd, 1}, {1, 1}, {0, 1, 0}},
		{{ hw,  hh,  hd, 1}, {1, 0}, {0, 1, 0}},
		{{-hw,  hh,  hd, 1}, {0, 0}, {0, 1, 0}},

		// 下
		{{-hw, -hh,  hd, 1}, {0, 1}, {0, -1, 0}},
		{{ hw, -hh,  hd, 1}, {1, 1}, {0, -1, 0}},
		{{ hw, -hh, -hd, 1}, {1, 0}, {0, -1, 0}},
		{{-hw, -hh, -hd, 1}, {0, 0}, {0, -1, 0}},
	};

	std::vector<uint32_t> indices = {
		// 前
		0, 1, 2, 0, 2, 3,
		// 後
		4, 5, 6, 4, 6, 7,
		// 左
		8, 9,10, 8,10,11,
		// 右
	   12,13,14,12,14,15,
	   // 上
	  16,17,18,16,18,19,
	  // 下
	 20,21,22,20,22,23,
	};

	mesh->Initialize(vertices, indices);
	mesh->TransferData();
	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateRing(float outerRadius, float innerRadius, uint32_t divide) {
	auto mesh = std::make_unique<Mesh>();

	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(divide);

	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	for (uint32_t i = 0; i < divide; ++i) {
		float theta = i * radianPerDivide;
		float thetaNext = (i + 1) * radianPerDivide;

		float sin = std::sin(theta);
		float cos = std::cos(theta);
		float sinNext = std::sin(thetaNext);
		float cosNext = std::cos(thetaNext);

		float u = float(i) / float(divide);
		float uNext = float(i + 1) / float(divide);

		// 頂点4点を定義
		V v1 = { { cos * outerRadius, sin * outerRadius, 0.0f, 1.0f }, { u, 0.0f }, { 0, 0, 1 } };
		V v2 = { { cosNext * outerRadius, sinNext * outerRadius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0, 0, 1 } };
		V v3 = { { cos * innerRadius, sin * innerRadius, 0.0f, 1.0f }, { u, 1.0f }, { 0, 0, 1 } };
		V v4 = { { cosNext * innerRadius, sinNext * innerRadius, 0.0f, 1.0f }, { uNext, 1.0f }, { 0, 0, 1 } };

		uint32_t start = static_cast<uint32_t>(vertices.size());

		// 頂点追加
		vertices.push_back(v1); // 0
		vertices.push_back(v2); // 1
		vertices.push_back(v3); // 2
		vertices.push_back(v4); // 3

		// 三角形① v1, v2, v3
		indices.push_back(start + 0);
		indices.push_back(start + 1);
		indices.push_back(start + 2);

		// 三角形② v2, v4, v3
		indices.push_back(start + 1);
		indices.push_back(start + 3);
		indices.push_back(start + 2);
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();

	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateCylinder(float outerRadius, float innerRadius, uint32_t divide, float height)
{
	auto mesh = std::make_unique<Mesh>();
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(divide);
	const float halfHeight = height * 0.5f;

	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	// --- リングの側面（外周・内周）を追加 ---
	for (uint32_t i = 0; i < divide; ++i) {
		float sin = std::sin(i * radianPerDivide);
		float cos = std::cos(i * radianPerDivide);
		float sinNext = std::sin((i + 1) * radianPerDivide);
		float cosNext = std::cos((i + 1) * radianPerDivide);
		float u = float(i) / float(divide);
		float uNext = float(i + 1) / float(divide);

		// 外側の上下
		V v1 = { {-sin * outerRadius, cos * outerRadius, -halfHeight, 1.0f}, {u, 0.0f} }; // 下
		V v2 = { {-sin * outerRadius, cos * outerRadius, +halfHeight, 1.0f}, {u, 1.0f} }; // 上
		V v3 = { {-sinNext * outerRadius, cosNext * outerRadius, -halfHeight, 1.0f}, {uNext, 0.0f} };
		V v4 = { {-sinNext * outerRadius, cosNext * outerRadius, +halfHeight, 1.0f}, {uNext, 1.0f} };

		// 内側の上下（※法線反転するならこっちの面の三角形順序も反転）
		V v5 = { {-sin * innerRadius, cos * innerRadius, -halfHeight, 1.0f}, {u, 0.0f} };
		V v6 = { {-sin * innerRadius, cos * innerRadius, +halfHeight, 1.0f}, {u, 1.0f} };
		V v7 = { {-sinNext * innerRadius, cosNext * innerRadius, -halfHeight, 1.0f}, {uNext, 0.0f} };
		V v8 = { {-sinNext * innerRadius, cosNext * innerRadius, +halfHeight, 1.0f}, {uNext, 1.0f} };

		uint32_t start = static_cast<uint32_t>(vertices.size());

		// --- 頂点登録 ---
		vertices.push_back(v1); // 0
		vertices.push_back(v2); // 1
		vertices.push_back(v3); // 2
		vertices.push_back(v4); // 3
		vertices.push_back(v5); // 4
		vertices.push_back(v6); // 5
		vertices.push_back(v7); // 6
		vertices.push_back(v8); // 7

		// --- 外側の側面 ---
		indices.push_back(start + 0);
		indices.push_back(start + 1);
		indices.push_back(start + 2);
		indices.push_back(start + 2);
		indices.push_back(start + 1);
		indices.push_back(start + 3);

		// --- 内側の側面（※順序逆にして裏面になるように）---
		indices.push_back(start + 6);
		indices.push_back(start + 5);
		indices.push_back(start + 4);
		indices.push_back(start + 6);
		indices.push_back(start + 7);
		indices.push_back(start + 5);
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();

	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateSphere(float radius, uint32_t subdivisions)
{
	auto mesh = std::make_shared<Mesh>();
	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	// 緯度(lat)と経度(lon)方向の分割
	const uint32_t latCount = subdivisions;
	const uint32_t lonCount = subdivisions;

	for (uint32_t lat = 0; lat <= latCount; ++lat) {
		float v = float(lat) / float(latCount);
		float theta = v * std::numbers::pi_v<float>; // 0 to PI
		float sinTheta = std::sin(theta);
		float cosTheta = std::cos(theta);

		for (uint32_t lon = 0; lon <= lonCount; ++lon) {
			float u = float(lon) / float(lonCount);
			float phi = u * 2.0f * std::numbers::pi_v<float>; // 0 to 2PI
			float sinPhi = std::sin(phi);
			float cosPhi = std::cos(phi);

			// 球面座標系からの変換
			float x = cosPhi * sinTheta;
			float y = cosTheta;
			float z = sinPhi * sinTheta;

			// 頂点データ (法線は位置ベクトルと同じ方向)
			V vertex;
			vertex.position = { x * radius, y * radius, z * radius, 1.0f };
			vertex.texcoord = { 1.0f - u, v }; // テクスチャが裏返らないようUを反転調整
			vertex.normal = { x, y, z };

			vertices.push_back(vertex);
		}
	}

	// インデックス生成
	for (uint32_t lat = 0; lat < latCount; ++lat) {
		for (uint32_t lon = 0; lon < lonCount; ++lon) {
			uint32_t first = (lat * (lonCount + 1)) + lon;
			uint32_t second = first + lonCount + 1;

			// 三角形1
			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);

			// 三角形2
			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();
	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateCone(float radius, float height, uint32_t divide)
{
	auto mesh = std::make_shared<Mesh>();
	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	const float halfH = height * 0.5f;
	const float radianPerDivide = 2.0f * std::numbers::pi_v<float> / float(divide);

	// 側面（円周部分）の生成
	for (uint32_t i = 0; i <= divide; ++i) { // テクスチャの継ぎ目のため <= divide
		float theta = i * radianPerDivide;
		float sin = std::sin(theta);
		float cos = std::cos(theta);
		float u = float(i) / float(divide);

		// 法線の計算（傾きを考慮）
		// 斜面のベクトル(cos, height, sin) に垂直なベクトル
		float slope = std::hypot(radius, height);
		float ny = radius / slope;
		float nxz = height / slope;

		// 頂点（Apex: 先端）
		// ※先端は1点だが、UVマッピングと法線を正しくするために分割数分用意する手法をとる
		V topV;
		topV.position = { 0.0f, halfH, 0.0f, 1.0f };
		topV.texcoord = { u, 0.0f };
		topV.normal = { sin * nxz, ny, cos * nxz }; // 近似的な法線
		vertices.push_back(topV);

		// 底面の円周上の点
		V bottomV;
		bottomV.position = { sin * radius, -halfH, cos * radius, 1.0f };
		bottomV.texcoord = { u, 1.0f };
		bottomV.normal = { sin * nxz, ny, cos * nxz };
		vertices.push_back(bottomV);
	}

	// 側面のインデックス
	for (uint32_t i = 0; i < divide; ++i) {
		uint32_t topCurrent = i * 2;
		uint32_t bottomCurrent = topCurrent + 1;
		uint32_t topNext = (i + 1) * 2;
		uint32_t bottomNext = topNext + 1;

		indices.push_back(topCurrent);
		indices.push_back(bottomCurrent);
		indices.push_back(bottomNext);

		// Coneなので四角形ではなく三角形で閉じるが、縮退三角形として処理するか
		// 上記のように3頂点だけで側面を作る
	}

	// 底面（円盤）の生成
	uint32_t bottomCenterIndex = static_cast<uint32_t>(vertices.size());
	vertices.push_back({ {0, -halfH, 0, 1}, {0.5f, 0.5f}, {0, -1, 0} }); // 底面中心

	uint32_t bottomStartIndex = static_cast<uint32_t>(vertices.size());
	for (uint32_t i = 0; i <= divide; ++i) {
		float theta = -static_cast<float>(i) * radianPerDivide; // 下面なので逆回しにするか、インデックス順序を変える
		float sin = std::sin(theta);
		float cos = std::cos(theta);

		V v;
		v.position = { sin * radius, -halfH, cos * radius, 1.0f };
		v.texcoord = { (sin + 1.0f) * 0.5f, (cos + 1.0f) * 0.5f };
		v.normal = { 0, -1, 0 };
		vertices.push_back(v);
	}

	// 底面のインデックス
	for (uint32_t i = 0; i < divide; ++i) {
		indices.push_back(bottomCenterIndex);
		indices.push_back(bottomStartIndex + i + 1);
		indices.push_back(bottomStartIndex + i);
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();
	return mesh;
}

std::shared_ptr<Mesh> MeshPrimitive::CreateFanShape(float radius, float angleDegree, uint32_t divide)
{
	auto mesh = std::make_shared<Mesh>();

	// 角度をラジアンに変換し、1分割あたりの角度を計算
	const float totalRadian = angleDegree * (std::numbers::pi_v<float> / 180.0f);
	const float radianPerDivide = totalRadian / float(divide);
	// 正面を基準に左右対称にするための開始角
	const float startAngle = -totalRadian * 0.5f;

	using V = Mesh::VertexData;
	std::vector<V> vertices;
	std::vector<uint32_t> indices;

	for (uint32_t i = 0; i < divide; ++i) {
		float theta = startAngle + (i * radianPerDivide);
		float thetaNext = startAngle + ((i + 1) * radianPerDivide);

		float s = std::sin(theta);
		float c = std::cos(theta);
		float sNext = std::sin(thetaNext);
		float cNext = std::cos(thetaNext);

		float u = float(i) / float(divide);
		float uNext = float(i + 1) / float(divide);

		// v1, v2: 外周の点
		// v3: 中心点
		V v1 = { { c * radius, s * radius, 0.0f, 1.0f }, { u, 0.0f }, { 0, 0, 1 } };
		V v2 = { { cNext * radius, sNext * radius, 0.0f, 1.0f }, { uNext, 0.0f }, { 0, 0, 1 } };
		V v3 = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.5f, 1.0f }, { 0, 0, 1 } };

		uint32_t start = static_cast<uint32_t>(vertices.size());

		// 頂点追加
		vertices.push_back(v1); // 0
		vertices.push_back(v2); // 1
		vertices.push_back(v3); // 2

		// 三角形定義 (v1 -> v2 -> v3)
		indices.push_back(start + 0);
		indices.push_back(start + 1);
		indices.push_back(start + 2);
	}

	mesh->Initialize(vertices, indices);
	mesh->TransferData();

	return mesh;
}