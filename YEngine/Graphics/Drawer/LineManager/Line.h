#pragma once

// C++
#include <array>
#include <d3d12.h>
#include <wrl.h>

// Math
#include "MathFunc.h"
#include "Matrix4x4.h"
#include "Vector3.h"

namespace YoRigine { class Camera; }
class LineManager;

namespace YoRigine {
class DirectXCommon;

/// <summary>
/// ライン描画クラス
/// </summary>
class Line {

public:
  ///************************* 基本関数 *************************///

  void Initialize();
  void DrawLine();

  // フレーム冒頭で呼び、頂点インデックス・カラーリングをリセットする。
  // 呼ばないとフレーム間で蓄積されていき、いずれ kMaxNum
  // を超えた頂点がスキップされる。
  void Reset();

  // 頂点の登録
  void RegisterLine(const Vector3 &start, const Vector3 &end);

  // 各形状の描画
  void DrawSphere(const Vector3 &center, float radius, int resolution);
  void DrawCircleXZ(const Vector3 &center, float radius, int resolution);
  void DrawAABB(const Vector3 &min, const Vector3 &max);
  void DrawOBB(const Vector3 &center, const Vector3 &rotationEuler,
               const Vector3 &size);
  void DrawCapsule(const Vector3 &start, const Vector3 &end, float radius,
                   int resolution = 16);
  void DrawCone(const Vector3 &position, float rotationY, float viewDistance,
                float viewAngleDeg, int resolution = 16);

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
  void SetCamera(YoRigine::Camera *camera) { this->camera_ = camera; }
  void SetColor(const Vector4 &color) { currentColor_ = color; }

  // 線の太さ（ワールド単位）。0以下で従来どおりの1px相当の細線(LINELIST)。
  // 0より大きいとカメラ視線に直交する板状の四角形(TRIANGLELIST)で太線を描く。
  // 注意: DrawLine() を呼ぶまでは値を変更しないこと（同一バッチ内で太さが混在すると
  // 頂点レイアウトとトポロジが食い違う）。RegisterXxx → DrawLine() → SetLineWidth(次の太さ) の順で使う。
  void SetLineWidth(float width) { lineWidth_ = width; }
  float GetLineWidth() const { return lineWidth_; }

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
    MaterialData *mapped = nullptr;
  };

private:
  ///************************* メンバ変数 *************************///
  LineManager *lineManager_ = nullptr;
  YoRigine::DirectXCommon *dxCommon_ = nullptr;
  YoRigine::Camera *camera_ = nullptr;

  // 頂点関連
  Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
  D3D12_VERTEX_BUFFER_VIEW vertexBufferView_ = {};
  VertexData *vertexData_ = nullptr;

  // マテリアル関連
  // 単一 CB をフレームを跨いで上書きすると、複数の DrawLine() の発行コマンドが
  // すべて「最後の SetColor の色」で描画されてしまう (DX12 はコマンド記録時に
  // CB の中身をコピーしない)。これを避けるためバッチごとに別 CB
  // スロットを使う。
  static constexpr uint32_t kMaterialSlotCount = 64u;
  std::array<MaterialSlot, kMaterialSlotCount> materialSlots_;
  uint32_t currentMaterialIndex_ = 0u;
  Vector4 currentColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
  float lineWidth_ = 0.0f; // 0以下=従来の細線(LINELIST)

  // 座標関連
  Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
  TransformationMatrix *transformationMatrix_ = nullptr;

  // 定数
  const uint32_t kMaxNum = 4096u * 4u;
  // 次に書き込む頂点インデックス
  uint32_t index = 0u;
  // 直近の DrawLine で描画済みの末尾位置 (= 次の DrawLine の開始
  // StartVertexLocation)
  uint32_t drawStartIndex_ = 0u;

  VertexData vertices_[2];
};

} // namespace YoRigine
