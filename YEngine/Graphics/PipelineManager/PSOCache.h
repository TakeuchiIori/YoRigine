#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <vector>

namespace YoRigine {

    /// <summary>
/// 完全版パイプラインキャッシュシステム
/// PSO + RootSignature + ParameterIndices + InputLayout をまとめてキャッシュ
/// </summary>
    class CompletePipelineCache {
    public:
        /// <summary>
        /// パイプライン全体のデータ
        /// </summary>
        struct PipelineData {
            Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
            Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
            std::unordered_map<std::string, UINT> parameterIndices;
            std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;

            // InputLayoutのsemanticName用メモリ（ポインタの寿命管理）
            std::vector<std::string> semanticNames;

            // 入力（コンパイル済みシェーダー等）のハッシュ。
            // ディスクキャッシュは「出力PSOブロブ」ではなくこの値で同一判定する。
            // （GetCachedBlob() はドライバ依存で毎回変化するため出力比較は不可）
            uint64_t inputHash = 0;
        };

        /// <summary>
        /// FNV-1a でバイト列のハッシュを計算する（複数ブロブを seed で連結可能）。
        /// 例: h = HashData(vs.ptr, vs.size); h = HashData(ps.ptr, ps.size, h);
        /// </summary>
        static uint64_t HashData(const void* data, size_t size,
            uint64_t seed = 0xcbf29ce484222325ULL);

        CompletePipelineCache() = default;
        ~CompletePipelineCache() = default;

        void Initialize(ID3D12Device* device, const std::filesystem::path& cacheDirectory);

        /// <summary>
        /// パイプライン全体をキャッシュから取得（メモリ優先、次にディスク）
        /// </summary>
        PipelineData* Get(const std::string& key);

        /// <summary>
        /// パイプライン全体をキャッシュに追加（自動的にディスクにも保存）
        /// </summary>
        void Add(const std::string& key, const PipelineData& data);

        /// <summary>
        /// すべてのキャッシュをディスクに保存
        /// </summary>
        void SaveAll();

        /// <summary>
        /// ディスクからすべてのキャッシュを読み込み（起動時に自動実行）
        /// </summary>
        void LoadAll();

        /// <summary>
        /// キャッシュをクリア
        /// </summary>
        void ClearCache();

        struct Stats {
            size_t totalPipelines = 0;
            size_t memoryHits = 0;
            size_t memoryMisses = 0;
            size_t diskHits = 0;
            size_t diskMisses = 0;
        };
        const Stats& GetStats() const { return stats_; }

    private:
        std::filesystem::path GetCacheFilePath(const std::string& key) const;

        bool SaveToFile(const std::string& key, const PipelineData& data);
        bool LoadFromFile(const std::string& key);

        ID3D12Device* device_ = nullptr;
        std::filesystem::path cacheDirectory_;
        std::unordered_map<std::string, PipelineData> cache_;
        Stats stats_;
    };

    /// <summary>
    /// UE4/UE5のようなPSOバイナリキャッシュシステム
    /// パイプラインステートをディスクにキャッシュして、起動時間を短縮
    /// </summary>
    class PSOCache {
    public:
        PSOCache() = default;
        ~PSOCache() = default;

        void Initialize(ID3D12Device* device, const std::filesystem::path& cacheDirectory);

        /// <summary>
        /// グラフィックスPSOをキャッシュから取得、なければ作成してキャッシュ
        /// </summary>
        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreate(
            const std::string& key,
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
        );

        /// <summary>
        /// コンピュートPSOをキャッシュから取得、なければ作成してキャッシュ
        /// </summary>
        Microsoft::WRL::ComPtr<ID3D12PipelineState> GetOrCreate(
            const std::string& key,
            const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc
        );

        void SaveAll();
        void ClearCache();

        struct Stats {
            size_t totalPSOs = 0;
            size_t cacheHits = 0;
            size_t cacheMisses = 0;
            size_t diskCacheHits = 0;
        };
        const Stats& GetStats() const { return stats_; }

    private:
        std::filesystem::path GetCacheFilePath(const std::string& key) const;

        /// <summary>
        /// ディスクからグラフィックスPSOをロード（完全版）
        /// </summary>
        Microsoft::WRL::ComPtr<ID3D12PipelineState> LoadFromDisk(
            const std::string& key,
            const D3D12_GRAPHICS_PIPELINE_STATE_DESC& originalDesc
        );

        /// <summary>
        /// ディスクからコンピュートPSOをロード（完全版）
        /// </summary>
        Microsoft::WRL::ComPtr<ID3D12PipelineState> LoadFromDiskCompute(
            const std::string& key,
            const D3D12_COMPUTE_PIPELINE_STATE_DESC& originalDesc
        );

        /// <summary>
        /// グラフィックスPSOをディスクに保存（完全版）
        /// </summary>
        void SaveToDisk(
            const std::string& key,
            ID3D12PipelineState* pso
        );

        /// <summary>
        /// コンピュートPSOをディスクに保存（完全版）
        /// </summary>
        void SaveToDiskCompute(
            const std::string& key,
            ID3D12PipelineState* pso
        );

        uint64_t CalculateHash(const void* data, size_t size) const;

        ID3D12Device* device_ = nullptr;
        std::filesystem::path cacheDirectory_;
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>> psoCache_;
        Stats stats_;
    };

    /// <summary>
    /// パイプライン設定を簡単に構築するビルダー
    /// </summary>
    class GraphicsPipelineBuilder {
    public:
        GraphicsPipelineBuilder();

        /// <summary>
        /// ルートシグネチャを設定
        /// </summary>
        GraphicsPipelineBuilder& SetRootSignature(ID3D12RootSignature* rootSignature);

        /// <summary>
        /// 頂点シェーダーを設定
        /// </summary>
        GraphicsPipelineBuilder& SetVertexShader(const void* bytecode, size_t size);
        GraphicsPipelineBuilder& SetVertexShader(ID3DBlob* blob);

        /// <summary>
        /// ピクセルシェーダーを設定
        /// </summary>
        GraphicsPipelineBuilder& SetPixelShader(const void* bytecode, size_t size);
        GraphicsPipelineBuilder& SetPixelShader(ID3DBlob* blob);

        /// <summary>
        /// ジオメトリシェーダーを設定
        /// </summary>
        GraphicsPipelineBuilder& SetGeometryShader(const void* bytecode, size_t size);

        /// <summary>
        /// インプットレイアウトを設定
        /// </summary>
        GraphicsPipelineBuilder& SetInputLayout(
            const D3D12_INPUT_ELEMENT_DESC* elements,
            UINT numElements
        );

        /// <summary>
        /// ブレンドステートを設定
        /// </summary>
        GraphicsPipelineBuilder& SetBlendState(const D3D12_BLEND_DESC& blendDesc);

        /// <summary>
        /// ラスタライザステートを設定
        /// </summary>
        GraphicsPipelineBuilder& SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizerDesc);

        /// <summary>
        /// デプスステンシルステートを設定
        /// </summary>
        GraphicsPipelineBuilder& SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc);

        /// <summary>
        /// レンダーターゲットフォーマットを設定
        /// </summary>
        GraphicsPipelineBuilder& SetRenderTargetFormats(
            const DXGI_FORMAT* formats,
            UINT numRenderTargets
        );

        /// <summary>
        /// 深度ステンシルフォーマットを設定
        /// </summary>
        GraphicsPipelineBuilder& SetDepthStencilFormat(DXGI_FORMAT format);

        /// <summary>
        /// プリミティブトポロジータイプを設定
        /// </summary>
        GraphicsPipelineBuilder& SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology);

        /// <summary>
        /// サンプル設定
        /// </summary>
        GraphicsPipelineBuilder& SetSampleDesc(UINT count = 1, UINT quality = 0);

        /// <summary>
        /// デフォルト設定を適用（よく使う設定をまとめて適用）
        /// </summary>
        GraphicsPipelineBuilder& ApplyDefaultSettings();

        /// <summary>
        /// パイプラインステートディスクリプションを取得
        /// </summary>
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& GetDesc() const { return desc_; }

        /// <summary>
        /// ビルド（実際のPSO作成はPSOCacheを通して行う）
        /// </summary>
        D3D12_GRAPHICS_PIPELINE_STATE_DESC Build() const { return desc_; }

    private:
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc_;
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements_;  // 寿命管理用
    };

    /// <summary>
    /// よく使うブレンドモード設定のヘルパー関数
    /// </summary>
    namespace BlendPresets {
        D3D12_BLEND_DESC CreateNone();
        D3D12_BLEND_DESC CreateAlphaBlend();
        D3D12_BLEND_DESC CreateAdditive();
        D3D12_BLEND_DESC CreateSubtractive();
        D3D12_BLEND_DESC CreateMultiply();
        D3D12_BLEND_DESC CreateScreen();
    }

    /// <summary>
    /// よく使うラスタライザー設定のヘルパー関数
    /// </summary>
    namespace RasterizerPresets {
        D3D12_RASTERIZER_DESC CreateDefault();
        D3D12_RASTERIZER_DESC CreateNoCull();
        D3D12_RASTERIZER_DESC CreateWireframe();
        D3D12_RASTERIZER_DESC CreateShadow();
    }

    /// <summary>
    /// よく使うデプスステンシル設定のヘルパー関数
    /// </summary>
    namespace DepthStencilPresets {
        D3D12_DEPTH_STENCIL_DESC CreateDefault();
        D3D12_DEPTH_STENCIL_DESC CreateReadOnly();
        D3D12_DEPTH_STENCIL_DESC CreateDisabled();
        D3D12_DEPTH_STENCIL_DESC CreateWriteOnly();
    }

    namespace SamplerPresets{
        D3D12_STATIC_SAMPLER_DESC CreateDefault();
        D3D12_STATIC_SAMPLER_DESC CreateShadowMap();
    }

    namespace PrimitiveTopologyPresets {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE Triangle();
        D3D12_PRIMITIVE_TOPOLOGY_TYPE Line();
        D3D12_PRIMITIVE_TOPOLOGY_TYPE Point();
        D3D12_PRIMITIVE_TOPOLOGY_TYPE Patch();
	}

} // namespace YoRigine