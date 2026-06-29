#include "ShaderReflection.h"
#include <cassert>
#include <iostream>
#include <algorithm>
#include <d3dcompiler.h>
#include "PSOCache.h"
#include "Debugger/Logger.h"

namespace YoRigine {

    // ========== ShaderReflection 実装 ==========

    bool ShaderReflection::Reflect(IDxcBlob* shaderBlob, const std::wstring& shaderProfile) {
        if (!shaderBlob) return false;

        // DXCを使用したリフレクション
        Microsoft::WRL::ComPtr<IDxcUtils> utils;
        HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        if (FAILED(hr)) return false;

        DxcBuffer reflectionBuffer{};
        reflectionBuffer.Ptr = shaderBlob->GetBufferPointer();
        reflectionBuffer.Size = shaderBlob->GetBufferSize();
        reflectionBuffer.Encoding = 0;

        Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
        hr = utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflection));
        if (FAILED(hr)) return false;

        // シェーダーの基本情報を取得
        D3D12_SHADER_DESC shaderDesc{};
        reflection->GetDesc(&shaderDesc);

        // シェーダーの可視性を判定
        shaderVisibility_ = DetermineVisibilityFromProfile(shaderProfile);

        // リソースバインディング情報を取得
        for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
            reflection->GetResourceBindingDesc(i, &bindDesc);

            ResourceBinding binding{};
            binding.name = bindDesc.Name;
            binding.type = bindDesc.Type;
            binding.bindPoint = bindDesc.BindPoint;
            binding.bindCount = bindDesc.BindCount;
            binding.space = bindDesc.Space;
            binding.visibility = shaderVisibility_;

            resourceBindings_.push_back(binding);
        }

        // 頂点シェーダーの場合、入力レイアウトを取得
        if (shaderProfile.find(L"vs_") == 0) {
            for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
                D3D12_SIGNATURE_PARAMETER_DESC paramDesc{};
                reflection->GetInputParameterDesc(i, &paramDesc);

                if (strncmp(paramDesc.SemanticName, "SV_", 3) == 0) {
                    continue;
                }

                InputElement element{};
                element.semanticName = paramDesc.SemanticName;
                element.semanticIndex = paramDesc.SemanticIndex;
                element.inputSlot = 0;
                element.alignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
                element.slotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                element.instanceDataStepRate = 0;
                element.format = GetFormat(paramDesc);

                // マスクからフォーマットを推定
                if (paramDesc.Mask == 1) {
                    if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) element.format = DXGI_FORMAT_R32_UINT;
                    else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) element.format = DXGI_FORMAT_R32_SINT;
                    else element.format = DXGI_FORMAT_R32_FLOAT;
                }
                else if (paramDesc.Mask <= 3) {
                    if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) element.format = DXGI_FORMAT_R32G32_UINT;
                    else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) element.format = DXGI_FORMAT_R32G32_SINT;
                    else element.format = DXGI_FORMAT_R32G32_FLOAT;
                }
                else if (paramDesc.Mask <= 7) {
                    if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) element.format = DXGI_FORMAT_R32G32B32_UINT;
                    else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) element.format = DXGI_FORMAT_R32G32B32_SINT;
                    else element.format = DXGI_FORMAT_R32G32B32_FLOAT;
                }
                else {
                    if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) element.format = DXGI_FORMAT_R32G32B32A32_UINT;
                    else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) element.format = DXGI_FORMAT_R32G32B32A32_SINT;
                    else element.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                }

                inputElements_.push_back(element);
            }
        }

        // 定数バッファの詳細情報を取得
        for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
            ID3D12ShaderReflectionConstantBuffer* cbReflection = reflection->GetConstantBufferByIndex(i);
            D3D12_SHADER_BUFFER_DESC bufferDesc{};
            cbReflection->GetDesc(&bufferDesc);

            ConstantBuffer cb{};
            cb.name = bufferDesc.Name;
            cb.size = bufferDesc.Size;

            // このCBのバインドポイントを探す
            for (const auto& binding : resourceBindings_) {
                if (binding.name == cb.name && binding.type == D3D_SIT_CBUFFER) {
                    cb.bindPoint = binding.bindPoint;
                    cb.space = binding.space;
                    break;
                }
            }

            // CB内の変数を取得
            for (UINT j = 0; j < bufferDesc.Variables; ++j) {
                ID3D12ShaderReflectionVariable* varReflection = cbReflection->GetVariableByIndex(j);
                D3D12_SHADER_VARIABLE_DESC varDesc{};
                varReflection->GetDesc(&varDesc);

                ConstantBufferVariable var{};
                var.name = varDesc.Name;
                var.offset = varDesc.StartOffset;
                var.size = varDesc.Size;

                ID3D12ShaderReflectionType* typeReflection = varReflection->GetType();
                D3D12_SHADER_TYPE_DESC typeDesc{};
                typeReflection->GetDesc(&typeDesc);
                var.typeName = typeDesc.Name ? typeDesc.Name : "unknown";

                cb.variables.push_back(var);
            }

            constantBuffers_.push_back(cb);
        }

        return true;
    }

    bool ShaderReflection::ReflectLegacy(ID3DBlob* shaderBlob) {
        if (!shaderBlob) return false;

        Microsoft::WRL::ComPtr<ID3D12ShaderReflection> reflection;
        HRESULT hr = D3DReflect(
            shaderBlob->GetBufferPointer(),
            shaderBlob->GetBufferSize(),
            IID_PPV_ARGS(&reflection)
        );

        if (FAILED(hr)) return false;

        // 以下、Reflect()と同様の処理
        // （実装は省略、基本的に同じロジック）

        return true;
    }

    D3D12_SHADER_VISIBILITY ShaderReflection::DetermineVisibilityFromProfile(const std::wstring& profile) {
        if (profile.find(L"vs_") == 0) return D3D12_SHADER_VISIBILITY_VERTEX;
        if (profile.find(L"ps_") == 0) return D3D12_SHADER_VISIBILITY_PIXEL;
        if (profile.find(L"gs_") == 0) return D3D12_SHADER_VISIBILITY_GEOMETRY;
        if (profile.find(L"hs_") == 0) return D3D12_SHADER_VISIBILITY_HULL;
        if (profile.find(L"ds_") == 0) return D3D12_SHADER_VISIBILITY_DOMAIN;
		if (profile.find(L"cs_") == 0) return D3D12_SHADER_VISIBILITY_ALL; // コンピュートシェーダーは全体に影響
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_ROOT_PARAMETER_TYPE ShaderReflection::ConvertToRootParameterType(D3D_SHADER_INPUT_TYPE inputType) {
        switch (inputType) {
        case D3D_SIT_CBUFFER:
            return D3D12_ROOT_PARAMETER_TYPE_CBV;
        case D3D_SIT_TBUFFER:
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        case D3D_SIT_UAV_RWTYPED:
        case D3D_SIT_UAV_RWSTRUCTURED:
        case D3D_SIT_UAV_RWBYTEADDRESS:
        case D3D_SIT_UAV_APPEND_STRUCTURED:
        case D3D_SIT_UAV_CONSUME_STRUCTURED:
        case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        default:
            return D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        }
    }

    void ShaderReflection::PrintDebugInfo() const {
        Logger("===== Shader Reflection Info =====\n");

        Logger("Resource Bindings:\n");
        for (const auto& binding : resourceBindings_) {
            char buf[256];
            sprintf_s(buf, "  %s: Type=%d, Bind=%d, Space=%d\n",
                binding.name.c_str(), binding.type, binding.bindPoint, binding.space);
            Logger(buf);
        }

        if (!inputElements_.empty()) {
            Logger("Input Elements:\n");
            for (const auto& element : inputElements_) {
                char buf[256];
                sprintf_s(buf, "  %s%d: Format=%d\n",
                    element.semanticName.c_str(), element.semanticIndex, element.format);
                Logger(buf);
            }
        }

        Logger("Constant Buffers:\n");
        for (const auto& cb : constantBuffers_) {
            char buf[256];
            sprintf_s(buf, "  %s: Size=%d, Bind=b%d\n", cb.name.c_str(), cb.size, cb.bindPoint);
            Logger(buf);
            for (const auto& var : cb.variables) {
                sprintf_s(buf, "    %s: Type=%s, Offset=%d, Size=%d\n",
                    var.name.c_str(), var.typeName.c_str(), var.offset, var.size);
                Logger(buf);
            }
        }
    }

    // ========== AutoRootSignatureGenerator 実装 ==========

    void AutoRootSignatureGenerator::AddShaderReflection(const ShaderReflection& reflection) {
        MergeResourceBindings(reflection.GetResourceBindings());

        // 頂点シェーダーの入力レイアウトを取得
        if (!reflection.GetInputElements().empty()) {
            for (const auto& element : reflection.GetInputElements()) {
                // セマンティック名の寿命管理
                semanticNames_.push_back(element.semanticName);

                D3D12_INPUT_ELEMENT_DESC desc{};
                desc.SemanticName = semanticNames_.back().c_str();
                desc.SemanticIndex = element.semanticIndex;
                desc.Format = element.format;
                desc.InputSlot = element.inputSlot;
                desc.AlignedByteOffset = element.alignedByteOffset;
                desc.InputSlotClass = element.slotClass;
                desc.InstanceDataStepRate = element.instanceDataStepRate;

                inputLayout_.push_back(desc);
            }
        }
    }

    /// <summary>
    /// 
    /// </summary>
    /// <param name="vertexShader"></param>
    /// <param name="pixelShader"></param>
    /// <param name="geometryShader"></param>
    /// <param name="hullShader"></param>
    /// <param name="domainShader"></param>
    void AutoRootSignatureGenerator::AddShaders(
        IDxcBlob* vertexShader,
        IDxcBlob* pixelShader,
        IDxcBlob* geometryShader,
        IDxcBlob* hullShader,
        IDxcBlob* domainShader
    ) {
        if (vertexShader) {
            ShaderReflection vsReflection;
            if (vsReflection.Reflect(vertexShader, L"vs_6_0")) {
                AddShaderReflection(vsReflection);
            }
        }
        if (pixelShader) {
            ShaderReflection psReflection;
            if (psReflection.Reflect(pixelShader, L"ps_6_0")) {
                AddShaderReflection(psReflection);
            }
		}
        if (geometryShader) {
            ShaderReflection gsReflection;
            if (gsReflection.Reflect(geometryShader, L"gs_6_0")) {
                AddShaderReflection(gsReflection);
            }
        }
        if (hullShader) {
            ShaderReflection hsReflection;
            if (hsReflection.Reflect(hullShader, L"hs_6_0")) {
                AddShaderReflection(hsReflection);
            }
        }
        if (domainShader) {
            ShaderReflection dsReflection;
            if (dsReflection.Reflect(domainShader, L"ds_6_0")) {
                AddShaderReflection(dsReflection);
            }
		}

    }

    void AutoRootSignatureGenerator::AddComputeShader(IDxcBlob* computeShader)
    {
        if (computeShader) {
            ShaderReflection csReflection;
            if (csReflection.Reflect(computeShader, L"cs_6_0")) {
                AddShaderReflection(csReflection);
            }
        }
    }

    /// <summary>
    /// シェーダーリソースバインディングをマージします。複数のシェーダーステージで使用されるリソースの可視性を統合し、バインドポイントでソートされた統一されたバインディングリストを生成します。
    /// </summary>
    /// <param name="bindings">
    void AutoRootSignatureGenerator::MergeResourceBindings(
        const std::vector<ShaderReflection::ResourceBinding>& bindings
    ) {
        for (const auto& newBinding : bindings) {
            // 既存のバインディングを探す
            auto it = std::find_if(mergedBindings_.begin(), mergedBindings_.end(),
                [&](const MergedResourceBinding& existing) {
                    return existing.name == newBinding.name &&
                        existing.bindPoint == newBinding.bindPoint &&
                        existing.space == newBinding.space;
                });

            if (it != mergedBindings_.end()) {
                // 既存のバインディングを更新
                // 複数のシェーダーステージで使われている場合はALLにする
                if (it->visibility != newBinding.visibility) {
                    it->visibility = D3D12_SHADER_VISIBILITY_ALL;
                }
            }
            else {
                // 新規追加
                MergedResourceBinding merged{};
                merged.name = newBinding.name;
                merged.type = newBinding.type;
                merged.bindPoint = newBinding.bindPoint;
                merged.bindCount = newBinding.bindCount;
                merged.space = newBinding.space;
                merged.visibility = newBinding.visibility;
                mergedBindings_.push_back(merged);
            }
        }

        // バインドポイントでソート（安定したルートパラメータ順序のため）
        std::sort(mergedBindings_.begin(), mergedBindings_.end(),
            [](const MergedResourceBinding& a, const MergedResourceBinding& b) {
                if (a.space != b.space) return a.space < b.space;
                if (a.type != b.type) return a.type < b.type;
                return a.bindPoint < b.bindPoint;
            });
    }

    /// <summary>
    /// ルートシグネチャの生成
    /// </summary>
    /// <returns></returns>
    std::vector<D3D12_ROOT_PARAMETER> AutoRootSignatureGenerator::CreateRootParameters() {
        std::vector<D3D12_ROOT_PARAMETER> rootParams;
        UINT rootIndex = 0;

        // ディスクリプタレンジを保持（寿命管理）
        static std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> descriptorRanges;
        descriptorRanges.clear();

        for (const auto& binding : mergedBindings_) {
            D3D12_ROOT_PARAMETER param{};
            param.ShaderVisibility = binding.visibility;

            switch (binding.type) {
            case D3D_SIT_CBUFFER:
                // 定数バッファは直接記述子
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                param.Descriptor.ShaderRegister = binding.bindPoint;
                param.Descriptor.RegisterSpace = binding.space;
                break;

            case D3D_SIT_TEXTURE:
            case D3D_SIT_STRUCTURED:
            case D3D_SIT_BYTEADDRESS:
                // SRVはディスクリプタテーブル
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                {
                    std::vector<D3D12_DESCRIPTOR_RANGE> ranges(1);
                    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                    ranges[0].NumDescriptors = binding.bindCount;
                    ranges[0].BaseShaderRegister = binding.bindPoint;
                    ranges[0].RegisterSpace = binding.space;
                    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                    descriptorRanges.push_back(ranges);
                    param.DescriptorTable.NumDescriptorRanges = 1;
                    param.DescriptorTable.pDescriptorRanges = descriptorRanges.back().data();
                }
                break;

            case D3D_SIT_UAV_RWTYPED:
            case D3D_SIT_UAV_RWSTRUCTURED:
            case D3D_SIT_UAV_RWBYTEADDRESS:
                // UAVはディスクリプタテーブル
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                {
                    std::vector<D3D12_DESCRIPTOR_RANGE> ranges(1);
                    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                    ranges[0].NumDescriptors = binding.bindCount;
                    ranges[0].BaseShaderRegister = binding.bindPoint;
                    ranges[0].RegisterSpace = binding.space;
                    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

                    descriptorRanges.push_back(ranges);
                    param.DescriptorTable.NumDescriptorRanges = 1;
                    param.DescriptorTable.pDescriptorRanges = descriptorRanges.back().data();
                }
                break;

            case D3D_SIT_SAMPLER:
                // サンプラーはスタティックサンプラーとして扱う（後で追加）
                continue;

            default:
                continue;
            }

            rootParams.push_back(param);
            parameterIndices_[binding.name] = rootIndex++;
        }

        return rootParams;
    }

    /// <summary>
    /// サンプラーの生成
    /// </summary>
    /// <returns></returns>
    std::vector<D3D12_STATIC_SAMPLER_DESC> AutoRootSignatureGenerator::CreateStaticSamplers() {
        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers;

        for (const auto& binding : mergedBindings_) {
            if (binding.type == D3D_SIT_SAMPLER) {
                D3D12_STATIC_SAMPLER_DESC sampler{};
                std::string name = binding.name;

                if(name == "gShadowSampler"){
                    sampler.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
                    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
                    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
                    sampler.MinLOD = 0.0f;
                    sampler.MaxLOD = D3D12_FLOAT32_MAX;
                    sampler.ShaderRegister = binding.bindPoint;
                    sampler.RegisterSpace = binding.space;
                    sampler.ShaderVisibility = binding.visibility;
                }
                else {
                    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
                    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                    sampler.MaxLOD = D3D12_FLOAT32_MAX;
                    sampler.ShaderRegister = binding.bindPoint;
                    sampler.RegisterSpace = binding.space;
                    sampler.ShaderVisibility = binding.visibility;
                }
                samplers.push_back(sampler);
            }
        }

        return samplers;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> AutoRootSignatureGenerator::Generate(
        ID3D12Device* device,
        D3D12_ROOT_SIGNATURE_FLAGS flags
    ) {
        auto rootParams = CreateRootParameters();
        auto samplers = CreateStaticSamplers();

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = static_cast<UINT>(rootParams.size());
        desc.pParameters = rootParams.empty() ? nullptr : rootParams.data();
        desc.NumStaticSamplers = static_cast<UINT>(samplers.size());
        desc.pStaticSamplers = samplers.empty() ? nullptr : samplers.data();
        desc.Flags = flags;

        Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
            &signatureBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                Logger(static_cast<char*>(errorBlob->GetBufferPointer()));
            }
            return nullptr;
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
        hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature));
        if (FAILED(hr)) {
            return nullptr;
        }

        return rootSignature;
    }

    void AutoRootSignatureGenerator::PrintDebugInfo() const {
        Logger("===== Auto-Generated Root Signature =====\n");
        Logger("Root Parameters (Ordered by Index):\n");

        // ソート用のテンポラリ配列を作成 (Index, Name)
        std::vector<std::pair<UINT, std::string>> sortedParams;
        for (const auto& [name, index] : parameterIndices_) {
            sortedParams.push_back({ index, name });
        }

        // インデックス（first）を基準に昇順ソート
        std::sort(sortedParams.begin(), sortedParams.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        // ソートされた順に表示
        for (const auto& [index, name] : sortedParams) {
            char buf[256];
            // インデックスは UINT なので %u を使うのがより正確です
            sprintf_s(buf, "  [%u] %s\n", index, name.c_str());
            Logger(buf);
        }

        if (!inputLayout_.empty()) {
            Logger("Input Layout:\n");
            for (const auto& element : inputLayout_) {
                char buf[256];
                sprintf_s(buf, "  %s%d: Format=%d\n",
                    element.SemanticName, element.SemanticIndex, (int)element.Format);
                Logger(buf);
            }
        }
    }

    // ========== ReflectionBasedPipelineBuilder 実装 ==========

    ReflectionBasedPipelineBuilder::ReflectionBasedPipelineBuilder()
    {
        blendState_ = BlendPresets::CreateAlphaBlend();
        rasterizerState_ = RasterizerPresets::CreateDefault();
        depthStencilState_ = DepthStencilPresets::CreateDefault();
        primitiveTopology_ = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        rtvFormat_ = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        dsvFormat_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
    }

    ReflectionBasedPipelineBuilder::BuildResult ReflectionBasedPipelineBuilder::BuildFromCompiledShaders(
        ID3D12Device* device,
        IDxcBlob* vertexShader,
        IDxcBlob* pixelShader,
        IDxcBlob* geometryShader
    ) {

        BuildResult result;

        // リフレクション情報を収集
        AutoRootSignatureGenerator generator;
        generator.AddShaders(vertexShader,pixelShader);

        // ルートシグネチャを生成
        result.rootSignature = generator.Generate(device);
        result.parameterIndices = generator.GetParameterIndices();
        result.inputLayout = generator.GetInputLayout();

		// デバッグ情報を出力
        generator.PrintDebugInfo();

        // パイプラインステートを生成
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = result.rootSignature.Get();

        if (vertexShader) {
            psoDesc.VS.pShaderBytecode = vertexShader->GetBufferPointer();
            psoDesc.VS.BytecodeLength = vertexShader->GetBufferSize();
        }

        if (pixelShader) {
            psoDesc.PS.pShaderBytecode = pixelShader->GetBufferPointer();
            psoDesc.PS.BytecodeLength = pixelShader->GetBufferSize();
        }

        if (geometryShader) {
            psoDesc.GS.pShaderBytecode = geometryShader->GetBufferPointer();
            psoDesc.GS.BytecodeLength = geometryShader->GetBufferSize();
        }

        psoDesc.InputLayout.pInputElementDescs = result.inputLayout.data();
        psoDesc.InputLayout.NumElements = static_cast<UINT>(result.inputLayout.size());
        psoDesc.BlendState = blendState_;
        psoDesc.RasterizerState = rasterizerState_;
        psoDesc.DepthStencilState = depthStencilState_;
        psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        psoDesc.PrimitiveTopologyType = primitiveTopology_;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = rtvFormat_;
        psoDesc.DSVFormat = dsvFormat_;
        psoDesc.SampleDesc.Count = 1;

        // G-buffer 法線（MRT 第2ターゲット）。
        // writeNormal=false の場合は RenderTarget[1] の writemask を 0 のままにして法線を汚さない。
        if (useGBufferNormal_) {
            psoDesc.NumRenderTargets = 2;
            psoDesc.RTVFormats[1] = DXGI_FORMAT_R16G16B16A16_FLOAT;
            psoDesc.BlendState.RenderTarget[1].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask =
                writeNormal_ ? D3D12_COLOR_WRITE_ENABLE_ALL : 0;
        }

        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc,
            IID_PPV_ARGS(&result.pipelineState));
        if (FAILED(hr)) {
            Logger("Failed to create pipeline state from reflection\n");
        }

        return result;
    }

    ReflectionBasedPipelineBuilder::BuildResult ReflectionBasedPipelineBuilder::BuildFromCompiledComputeShader(ID3D12Device* device, IDxcBlob* computeShader)
    {
        // 計算シェーダーのリフレクション
		AutoRootSignatureGenerator generator;
		generator.AddComputeShader(computeShader);

		// ルートシグネチャを生成
        BuildResult result;
		result.rootSignature = generator.Generate(device);
		result.parameterIndices = generator.GetParameterIndices();

		// パイプラインステートを生成
		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
		psoDesc.pRootSignature = result.rootSignature.Get();
		psoDesc.CS.pShaderBytecode = computeShader->GetBufferPointer();
		psoDesc.CS.BytecodeLength = computeShader->GetBufferSize();

        device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&result.pipelineState));
       
		return result;
    }

} // namespace YoRigine

DXGI_FORMAT GetFormat(const D3D12_SIGNATURE_PARAMETER_DESC& desc)
{
    UINT componentCount = 0;
    if (desc.Mask & 1) componentCount++;
    if (desc.Mask & 2) componentCount++;
    if (desc.Mask & 4) componentCount++;
    if (desc.Mask & 8) componentCount++;

    if (desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
    {
        switch (componentCount)
        {
        case 1: return DXGI_FORMAT_R32_FLOAT;
        case 2: return DXGI_FORMAT_R32G32_FLOAT;
        case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
        case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
    }
    else if (desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
    {
        switch (componentCount)
        {
        case 1: return DXGI_FORMAT_R32_UINT;
        case 2: return DXGI_FORMAT_R32G32_UINT;
        case 3: return DXGI_FORMAT_R32G32B32_UINT;
        case 4: return DXGI_FORMAT_R32G32B32A32_UINT;
        }
    }
    else if (desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
    {
        switch (componentCount)
        {
        case 1: return DXGI_FORMAT_R32_SINT;
        case 2: return DXGI_FORMAT_R32G32_SINT;
        case 3: return DXGI_FORMAT_R32G32B32_SINT;
        case 4: return DXGI_FORMAT_R32G32B32A32_SINT;
        }
    }

    assert(false);
    return DXGI_FORMAT_UNKNOWN;
}
