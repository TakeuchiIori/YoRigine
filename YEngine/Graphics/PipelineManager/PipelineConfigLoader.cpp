#include "PipelineConfigLoader.h"
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "Debugger/Logger.h"

namespace YoRigine {

    // ========== PipelineConfigLoader 実装 ==========

    PipelineConfigLoader::PipelineConfig PipelineConfigLoader::LoadFromFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open pipeline config file: " + filePath);
        }

        nlohmann::json j;
        file >> j;

        PipelineConfig config;
        config.name = j["name"].get<std::string>();

        // ルートシグネチャ設定
        if (j.contains("rootSignature")) {
            auto& rsJson = j["rootSignature"];

            // パラメータ
            if (rsJson.contains("parameters")) {
                for (auto& paramJson : rsJson["parameters"]) {
                    RootParameterConfig param;
                    param.name = paramJson["name"].get<std::string>();
                    param.type = paramJson["type"].get<std::string>();
                    param.shaderRegister = paramJson["register"].get<UINT>();
                    param.visibility = paramJson.value("visibility", "ALL");

                    if (paramJson.contains("count")) {
                        param.descriptorCount = paramJson["count"].get<UINT>();
                    }
                    if (paramJson.contains("registerSpace")) {
                        param.registerSpace = paramJson["registerSpace"].get<UINT>();
                    }

                    config.rootSignature.parameters.push_back(param);
                }
            }

            // サンプラー
            if (rsJson.contains("samplers")) {
                for (auto& samplerJson : rsJson["samplers"]) {
                    SamplerConfig sampler;
                    sampler.shaderRegister = samplerJson["register"].get<UINT>();
                    sampler.filter = samplerJson.value("filter", "LINEAR");
                    sampler.addressMode = samplerJson.value("addressMode", "WRAP");
                    sampler.visibility = samplerJson.value("visibility", "PIXEL");
                    config.rootSignature.samplers.push_back(sampler);
                }
            }
        }

        // シェーダー設定
        if (j.contains("shaders")) {
            auto& shaderJson = j["shaders"];
            if (shaderJson.contains("vertex")) {
                std::string vs = shaderJson["vertex"].get<std::string>();
                config.shaders.vertex = std::wstring(vs.begin(), vs.end());
            }
            if (shaderJson.contains("pixel")) {
                std::string ps = shaderJson["pixel"].get<std::string>();
                config.shaders.pixel = std::wstring(ps.begin(), ps.end());
            }
            if (shaderJson.contains("geometry")) {
                std::string gs = shaderJson["geometry"].get<std::string>();
                config.shaders.geometry = std::wstring(gs.begin(), gs.end());
            }
        }

        // インプットレイアウト
        if (j.contains("inputLayout")) {
            for (auto& elementJson : j["inputLayout"]) {
                InputElementConfig element;
                element.semantic = elementJson["semantic"].get<std::string>();
                element.semanticIndex = elementJson.value("semanticIndex", 0);
                element.format = elementJson["format"].get<std::string>();
                config.inputLayout.push_back(element);
            }
        }

        // レンダーステート
        if (j.contains("renderState")) {
            auto& rsJson = j["renderState"];
            config.renderState.blendMode = rsJson.value("blendMode", "AlphaBlend");
            config.renderState.cullMode = rsJson.value("cullMode", "BACK");
            config.renderState.fillMode = rsJson.value("fillMode", "SOLID");
            config.renderState.depthEnable = rsJson.value("depthEnable", true);
            config.renderState.depthWrite = rsJson.value("depthWrite", true);
            config.renderState.depthFunc = rsJson.value("depthFunc", "LESS");
            config.renderState.primitiveTopology = rsJson.value("primitiveTopology", "TRIANGLE");
        }

        return config;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> PipelineConfigLoader::BuildRootSignature(
        ID3D12Device* device,
        const RootSignatureConfig& config,
        RootParameterIndices& outIndices
    ) {
        RootSignatureBuilder builder;

        // パラメータを追加
        for (const auto& param : config.parameters) {
            D3D12_SHADER_VISIBILITY visibility = ParseVisibility(param.visibility);

            if (param.type == "CBV") {
                builder.AddCBV(param.name, param.shaderRegister, visibility, param.registerSpace);
            }
            else if (param.type == "SRV_TABLE") {
                builder.AddDescriptorTable(param.name, param.shaderRegister,
                    param.descriptorCount, visibility, param.registerSpace);
            }
            else if (param.type == "UAV_TABLE") {
                builder.AddUAVTable(param.name, param.shaderRegister,
                    param.descriptorCount, visibility, param.registerSpace);
            }
            else if (param.type == "CONSTANT_32BIT") {
                builder.Add32BitConstants(param.name, param.descriptorCount,
                    param.shaderRegister, visibility, param.registerSpace);
            }
        }

        // サンプラーを追加
        for (const auto& sampler : config.samplers) {
            builder.AddStaticSampler(
                sampler.shaderRegister,
                ParseFilter(sampler.filter),
                ParseAddressMode(sampler.addressMode),
                ParseVisibility(sampler.visibility)
            );
        }

        // インデックス情報を保存
        outIndices.InitializeFrom(builder);

        // ビルド
        return builder.Build(device);
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PipelineConfigLoader::BuildPipelineState(
        [[maybe_unused]] ID3D12Device* device,
        PSOCache* psoCache,
        const PipelineConfig& config,
        ID3D12RootSignature* rootSignature,
        std::function<Microsoft::WRL::ComPtr<IDxcBlob>(const std::wstring&, const std::wstring&)> compileShader
    ) {
        GraphicsPipelineBuilder builder;
        builder.SetRootSignature(rootSignature);

        // シェーダーをコンパイル
        if (!config.shaders.vertex.empty()) {
            auto vs = compileShader(config.shaders.vertex, L"vs_6_0");
            builder.SetVertexShader(vs->GetBufferPointer(), vs->GetBufferSize());
        }
        if (!config.shaders.pixel.empty()) {
            auto ps = compileShader(config.shaders.pixel, L"ps_6_0");
            builder.SetPixelShader(ps->GetBufferPointer(), ps->GetBufferSize());
        }
        if (!config.shaders.geometry.empty()) {
            auto gs = compileShader(config.shaders.geometry, L"gs_6_0");
            builder.SetGeometryShader(gs->GetBufferPointer(), gs->GetBufferSize());
        }

        // インプットレイアウト
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
        for (const auto& element : config.inputLayout) {
            D3D12_INPUT_ELEMENT_DESC desc{};
            desc.SemanticName = element.semantic.c_str();
            desc.SemanticIndex = element.semanticIndex;
            desc.Format = ParseFormat(element.format);
            desc.InputSlot = 0;
            desc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
            desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
            desc.InstanceDataStepRate = 0;
            inputElements.push_back(desc);
        }

        if (!inputElements.empty()) {
            builder.SetInputLayout(inputElements.data(), static_cast<UINT>(inputElements.size()));
        }

        // レンダーステート
        builder.SetBlendState(ParseBlendMode(config.renderState.blendMode));

        D3D12_RASTERIZER_DESC rasterizer = RasterizerPresets::CreateDefault();
        rasterizer.CullMode = ParseCullMode(config.renderState.cullMode);
        rasterizer.FillMode = ParseFillMode(config.renderState.fillMode);
        builder.SetRasterizerState(rasterizer);

        D3D12_DEPTH_STENCIL_DESC depthStencil = DepthStencilPresets::CreateDefault();
        depthStencil.DepthEnable = config.renderState.depthEnable;
        depthStencil.DepthWriteMask = config.renderState.depthWrite ?
            D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencil.DepthFunc = ParseComparisonFunc(config.renderState.depthFunc);
        builder.SetDepthStencilState(depthStencil);

        builder.SetPrimitiveTopology(ParsePrimitiveTopology(config.renderState.primitiveTopology));

        auto desc = builder.Build();
        return psoCache->GetOrCreate(config.name, desc);
    }

    // ========== パース関数 ==========

    D3D12_SHADER_VISIBILITY PipelineConfigLoader::ParseVisibility(const std::string& str) {
        if (str == "ALL") return D3D12_SHADER_VISIBILITY_ALL;
        if (str == "VERTEX") return D3D12_SHADER_VISIBILITY_VERTEX;
        if (str == "PIXEL") return D3D12_SHADER_VISIBILITY_PIXEL;
        if (str == "GEOMETRY") return D3D12_SHADER_VISIBILITY_GEOMETRY;
        if (str == "HULL") return D3D12_SHADER_VISIBILITY_HULL;
        if (str == "DOMAIN") return D3D12_SHADER_VISIBILITY_DOMAIN;
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_FILTER PipelineConfigLoader::ParseFilter(const std::string& str) {
        if (str == "POINT") return D3D12_FILTER_MIN_MAG_MIP_POINT;
        if (str == "LINEAR") return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        if (str == "ANISOTROPIC") return D3D12_FILTER_ANISOTROPIC;
        return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }

    D3D12_TEXTURE_ADDRESS_MODE PipelineConfigLoader::ParseAddressMode(const std::string& str) {
        if (str == "WRAP") return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        if (str == "CLAMP") return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        if (str == "MIRROR") return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        if (str == "BORDER") return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }

    DXGI_FORMAT PipelineConfigLoader::ParseFormat(const std::string& str) {
        if (str == "R32G32B32A32_FLOAT") return DXGI_FORMAT_R32G32B32A32_FLOAT;
        if (str == "R32G32B32_FLOAT") return DXGI_FORMAT_R32G32B32_FLOAT;
        if (str == "R32G32_FLOAT") return DXGI_FORMAT_R32G32_FLOAT;
        if (str == "R32_FLOAT") return DXGI_FORMAT_R32_FLOAT;
        if (str == "R8G8B8A8_UNORM") return DXGI_FORMAT_R8G8B8A8_UNORM;
        return DXGI_FORMAT_UNKNOWN;
    }

    D3D12_CULL_MODE PipelineConfigLoader::ParseCullMode(const std::string& str) {
        if (str == "NONE") return D3D12_CULL_MODE_NONE;
        if (str == "FRONT") return D3D12_CULL_MODE_FRONT;
        if (str == "BACK") return D3D12_CULL_MODE_BACK;
        return D3D12_CULL_MODE_BACK;
    }

    D3D12_FILL_MODE PipelineConfigLoader::ParseFillMode(const std::string& str) {
        if (str == "WIREFRAME") return D3D12_FILL_MODE_WIREFRAME;
        if (str == "SOLID") return D3D12_FILL_MODE_SOLID;
        return D3D12_FILL_MODE_SOLID;
    }

    D3D12_COMPARISON_FUNC PipelineConfigLoader::ParseComparisonFunc(const std::string& str) {
        if (str == "NEVER") return D3D12_COMPARISON_FUNC_NEVER;
        if (str == "LESS") return D3D12_COMPARISON_FUNC_LESS;
        if (str == "EQUAL") return D3D12_COMPARISON_FUNC_EQUAL;
        if (str == "LESS_EQUAL") return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        if (str == "GREATER") return D3D12_COMPARISON_FUNC_GREATER;
        if (str == "NOT_EQUAL") return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        if (str == "GREATER_EQUAL") return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        if (str == "ALWAYS") return D3D12_COMPARISON_FUNC_ALWAYS;
        return D3D12_COMPARISON_FUNC_LESS;
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE PipelineConfigLoader::ParsePrimitiveTopology(const std::string& str) {
        if (str == "POINT") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        if (str == "LINE") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        if (str == "TRIANGLE") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        if (str == "PATCH") return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }

    D3D12_BLEND_DESC PipelineConfigLoader::ParseBlendMode(const std::string& str) {
        if (str == "None") return BlendPresets::CreateNone();
        if (str == "AlphaBlend") return BlendPresets::CreateAlphaBlend();
        if (str == "Additive") return BlendPresets::CreateAdditive();
        if (str == "Subtractive") return BlendPresets::CreateSubtractive();
        if (str == "Multiply") return BlendPresets::CreateMultiply();
        if (str == "Screen") return BlendPresets::CreateScreen();
        return BlendPresets::CreateAlphaBlend();
    }

    // ========== PipelineConfigManager 実装 ==========

    void PipelineConfigManager::Initialize(const std::string& configDirectory) {
        configDirectory_ = configDirectory;
    }

    void PipelineConfigManager::LoadAllConfigs() {
        configs_.clear();

        if (!std::filesystem::exists(configDirectory_)) {
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(configDirectory_)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find(".pipeline.") != std::string::npos) {

                try {
                    auto config = PipelineConfigLoader::LoadFromFile(entry.path().string());
                    configs_[config.name] = config;
                }
                catch (const std::exception& e) {
                    // ログ出力など
                    Logger(("Failed to load pipeline config: " + entry.path().string() +
                        " - " + e.what() + "\n").c_str());
                }
            }
        }
    }

    const PipelineConfigLoader::PipelineConfig* PipelineConfigManager::GetConfig(const std::string& name) const {
        auto it = configs_.find(name);
        return (it != configs_.end()) ? &it->second : nullptr;
    }

    void PipelineConfigManager::BuildAll(
        ID3D12Device* device,
        PSOCache* psoCache,
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12RootSignature>>& rootSignatures,
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D12PipelineState>>& pipelineStates,
        std::unordered_map<std::string, RootParameterIndices>& rootParamIndices,
        std::function<Microsoft::WRL::ComPtr<IDxcBlob>(const std::wstring&, const std::wstring&)> compileShader
    ) {
        for (const auto& [name, config] : configs_) {
            try {
                // ルートシグネチャを構築
                RootParameterIndices indices;
                auto rootSig = PipelineConfigLoader::BuildRootSignature(
                    device, config.rootSignature, indices
                );
                rootSignatures[name] = rootSig;
                rootParamIndices[name] = indices;

                // パイプラインステートを構築
                auto pso = PipelineConfigLoader::BuildPipelineState(
                    device, psoCache, config, rootSig.Get(), compileShader
                );
                pipelineStates[name] = pso;

            }
            catch (const std::exception& e) {
                Logger(("Failed to build pipeline: " + name +
                    " - " + e.what() + "\n").c_str());
            }
        }
    }

} // namespace YoRigine