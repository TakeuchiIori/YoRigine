#include "RootSignatureBuilder.h"
#include <cassert>
#include <stdexcept>
#include "Debugger/Logger.h"

namespace YoRigine {

    RootSignatureBuilder::RootSignatureBuilder() {
        Reset();
    }

    void RootSignatureBuilder::Reset() {
        rootParameters_.clear();
        parameterInfos_.clear();
        staticSamplers_.clear();
        descriptorRanges_.clear();
        currentRootIndex_ = 0;
    }

    UINT RootSignatureBuilder::AddCBV(
        const std::string& name,
        UINT shaderRegister,
        D3D12_SHADER_VISIBILITY visibility,
        UINT registerSpace
    ) {
        D3D12_ROOT_PARAMETER param{};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param.ShaderVisibility = visibility;
        param.Descriptor.ShaderRegister = shaderRegister;
        param.Descriptor.RegisterSpace = registerSpace;

        rootParameters_.push_back(param);

        ParameterInfo info{};
        info.name = name;
        info.type = ParamType::CBV;
        info.rootIndex = currentRootIndex_;
        info.visibility = visibility;
        info.shaderRegister = shaderRegister;
        info.registerSpace = registerSpace;
        parameterInfos_.push_back(info);

        return currentRootIndex_++;
    }

    UINT RootSignatureBuilder::AddDescriptorTable(
        const std::string& name,
        UINT baseRegister,
        UINT numDescriptors,
        D3D12_SHADER_VISIBILITY visibility,
        UINT registerSpace
    ) {
        // ディスクリプタレンジを作成
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges(1);
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[0].NumDescriptors = numDescriptors;
        ranges[0].BaseShaderRegister = baseRegister;
        ranges[0].RegisterSpace = registerSpace;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // 寿命管理のため保存
        descriptorRanges_.push_back(ranges);

        D3D12_ROOT_PARAMETER param{};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.ShaderVisibility = visibility;
        param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
        param.DescriptorTable.pDescriptorRanges = descriptorRanges_.back().data();

        rootParameters_.push_back(param);

        ParameterInfo info{};
        info.name = name;
        info.type = ParamType::SRV_TABLE;
        info.rootIndex = currentRootIndex_;
        info.visibility = visibility;
        info.shaderRegister = baseRegister;
        info.registerSpace = registerSpace;
        parameterInfos_.push_back(info);

        return currentRootIndex_++;
    }

    UINT RootSignatureBuilder::AddUAVTable(
        const std::string& name,
        UINT baseRegister,
        UINT numDescriptors,
        D3D12_SHADER_VISIBILITY visibility,
        UINT registerSpace
    ) {
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges(1);
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        ranges[0].NumDescriptors = numDescriptors;
        ranges[0].BaseShaderRegister = baseRegister;
        ranges[0].RegisterSpace = registerSpace;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        descriptorRanges_.push_back(ranges);

        D3D12_ROOT_PARAMETER param{};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        param.ShaderVisibility = visibility;
        param.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
        param.DescriptorTable.pDescriptorRanges = descriptorRanges_.back().data();

        rootParameters_.push_back(param);

        ParameterInfo info{};
        info.name = name;
        info.type = ParamType::UAV_TABLE;
        info.rootIndex = currentRootIndex_;
        info.visibility = visibility;
        info.shaderRegister = baseRegister;
        info.registerSpace = registerSpace;
        parameterInfos_.push_back(info);

        return currentRootIndex_++;
    }

    UINT RootSignatureBuilder::Add32BitConstants(
        const std::string& name,
        UINT num32BitValues,
        UINT shaderRegister,
        D3D12_SHADER_VISIBILITY visibility,
        UINT registerSpace
    ) {
        D3D12_ROOT_PARAMETER param{};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        param.ShaderVisibility = visibility;
        param.Constants.ShaderRegister = shaderRegister;
        param.Constants.RegisterSpace = registerSpace;
        param.Constants.Num32BitValues = num32BitValues;

        rootParameters_.push_back(param);

        ParameterInfo info{};
        info.name = name;
        info.type = ParamType::CONSTANT_32BIT;
        info.rootIndex = currentRootIndex_;
        info.visibility = visibility;
        info.shaderRegister = shaderRegister;
        info.registerSpace = registerSpace;
        parameterInfos_.push_back(info);

        return currentRootIndex_++;
    }

    void RootSignatureBuilder::AddStaticSampler(
        UINT shaderRegister,
        D3D12_FILTER filter,
        D3D12_TEXTURE_ADDRESS_MODE addressMode,
        D3D12_SHADER_VISIBILITY visibility
    ) {
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = filter;
        sampler.AddressU = addressMode;
        sampler.AddressV = addressMode;
        sampler.AddressW = addressMode;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = shaderRegister;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = visibility;

        staticSamplers_.push_back(sampler);
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> RootSignatureBuilder::Build(
        ID3D12Device* device,
        D3D12_ROOT_SIGNATURE_FLAGS flags
    ) {
        assert(device != nullptr);

        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = static_cast<UINT>(rootParameters_.size());
        desc.pParameters = rootParameters_.empty() ? nullptr : rootParameters_.data();
        desc.NumStaticSamplers = static_cast<UINT>(staticSamplers_.size());
        desc.pStaticSamplers = staticSamplers_.empty() ? nullptr : staticSamplers_.data();
        desc.Flags = flags;

        Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

        HRESULT hr = D3D12SerializeRootSignature(
            &desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signatureBlob,
            &errorBlob
        );

        if (FAILED(hr)) {
            if (errorBlob) {
                Logger(static_cast<char*>(errorBlob->GetBufferPointer()));
            }
            throw std::runtime_error("Failed to serialize root signature");
        }

        Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
        hr = device->CreateRootSignature(
            0,
            signatureBlob->GetBufferPointer(),
            signatureBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)
        );

        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create root signature");
        }

        return rootSignature;
    }

    UINT RootSignatureBuilder::GetIndex(const std::string& name) const {
        for (const auto& info : parameterInfos_) {
            if (info.name == name) {
                return info.rootIndex;
            }
        }
        throw std::runtime_error("Root parameter not found: " + name);
    }

    /*=================================================================================================================
    
                                                    RootParameterIndices の実装

    //===============================================================================================================*/
    void RootParameterIndices::InitializeFrom(const RootSignatureBuilder& builder) {
        indices_.clear();
        for (const auto& info : builder.GetParameterInfos()) {
            indices_[info.name] = info.rootIndex;
        }
    }

    UINT RootParameterIndices::operator[](const std::string& name) const {
        auto it = indices_.find(name);
        if (it == indices_.end()) {
            throw std::runtime_error("Root parameter not found: " + name);
        }
        return it->second;
    }

    bool RootParameterIndices::HasParameter(const std::string& name) const {
        return indices_.find(name) != indices_.end();
    }

} // namespace YoRigine