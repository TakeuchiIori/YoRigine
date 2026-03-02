#include "YPipelineManager.h"
#include "DirectXCommon.h"

#include "Debugger/Logger.h"

using namespace YoRigine;

namespace {
    const std::wstring DEFAULT_VS_PATH = L"Resources/Shaders/PostEffect/FullScreen/FullScreen.VS.hlsl";
    const std::wstring DEFAULT_PS_PATH = L"Resources/Shaders/PostEffect/CopyImage/CopyImage.PS.hlsl";
}

YPipelineManager* YPipelineManager::GetInstance()
{
    static YPipelineManager instance;
    return &instance;
}

ID3D12RootSignature* YPipelineManager::GetRootSignature(const std::string& key)
{
    auto it = rootSignatures_.find(key);
    if (it != rootSignatures_.end()) {
        return it->second.Get();
    }
    return nullptr;
}

ID3D12PipelineState* YPipelineManager::GetPipeLineStateObject(const std::string& key)
{
    auto it = pipelineStates_.find(key);
    if (it != pipelineStates_.end()) {
        return it->second.Get();
    }
    return nullptr;
}

const RootParameterIndices& YPipelineManager::GetRootParameterIndices(const std::string& key) const
{
    auto it = rootParamIndices_.find(key);
    if (it != rootParamIndices_.end()) {
        return it->second;
    }
    throw std::runtime_error("Root parameter indices not found: " + key);
}

const std::unordered_map<std::string, UINT>& YPipelineManager::GetParameterIndices(const std::string& pipelineName) const
{
    auto it = parameterIndices_.find(pipelineName);
    if (it != parameterIndices_.end()) {
        return it->second;
    }

    // 空のマップを返す（静的変数）
    static std::unordered_map<std::string, UINT> empty;
    return empty;
}

ID3D12PipelineState* YPipelineManager::GetBlendModePSO(const std::string& key,BlendMode blendMode)
{
    auto it = blendModePipelineStates_[key].find(blendMode);
    if (it != blendModePipelineStates_[key].end()) {
        return it->second.Get();
    }
    return nullptr;
}

D3D12_BLEND_DESC YPipelineManager::GetBlendDescFromMode(BlendMode mode)
{
    switch (mode) {
    case BlendMode::kBlendModeNone:
        return BlendPresets::CreateNone();
    case BlendMode::kBlendModeNormal:
        return BlendPresets::CreateAlphaBlend();
    case BlendMode::kBlendModeAdd:
        return BlendPresets::CreateAdditive();
    case BlendMode::kBlendModeSubtract:
        return BlendPresets::CreateSubtractive();
    case BlendMode::kBlendModeMultiply:
        return BlendPresets::CreateMultiply();
    case BlendMode::kBlendModeScreen:
        return BlendPresets::CreateScreen();
    default:
        return BlendPresets::CreateAlphaBlend();
    }
}

void YPipelineManager::Initialize()
{
    dxCommon_ = DirectXCommon::GetInstance();

    // PSOキャッシュの初期化
    psoCache_ = std::make_unique<PSOCache>();
    psoCache_->Initialize(dxCommon_->GetDevice().Get(), "Resources/Binary/PSO/");

	completePipelineCache_ = std::make_unique<CompletePipelineCache>();
	completePipelineCache_->Initialize(dxCommon_->GetDevice().Get(), "Resources/Binary/Pipeline/");

    // 基本パイプラインの生成
    CreatePSO_Sprite();
    CreatePSO_Object();
    CreatePSO_ShadowMap();
    CreatePSO_Line();
    CreatePSO_Particle();
    CreatePSO_YParticle();
    CreatePSO_YParticleAllBlendModes();
    CreatePSO_ParticleAllBlendModes();
    CreatePSO_GPUParticleALLBlendModes();
    CreatePSO_CubeMap();
    CreatePSO_GPUParticleInit();
    CreatePSO_EffectObject();

    // ポストエフェクト系パイプライン
    CreatePSO_BaseOffScreen();
    CreatePSO_BaseOffScreen(
        L"Resources/Shaders/PostEffect/Grayscale/Grayscale.PS.hlsl",
        "Grayscale");
    CreatePSO_BaseOffScreen(
        L"Resources/Shaders/PostEffect/Sepia/Sepia.PS.hlsl",
        "Sepia");
    CreatePSO_BaseOffScreen(
        L"Resources/Shaders/PostEffect/Vignette/Vignette.PS.hlsl",
        "Vignette");
    CreatePSO_Smoothing(
        L"Resources/Shaders/PostEffect/Smoothing/BoxFilter.PS.hlsl",
        "OffScreen_BoxSmoothing");
    CreatePSO_Smoothing(
        L"Resources/Shaders/PostEffect/Smoothing/GaussianFilter.PS.hlsl",
        "GaussSmoothing");
    CreatePSO_DepthOutLine(
        L"Resources/Shaders/PostEffect/OutLine/DepthBasedOutLine.PS.hlsl",
        "DepthOutLine");
    CreatePSO_RadialBlur(
        L"Resources/Shaders/PostEffect/Blur/RadialBlur.PS.hlsl",
        "RadialBlur");
    CreatePSO_ToneMapping(
        L"Resources/Shaders/PostEffect/ColorRemapping/ToneMapping.PS.hlsl",
        "ToneMapping");
    CreatePSO_Dissolve(
        L"Resources/Shaders/PostEffect/Dissolve/Dissolve.PS.hlsl",
        "Dissolve");
    CreatePSO_Chromatic(
        L"Resources/Shaders/PostEffect/ColorRemapping/Chromatic.PS.hlsl",
        "Chromatic");
    CreatePSO_ColorAdjust(
        L"Resources/Shaders/PostEffect/ColorRemapping/ColorAdjust.PS.hlsl",
        "ColorAdjust");
    CreatePSO_ShatterTransition(
        L"Resources/Shaders/PostEffect/Transition/ShatterTransition.PS.hlsl",
        "ShatterTransition");

    // 統計情報を出力
    auto stats = psoCache_->GetStats();
    char buf[512];
    sprintf_s(buf,
        "PSO Cache Stats:\n"
        "  Total PSOs: %zu\n"
        "  Cache Hits: %zu\n"
        "  Cache Misses: %zu\n"
        "  Disk Cache Hits: %zu\n",
        stats.totalPSOs, stats.cacheHits, stats.cacheMisses, stats.diskCacheHits
    );
    Logger(buf);
}

void YPipelineManager::Finalize()
{
    // すべてのPSOをディスクに保存
    if (psoCache_) {
        psoCache_->SaveAll();
    }
    // すべてのキャッシュをディスクに保存
    if (completePipelineCache_) {
        completePipelineCache_->SaveAll();
    }

    // クリーンアップ
    pipelineStates_.clear();
    blendModePipelineStates_.clear();
    rootSignatures_.clear();
    rootParamIndices_.clear();
    parameterIndices_.clear();
}

// ===== パイプライン生成関数の実装 =====

void YPipelineManager::CreatePSO_Sprite()
{

    const std::string key = "Sprite";

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: Sprite (Alpha Blend)              \n\n\n");
    Logger("==============================================================\n");

    // ⭐ まずキャッシュをチェック
    auto* cached = completePipelineCache_->Get(key);
    if (cached) {
        Logger("[Sprite] ✅ Using cached pipeline data\n");

        rootSignatures_[key] = cached->rootSignature;
        pipelineStates_[key] = cached->pipelineState;
        parameterIndices_[key] = cached->parameterIndices;

        // キャッシュから復元できた場合は終了
        if (cached->rootSignature && cached->pipelineState) {
            Logger("[Sprite] ✅ Pipeline fully restored from cache\n\n");
            return;
        }

        // パラメータインデックスだけ復元できた場合は、PSO等を再作成
        Logger("[Sprite] ⚠️ Partial cache hit, rebuilding PSO...\n");
    }

    // ⭐ キャッシュミス or 部分的なキャッシュヒット → 新規作成
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Sprite/Sprite.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Sprite/Sprite.PS.hlsl", L"ps_6_0");

    YoRigine::ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(YoRigine::BlendPresets::CreateAlphaBlend())
        .SetRasterizerState(YoRigine::RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(YoRigine::DepthStencilPresets::CreateReadOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;

    // ⭐ 完全版キャッシュに追加（自動的にディスクにも保存される）
    YoRigine::CompletePipelineCache::PipelineData cacheData;
    cacheData.rootSignature = result.rootSignature;
    cacheData.pipelineState = result.pipelineState;
    cacheData.parameterIndices = result.parameterIndices;
    cacheData.inputLayout = result.inputLayout;

    // SemanticNameの寿命管理用にコピー
    for (const auto& element : result.inputLayout) {
        if (element.SemanticName) {
            cacheData.semanticNames.push_back(element.SemanticName);
        }
    }

    completePipelineCache_->Add(key, cacheData);
}

void YPipelineManager::CreatePSO_Object()
{

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: Object              \n\n\n");
    Logger("==============================================================\n");
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3d/Object3D.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3d/Object3D.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .BuildFromCompiledShaders(
        dxCommon_->GetDevice().Get(),
        vsBlob.Get(),
        psBlob.Get()
    );

    // 結果を保存
    rootSignatures_["Object"] = result.rootSignature;
    pipelineStates_["Object"] = result.pipelineState;
    parameterIndices_["Object"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_ShadowMap()
{

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: ShadowMap             \n\n\n");
    Logger("==============================================================\n");
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Shadow/ShadowMap.VS.hlsl", L"vs_6_0");
    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
		.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT)
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get()
        );

    // 結果を保存
    rootSignatures_["ShadowMap"] = result.rootSignature;
    pipelineStates_["ShadowMap"] = result.pipelineState;
    parameterIndices_["ShadowMap"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_ObjectInstance()
{

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: ObjectInstance              \n\n\n");
    Logger("==============================================================\n");
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3DInstance.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3D.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder.BuildFromCompiledShaders(
        dxCommon_->GetDevice().Get(),
        vsBlob.Get(),
        psBlob.Get()
    );

    // 結果を保存
    rootSignatures_["ObjectInstance"] = result.rootSignature;
    pipelineStates_["ObjectInstance"] = result.pipelineState;
    parameterIndices_["ObjectInstance"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_Particle()
{

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: Particle            \n\n\n");
    Logger("==============================================================\n");
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/Particle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/Particle.PS.hlsl", L"ps_6_0");

    // 通常のアルファブレンド版
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateAlphaBlend())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["Particle"] = result.rootSignature;
    pipelineStates_["Particle"] = result.pipelineState;
    parameterIndices_["Particle"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_ParticleAllBlendModes()
{
    // シェーダーコンパイル（1回だけ）
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/Particle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/Particle.PS.hlsl", L"ps_6_0");

    // ブレンドモード設定
    struct BlendConfig {
        BlendMode mode;
        std::string name;
        D3D12_BLEND_DESC blendDesc;
    };

    BlendConfig configs[] = {
        { BlendMode::kBlendModeNone, "None", BlendPresets::CreateNone() },
        { BlendMode::kBlendModeNormal, "Normal", BlendPresets::CreateAlphaBlend() },
        { BlendMode::kBlendModeAdd, "Add", BlendPresets::CreateAdditive() },
        { BlendMode::kBlendModeSubtract, "Subtract", BlendPresets::CreateSubtractive() },
        { BlendMode::kBlendModeMultiply, "Multiply", BlendPresets::CreateMultiply() },
        { BlendMode::kBlendModeScreen, "Screen", BlendPresets::CreateScreen() },
    };

    // 各ブレンドモードごとにPSOを生成
    for (const auto& config : configs) {
        ReflectionBasedPipelineBuilder builder;
        auto result = builder
            .SetBlendState(config.blendDesc)
            .SetRasterizerState(RasterizerPresets::CreateNoCull())
            .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
            .BuildFromCompiledShaders(
                dxCommon_->GetDevice().Get(),
                vsBlob.Get(),
                psBlob.Get()
            );

        std::string psoName = "Particle_" + config.name;
        pipelineStates_[psoName] = result.pipelineState;
        blendModePipelineStates_["Particle"][config.mode] = result.pipelineState;

        // 最初のモードだけルートシグネチャとインデックスを保存
        if (config.mode == BlendMode::kBlendModeNormal) {
            rootSignatures_["Particle"] = result.rootSignature;
            parameterIndices_["Particle"] = result.parameterIndices;
        }
    }
}

void YPipelineManager::CreatePSO_GPUParticleALLBlendModes()
{

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: GPUParticleInit             \n\n\n");
    Logger("==============================================================\n");
    // シェーダーコンパイル（1回だけ）
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/GPUParticle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/GPUParticle.PS.hlsl", L"ps_6_0");

    // ブレンドモード設定
    struct BlendConfig {
        BlendMode mode;
        std::string name;
        D3D12_BLEND_DESC blendDesc;
    };

    BlendConfig configs[] = {
        { BlendMode::kBlendModeNone, "None", BlendPresets::CreateNone() },
        { BlendMode::kBlendModeNormal, "Normal", BlendPresets::CreateAlphaBlend() },
        { BlendMode::kBlendModeAdd, "Add", BlendPresets::CreateAdditive() },
        { BlendMode::kBlendModeSubtract, "Subtract", BlendPresets::CreateSubtractive() },
        { BlendMode::kBlendModeMultiply, "Multiply", BlendPresets::CreateMultiply() },
        { BlendMode::kBlendModeScreen, "Screen", BlendPresets::CreateScreen() },
    };

    // 各ブレンドモードごとにPSOを生成
    for (const auto& config : configs) {
        ReflectionBasedPipelineBuilder builder;
        auto result = builder
            .SetBlendState(config.blendDesc)
            .SetRasterizerState(RasterizerPresets::CreateNoCull())
            .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
            .BuildFromCompiledShaders(
                dxCommon_->GetDevice().Get(),
                vsBlob.Get(),
                psBlob.Get()
            );

        std::string psoName = "GPUParticleInit_" + config.name;
        pipelineStates_[psoName] = result.pipelineState;
        blendModePipelineStates_["GPUParticleInit"][config.mode] = result.pipelineState;

        // 最初のモードだけルートシグネチャとインデックスを保存
        if (config.mode == BlendMode::kBlendModeNormal) {
            rootSignatures_["GPUParticleInit"] = result.rootSignature;
            parameterIndices_["GPUParticleInit"] = result.parameterIndices;
        }
    }
}

void YPipelineManager::CreatePSO_YParticleAllBlendModes()
{   
    // シェーダーコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/YParticle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/YParticle.PS.hlsl", L"ps_6_0");

    // ブレンドモード設定
    struct BlendConfig {
        BlendMode mode;
        std::string name;
        D3D12_BLEND_DESC blendDesc;
    };

    BlendConfig configs[] = {
        { BlendMode::kBlendModeNone, "None", BlendPresets::CreateNone() },
        { BlendMode::kBlendModeNormal, "Normal", BlendPresets::CreateAlphaBlend() },
        { BlendMode::kBlendModeAdd, "Add", BlendPresets::CreateAdditive() },
        { BlendMode::kBlendModeSubtract, "Subtract", BlendPresets::CreateSubtractive() },
        { BlendMode::kBlendModeMultiply, "Multiply", BlendPresets::CreateMultiply() },
        { BlendMode::kBlendModeScreen, "Screen", BlendPresets::CreateScreen() },
    };

    // 各ブレンドモードごとにPSOを生成
    for (const auto& config : configs) {
        ReflectionBasedPipelineBuilder builder;
        auto result = builder
            .SetBlendState(config.blendDesc)
            .SetRasterizerState(RasterizerPresets::CreateNoCull())
            .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
            .BuildFromCompiledShaders(
                dxCommon_->GetDevice().Get(),
                vsBlob.Get(),
                psBlob.Get()
            );

        std::string psoName = "YParticle_" + config.name;
        pipelineStates_[psoName] = result.pipelineState;
        blendModePipelineStates_["YParticle"][config.mode] = result.pipelineState;

        // 最初のモードだけルートシグネチャとインデックスを保存
        if (config.mode == BlendMode::kBlendModeNormal) {
            rootSignatures_["YParticle"] = result.rootSignature;
            parameterIndices_["YParticle"] = result.parameterIndices;
        }
    }
}

void YPipelineManager::CreatePSO_Object_Manual()
{
    // ルートシグネチャをビルダーで作成
    RootSignatureBuilder rsBuilder;

    rsBuilder.AddCBV("Material", 0, D3D12_SHADER_VISIBILITY_PIXEL);
    rsBuilder.AddCBV("TransformMatrix", 0, D3D12_SHADER_VISIBILITY_VERTEX);
    rsBuilder.AddCBV("DirectionalLight", 1, D3D12_SHADER_VISIBILITY_PIXEL);
    rsBuilder.AddDescriptorTable("Texture", 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    rsBuilder.AddStaticSampler(0);

    auto rootSignature = rsBuilder.Build(dxCommon_->GetDevice().Get());
    rootSignatures_["Object"] = rootSignature;

    // インデックスマップを保存
    RootParameterIndices indices;
    indices.InitializeFrom(rsBuilder);
    rootParamIndices_["Object"] = indices;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3D.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3D.PS.hlsl", L"ps_6_0");

    // インプットレイアウトを定義
    D3D12_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // パイプラインステートをビルダーで作成
    GraphicsPipelineBuilder pipelineBuilder;
    auto psoDesc = pipelineBuilder
        .SetRootSignature(rootSignature.Get())
        .SetVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize())
        .SetPixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize())
        .SetInputLayout(inputElements, _countof(inputElements))
        .SetBlendState(BlendPresets::CreateAlphaBlend())
        .SetRasterizerState(RasterizerPresets::CreateDefault())
        .SetDepthStencilState(DepthStencilPresets::CreateDefault())
        .Build();

    // PSOキャッシュを使って作成
    auto pso = psoCache_->GetOrCreate("Object", psoDesc);
    pipelineStates_["Object"] = pso;
}

void YPipelineManager::CreatePSO_YParticle()
{

    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: YParticle              \n\n\n");
    Logger("==============================================================\n");
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/YParticle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/YParticle.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateAdditive())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["YParticle"] = result.rootSignature;
    pipelineStates_["YParticle"] = result.pipelineState;
    parameterIndices_["YParticle"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_GPUParticleInit()
{
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/GPUParticle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/GPUParticle.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateAlphaBlend())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["GPUParticleInit"] = result.rootSignature;
    pipelineStates_["GPUParticleInit"] = result.pipelineState;
    parameterIndices_["GPUParticleInit"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_Line()
{
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Primitive/Line/Line.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Primitive/Line/Line.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetPrimitiveTopologyType(PrimitiveTopologyPresets::Line())
        .SetBlendState(BlendPresets::CreateAlphaBlend())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateWriteOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["Line"] = result.rootSignature;
    pipelineStates_["Line"] = result.pipelineState;
    parameterIndices_["Line"] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_CubeMap()
{
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/CubeMap/CubeMap.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/CubeMap/CubeMap.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["CubeMap"] = result.rootSignature;
    pipelineStates_["CubeMap"] = result.pipelineState;
    parameterIndices_["CubeMap"] = result.parameterIndices;
}

/// <summary>
/// エフェクト用オブジェクト描画パイプライン
/// </summary>
void YPipelineManager::CreatePSO_EffectObject()
{
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Effect/Effect.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Effect/Effect.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["EffectObject"] = result.rootSignature;
    pipelineStates_["EffectObject"] = result.pipelineState;
    parameterIndices_["EffectObject"] = result.parameterIndices;
}


// ===== ポストエフェクト系パイプライン =====

void YPipelineManager::CreatePSO_BaseOffScreen(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ? DEFAULT_PS_PATH : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "BaseOffScreen" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_Smoothing(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/Smoothing/GaussianFilter.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "Smoothing" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_DepthOutLine(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/OutLine/DepthBasedOutLine.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "DepthOutLine" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_RadialBlur(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/Blur/RadialBlur.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "RadialBlur" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_ToneMapping(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/ColorRemapping/ToneMapping.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "ToneMapping" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_Dissolve(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/Dissolve/Dissolve.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "Dissolve" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_Chromatic(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/ColorRemapping/Chromatic.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "Chromatic" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_ColorAdjust(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/ColorRemapping/ColorAdjust.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "ColorAdjust" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}

void YPipelineManager::CreatePSO_ShatterTransition(
    const std::wstring& pixelShaderPath,
    const std::string& pipelineKey
)
{
    std::wstring vsPath = DEFAULT_VS_PATH;
    std::wstring psPath = pixelShaderPath.empty() ?
        L"Resources/Shaders/PostEffect/Transition/ShatterTransition.PS.hlsl" : pixelShaderPath;
    std::string key = pipelineKey.empty() ? "ShatterTransition" : pipelineKey;

    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(vsPath, L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(psPath, L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetBlendState(BlendPresets::CreateNone())
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateDisabled())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_[key] = result.rootSignature;
    pipelineStates_[key] = result.pipelineState;
    parameterIndices_[key] = result.parameterIndices;
}