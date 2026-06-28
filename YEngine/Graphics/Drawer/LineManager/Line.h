#pragma once

// C++
#include <d3d12.h>
#include <wrl.h>
#include <array>

// Math
#include "Vector3.h"
#include "Matrix4x4.h"
#include "MathFunc.h"


class Camera;
class LineManager;

namespace YoRigine {
	class DirectXCommon;
}

/// <summary>
/// ライン描画クラス
/// </summary>
class Line
{

public:
	///************************* 基本関数 *************************///

	void Initialize();
	void DrawLine();

	// フレーム冒頭で呼び、頂点インデックス・カラーリングをリセットする。
	// 呼ばないとフレーム間で蓄積されていき、いずれ kMaxNum を超えた頂点がスキップされる。
	void Reset();

	// 頂点の登録
	void RegisterLine(const Vector3& start, const Vector3& end);

	// 各形状の描画
	void DrawSphere(const Vector3& center, float radius, int resolution);
	void DrawCircleXZ(const Vector3& center, float radius, int resolution);
	void DrawAABB(const Vector3& min, const Vector3& max);
	void DrawOBB(const Vector3& center, const Vector3& rotationEuler, const Vector3& size);
	void DrawCapsule(const Vector3& start, const Vector3& end, float radius, int resolution = 16);
	void DrawCone(const Vector3& position, float rotationY,float viewDistance, float viewAngleDeg, int resolution = 16);
private:
	///************************* 内部処理 *************************///

	// 頂点リソース
	void CrateVetexResource();
	// マテリアルリソース
	void CrateMaterialResource();
	// 座標変換リソース
	void CreateTransformResource();
public:
	///************************* アクセッサ *************************///
	void SetCamera(Camera* camera) { this->camera_ = camera; }
	void SetColor(const Vector4& color) { currentColor_ = color; }
private:
	///************************* GPU用の構造体 *************************///
	// 頂点データ構造体
	struct VertexData {
		Vector4 position;
	};
	// マテリアル構造体
	struct MaterialData {
		Vector4 color;
		float padiing[3];
	};
	// 座標変換構造体
	struct TransformationMatrix {
		Matrix4x4 WVP;
	};

	// マテリアルバッチ (DrawLine 1 回ごとに 1 つ消費される)
	struct MaterialSlot {
		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		MaterialData* mapped = nullptr;
	};
private:
	///************************* メンバ変数 *************************///
	LineManager* lineManager_ = nullptr;
	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// 頂点関連
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW  vertexBufferView_ = {};
	VertexData* vertexData_ = nullptr;

	// マテリアル関連
	// 単一 CB をフレームを跨いで上書きすると、複数の DrawLine() の発行コマンドが
	// すべて「最後の SetColor の色」で描画されてしまう (DX12 はコマンド記録時に
	// CB の中身をコピーしない)。これを避けるためバッチごとに別 CB スロットを使う。
	static constexpr uint32_t kMaterialSlotCount = 64u;
	std::array<MaterialSlot, kMaterialSlotCount> materialSlots_;
	uint32_t currentMaterialIndex_ = 0u;
	Vector4  currentColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 座標関連
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	TransformationMatrix* transformationMatrix_ = nullptr;

	// 定数
	const uint32_t kMaxNum = 4096u * 4u;
	// 次に書き込む頂点インデックス
	uint32_t index = 0u;
	// 直近の DrawLine で描画済みの末尾位置 (= 次の DrawLine の開始 StartVertexLocation)
	uint32_t drawStartIndex_ = 0u;

	VertexData vertices_[2];

};

