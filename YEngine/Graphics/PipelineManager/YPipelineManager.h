#pragma once

#include <d3d12.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <wrl.h>

#include "Loaders/Json/EnumUtils.h"
#include "PSOCache.h"
#include "RootSignatureBuilder.h"
#include "ShaderReflection.h"

namespace YoRigine {
class DirectXCommon;
}

/* <summary>
    改良版パイプライン管理クラス
    使用例:
    auto* manager = YPipelineManager::GetInstance();
    manager->Initialize();

    パイプラインステートを取得
    auto* pso = manager->GetPipeLineStateObject("Sprite");

    パラメータインデックスを取得（名前ベース）
    auto& indices = manager->GetParameterIndices("Sprite");
    UINT materialIdx = indices.at("Material");

    コマンドリストに設定
    commandList->SetGraphicsRootConstantBufferView(materialIdx, address);
    </summary>*/
class YPipelineManager {
public:
  static YPipelineManager *GetInstance();

  void Initialize();
  void Finalize();

  /// <summary>
  /// ルートシグネチャを取得
  /// </summary>
  ID3D12RootSignature *GetRootSignature(const std::string &key);

  /// <summary>
  /// パイプラインステートオブジェクトを取得
  /// </summary>
  ID3D12PipelineState *GetPipeLineStateObject(const std::string &key);

  /// <summary>
  /// ルートパラメータのインデックスマップを取得（手動ビルダー用）
  /// 使用例: auto idx = manager->GetRootParameterIndices("Object")["Material"];
  /// </summary>
  const YoRigine::RootParameterIndices &
  GetRootParameterIndices(const std::string &key) const;

  /// <summary>
  /// パラメータインデックスを取得（リフレクション生成用）
  /// 使用例: auto idx = manager->GetParameterIndices("Sprite").at("Material");
  /// </summary>
  const std::unordered_map<std::string, UINT> &
  GetParameterIndices(const std::string &pipelineName) const;

  /// <summary>
  /// ブレンドモード別のPSOを取得（パーティクル用）
  /// </summary>
  ID3D12PipelineState *GetBlendModePSO(const std::string &key,
                                       BlendMode blendMode);

  /// <summary>
  /// ブレンドモード上書きを考慮して PSO を解決する。
  ///   blendModeOverride が 0..(kCount未満) かつ該当バリアントがあればそれを返し、
  ///   それ以外（-1=継承 や 未登録）なら key の既定 PSO を返す。
  /// VfxMesh のエレメント単位ブレンド切り替え等に使う。
  /// </summary>
  ID3D12PipelineState *ResolveBlendPSO(const std::string &key,
                                       int blendModeOverride);

  /// <summary>
  /// PSOキャッシュの統計情報を取得
  /// </summary>
  const YoRigine::PSOCache::Stats &GetCacheStats() const {
    return psoCache_->GetStats();
  }

private:
  YPipelineManager() = default;
  ~YPipelineManager() = default;
  YPipelineManager(const YPipelineManager &) = delete;
  YPipelineManager &operator=(const YPipelineManager &) = delete;

  // ===== 基本パイプライン作成関数群 =====

  /// <summary>
  /// スプライト描画用パイプライン
  /// </summary>
  void CreatePSO_Sprite();

  /// <summary>
  /// 3Dオブジェクト描画用パイプライン
  /// </summary>
  void CreatePSO_Object();

  /// <summary>
  /// シャドウマップ用パイプライン
  /// </summary>
  void CreatePSO_ShadowMap();

  /// <summary>
  /// インスタンシング描画用パイプライン (Object3dInstanced.VS/PS.hlsl)
  /// </summary>
  void CreatePSO_ObjectInstanced();

  /// <summary>
  /// 半透明インスタンシング描画用パイプライン — depth-write OFF (depth-test ON)
  /// カメラ遮蔽フェードで壁を半透明にする際に使用
  /// </summary>

  /// <summary>
  /// インスタンシング影描画用パイプライン (ShadowmapInstanced.VS.hlsl)
  /// </summary>
  void CreatePSO_ShadowMapInstanced();

  /// <summary>
  /// インバートハル輪郭線用パイプライン (OutLine.VS/PS.hlsl)
  /// 前面カリングで押し出しシェルの背面のみ描画し、シルエットを縁取る。
  /// </summary>
  void CreatePSO_ObjectOutline();

  /// <summary>
  /// インバートハル輪郭線用パイプライン（インスタンス描画版 /
  /// OutLineInstanced.VS） per-instance
  /// のワールド行列でシェルを押し出し、静的インスタンスにも輪郭を付ける。
  /// </summary>
  void CreatePSO_ObjectOutlineInstanced();

  void CreatePSO_GPUParticleALLBlendModes();
  void CreatePSO_YParticleAllBlendModes();
  /// <summary>
  /// 手動ルートシグネチャビルダーを使用したオブジェクト用パイプライン
  /// </summary>
  void CreatePSO_Object_Manual();

  /// <summary>
  /// Yパーティクルシステム用パイプライン
  /// </summary>
  void CreatePSO_YParticle();

  /// <summary>
  /// GPUパーティクル初期化用パイプライン
  /// </summary>
  void CreatePSO_GPUParticleInit();

  /// <summary>
  /// ライン描画用パイプライン
  /// </summary>
  void CreatePSO_Line();

  /// <summary>
  /// InstancedCube (ライン形状を StructuredBuffer + 行列で描画) 用パイプライン
  /// </summary>
  void CreatePSO_InstancedCube();

  /// <summary>
  /// キューブマップ（スカイボックス）描画用パイプライン
  /// </summary>
  void CreatePSO_CubeMap();

  /// <summary>
  /// エフェクト用オブジェクト描画パイプライン
  /// </summary>
  void CreatePSO_EffectObject();

  /// <summary>
  /// Meshを使用したVFX
  /// </summary>
  void CreatePSO_VfxMeshTrail();
  void CreatePSO_VfxMeshVolume();
  void CreatePSO_VfxMeshSmoke();
  void CreatePSO_VfxMeshLightning();
  void CreatePSO_VfxMeshShockwave();
  void CreatePSO_VfxMeshAreaField();
  void CreatePSO_VfxMeshRimFx();

  /// <summary>
  /// VfxMesh 系 PSO を全ブレンドモード分まとめて生成するヘルパー。
  ///   logicalName … "VfxMeshSmoke" 等の論理キー
  ///   psPath      … ピクセルシェーダのパス（VS は VfxMesh.VS 共通）
  ///   defaultMode … blendModePipelineStates_ とは別に、
  ///                 既定 PSO(pipelineStates_[logicalName]) / ルートシグネチャ /
  ///                 パラメータインデックスとして採用するモード（従来の見た目を維持）
  /// </summary>
  void CreateVfxMeshBlendPSOs(const std::string &logicalName,
                              const std::wstring &psPath,
                              BlendMode defaultMode);
  // ===== ポストエフェクト系パイプライン作成関数群 =====

  /// <summary>
  /// オフスクリーンレンダリング基本パイプライン
  /// </summary>
  void CreatePSO_BaseOffScreen(const std::wstring &pixelShaderPath = L"",
                               const std::string &pipelineKey = "");

  /// <summary>
  /// スムージング（ぼかし）用パイプライン
  /// </summary>
  void CreatePSO_Smoothing(const std::wstring &pixelShaderPath = L"",
                           const std::string &pipelineKey = "");

  /// <summary>
  /// 深度ベースアウトライン用パイプライン
  /// </summary>
  void CreatePSO_DepthOutLine(const std::wstring &pixelShaderPath = L"",
                              const std::string &pipelineKey = "");

  /// <summary>
  /// ラジアルブラー用パイプライン
  /// </summary>
  void CreatePSO_RadialBlur(const std::wstring &pixelShaderPath = L"",
                            const std::string &pipelineKey = "");

  /// <summary>
  /// トーンマッピング用パイプライン
  /// </summary>
  void CreatePSO_ToneMapping(const std::wstring &pixelShaderPath = L"",
                             const std::string &pipelineKey = "");

  /// <summary>
  /// ディゾルブエフェクト用パイプライン
  /// </summary>
  void CreatePSO_Dissolve(const std::wstring &pixelShaderPath = L"",
                          const std::string &pipelineKey = "");

  /// <summary>
  /// 色収差エフェクト用パイプライン
  /// </summary>
  void CreatePSO_Chromatic(const std::wstring &pixelShaderPath = L"",
                           const std::string &pipelineKey = "");

  /// <summary>
  /// カラー調整用パイプライン
  /// </summary>
  void CreatePSO_ColorAdjust(const std::wstring &pixelShaderPath = L"",
                             const std::string &pipelineKey = "");

  /// <summary>
  /// シャッタートランジション用パイプライン
  /// </summary>
  void CreatePSO_ShatterTransition(const std::wstring &pixelShaderPath = L"",
                                   const std::string &pipelineKey = "");

  /// <summary>
  /// BlendModeからD3D12_BLEND_DESCを取得
  /// </summary>
  D3D12_BLEND_DESC GetBlendDescFromMode(BlendMode mode);

  // ===== メンバ変数 =====

  YoRigine::DirectXCommon *dxCommon_ = nullptr;

  /// <summary>
  /// PSOキャッシュシステム（バイナリキャッシュ対応）
  /// </summary>
  std::unique_ptr<YoRigine::PSOCache> psoCache_;
  std::unique_ptr<YoRigine::CompletePipelineCache> completePipelineCache_;
  /// <summary>
  /// パイプラインステートオブジェクトのマップ
  /// キー: パイプライン名（例: "Sprite", "Object", "Particle"）
  /// </summary>
  std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>
      pipelineStates_;

  /// <summary>
  /// ブレンドモード別PSO（パーティクル用）
  /// </summary>
  std::unordered_map<
      std::string, std::unordered_map<
                       BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>>>
      blendModePipelineStates_;

  /// <summary>
  /// ルートシグネチャのマップ
  /// キー: パイプライン名
  /// </summary>
  std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>>
      rootSignatures_;

  /// <summary>
  /// ルートパラメータのインデックスマップ（手動ビルダー用）
  /// キー: パイプライン名
  /// 値: パラメータ名とインデックスのマップ
  /// </summary>
  std::unordered_map<std::string, YoRigine::RootParameterIndices>
      rootParamIndices_;

  /// <summary>
  /// リフレクション用のパラメータインデックス
  /// パイプライン名 -> (パラメータ名 -> インデックス)
  ///
  /// 使用例:
  /// auto& indices = manager->GetParameterIndices("Sprite");
  /// UINT materialIdx = indices.at("Material");
  /// commandList->SetGraphicsRootConstantBufferView(materialIdx, address);
  /// </summary>
  std::unordered_map<std::string, std::unordered_map<std::string, UINT>>
      parameterIndices_;
};
