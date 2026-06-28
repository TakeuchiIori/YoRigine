#pragma once

#include <string>
#include <vector>
#include <memory>
#include <json.hpp>
#include "RootSignatureBuilder.h"
#include "PSOCache.h"
#include <dxcapi.h>

namespace YoRigine {

    /// <summary>
    /// JSON設定ファイルからパイプラインを自動生成
    /// UE4/UE5のマテリアルエディタのような設定ベース方式
    /// 
    /// 設定ファイル例 (Sprite.pipeline.json):
    /// {
    ///   "name": "Sprite",
    ///   "rootSignature": {
    ///     "parameters": [
    ///       { "name": "Material", "type": "CBV", "register": 0, "visibility": "PIXEL" },
    ///       { "name": "Transform", "type": "CBV", "register": 0, "visibility": "VERTEX" },
    ///       { "name": "Texture", "type": "SRV_TABLE", "register": 0, "count": 1, "visibility": "PIXEL" }
    ///     ],
    ///     "samplers": [
    ///       { "register": 0, "filter": "LINEAR", "addressMode": "WRAP" }
    ///     ]
    ///   },
    ///   "shaders": {
    ///     "vertex": "Resources/Shaders/Sprite.VS.hlsl",
    ///     "pixel": "Resources/Shaders/Sprite.PS.hlsl"
    ///   },
    ///   "inputLayout": [
    ///     { "semantic": "POSITION", "format": "R32G32B32A32_FLOAT" },
    ///     { "semantic": "TEXCOORD", "format": "R32G32_FLOAT" },
    ///     { "semantic": "NORMAL", "format": "R32G32B32_FLOAT" }
    ///   ],
    ///   "renderState": {
    ///     "blendMode": "AlphaBlend",
    ///     "cullMode": "BACK",
    ///     "fillMode": "SOLID",
    ///     "depthEnable": true,
    ///     "depthWrite": true,
    ///     "depthFunc": "LESS"
    ///   }
    /// }
    /// </summary>
    class PipelineConfigLoader {
    public:
        struct RootParameterConfig {
            std::string name;
            std::string type;  // "CBV", "SRV_TABLE", "UAV_TABLE", "CONSTANT_32BIT"
            UINT shaderRegister;
            UINT descriptorCount = 1;  // テーブルの場合
            std::string visibility;  // "ALL", "VERTEX", "PIXEL", "GEOMETRY", "HULL", "DOMAIN"
            UINT registerSpace = 0;
        };

        struct SamplerConfig {
            UINT shaderRegister;
            std::string filter;  // "LINEAR", "POINT", "ANISOTROPIC"
            std::string addressMode;  // "WRAP", "CLAMP", "MIRROR"
            std::string visibility = "PIXEL";
        };

        struct RootSignatureConfig {
            std::vector<RootParameterConfig> parameters;
            std::vector<SamplerConfig> samplers;
        };

        struct ShaderConfig {
            std::wstring vertex;
            std::wstring pixel;
            std::wstring geometry;
            std::wstring hull;
            std::wstring domain;
            std::wstring compute;
        };

        struct InputElementConfig {
            std::string semantic;
            UINT semanticIndex = 0;
            std::string format;  // "R32G32B32A32_FLOAT", "R32G32_FLOAT", etc.
        };

        struct RenderStateConfig {
            std::string blendMode = "AlphaBlend";
            std::string cullMode = "BACK";
            std::string fillMode = "SOLID";
            bool depthEnable = true;
            bool depthWrite = true;
            std::string depthFunc = "LESS";
            std::string primitiveTopology = "TRIANGLE";
        };

        struct PipelineConfig {
            std::string name;
            RootSignatureConfig rootSignature;
            ShaderConfig shaders;
            std::vector<InputElementConfig> inputLayout;
            RenderStateConfig renderState;
        };

        /// <summary>
        /// JSONファイルから設定を読み込み
        /// </summary>
        static PipelineConfig LoadFromFile(const std::string& filePath);

        /// <summary>
        /// 設定からルートシグネチャを構築
        /// </summary>
        static Microsoft::WRL::ComPtr<ID3D12RootSignature> BuildRootSignature(
            ID3D12Device* device,
            const RootSignatureConfig& config,
            RootParameterIndices& outIndices
        );

        /// <summary>
        /// 設定からパイプラインステートを構築
        /// </summary>
        static Microsoft::WRL::ComPtr<ID3D12PipelineState> BuildPipelineState(
            ID3D12Device* device,
            PSOCache* psoCache,
            const PipelineConfig& config,
            ID3D12RootSignature* rootSignature,
            std::function<Microsoft::WRL::ComPtr<IDxcBlob>(const std::wstring&, const std::wstring&)> compileShader
        );

    private:
        static D3D12_SHADER_VISIBILITY ParseVisibility(const std::string& str);
        static D3D12_FILTER ParseFilter(const std::string& str);
        static D3D12_TEXTURE_ADDRESS_MODE ParseAddressMode(const std::string& str);
        static DXGI_FORMAT ParseFormat(const std::string& str);
        static D3D12_CULL_MODE ParseCullMode(const std::string& str);
        static D3D12_FILL_MODE ParseFillMode(const std::string& str);
        static D3D12_COMPARISON_FUNC ParseComparisonFunc(const std::string& str);
        static D3D12_PRIMITIVE_TOPOLOGY_TYPE ParsePrimitiveTopology(const std::string& str);
        static D3D12_BLEND_DESC ParseBlendMode(const std::string& str);
    };

    /// <summary>
    /// パイプライン設定を一括管理
    /// ディレクトリ内のすべての.pipeline.jsonファイルを自動読み込み
    /// </summary>
    class PipelineConfigManager {
    public:
        void Initialize(const std::string& configDirectory);

        /// <summary>
        /// すべての設定ファイルを読み込み
        /// </summary>
        void LoadAllConfigs();

        /// <summary>
        /// 設定を取得
        /// </summary>
        const PipelineConfigLoader::PipelineConfig* GetConfig(const std::string& name) const;

        /// <summary>
        /// すべての設定を自動的にパイプラインとして構築
        /// </summary>
        void BuildAll(
            ID3D12Device* device,
            PSOCache* psoCache,
            std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>>& rootSignatures,
            std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>& pipelineStates,
            std::unordered_map<std::string, RootParameterIndices>& rootParamIndices,
            std::function<Microsoft::WRL::ComPtr<IDxcBlob>(const std::wstring&, const std::wstring&)> compileShader
        );

    private:
        std::string configDirectory_;
        std::unordered_map<std::string, PipelineConfigLoader::PipelineConfig> configs_;
    };

} // namespace YoRigine