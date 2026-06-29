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

// ============================================================
// 初期化
// ============================================================
void YPipelineManager::Initialize()
{
    dxCommon_ = DirectXCommon::GetInstance();

    // PSOキャッシュの初期化
    psoCache_ = std::make_unique<PSOCache>();
    psoCache_->Initialize(dxCommon_->GetDevice().Get(), "Resources/Binary/YPipeline/");

	completePipelineCache_ = std::make_unique<CompletePipelineCache>();
	completePipelineCache_->Initialize(dxCommon_->GetDevice().Get(), "Resources/Binary/YPipeline/");

    // 基本パイプラインの生成
    CreatePSO_Sprite();
    CreatePSO_Object();
    CreatePSO_ShadowMap();
    CreatePSO_ObjectInstanced();
    CreatePSO_ShadowMapInstanced();
    CreatePSO_Line();
    CreatePSO_InstancedCube();
    CreatePSO_YParticle();
    CreatePSO_YParticleAllBlendModes();
    CreatePSO_GPUParticleALLBlendModes();
    CreatePSO_CubeMap();
    CreatePSO_GPUParticleInit();
    CreatePSO_EffectObject();

    // ポストエフェクト系PSパイプライン: 全エフェクトCS化に伴い、
    // 最終 blit 用の BaseOffScreen (CopyImage.PS.hlsl) のみ残す
    CreatePSO_BaseOffScreen();

	// Meshを使用したVFX用パイプライン
    CreatePSO_VfxMeshTrail();
    CreatePSO_VfxMeshVolume();
    CreatePSO_VfxMeshSmoke();
    CreatePSO_VfxMeshLightning();
    CreatePSO_VfxMeshShockwave();

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

// ============================================================
// 終了処理
// ============================================================
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


// ============================================================
// 
// Sprite
// 
// ============================================================
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

    // 入力（コンパイル済みVS/PSバイトコード）のハッシュを計算。
    // ディスクキャッシュはこの値で「変化なし＝書き換え不要」を判定する。
    uint64_t inputHash = YoRigine::CompletePipelineCache::HashData(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize());
    inputHash = YoRigine::CompletePipelineCache::HashData(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), inputHash);
    cacheData.inputHash = inputHash;

    // SemanticNameの寿命管理用にコピー
    for (const auto& element : result.inputLayout) {
        if (element.SemanticName) {
            cacheData.semanticNames.push_back(element.SemanticName);
        }
    }

    completePipelineCache_->Add(key, cacheData);
}

// ============================================================
// 
// BaseObject
// 
// ============================================================
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

// ============================================================
// 
// ShadowMap
// 
// ============================================================
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
        .SetRasterizerState(YoRigine::RasterizerPresets::CreateShadow())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get()
        );

    // 結果を保存
    rootSignatures_["ShadowMap"] = result.rootSignature;
    pipelineStates_["ShadowMap"] = result.pipelineState;
    parameterIndices_["ShadowMap"] = result.parameterIndices;
}

// ============================================================
// 
// Object : Instance
// 
// ============================================================
void YPipelineManager::CreatePSO_ObjectInstanced()
{
    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: ObjectInstanced              \n\n\n");
    Logger("==============================================================\n");
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3d/Object3dInstanced.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3d/Object3dInstanced.PS.hlsl", L"ps_6_0");

    ReflectionBasedPipelineBuilder builder;
    auto result = builder.BuildFromCompiledShaders(
        dxCommon_->GetDevice().Get(),
        vsBlob.Get(),
        psBlob.Get()
    );

    rootSignatures_["ObjectInstanced"] = result.rootSignature;
    pipelineStates_["ObjectInstanced"] = result.pipelineState;
    parameterIndices_["ObjectInstanced"] = result.parameterIndices;
}

// ============================================================
// 
// ShadowMap : Insctance
// 
// ============================================================
void YPipelineManager::CreatePSO_ShadowMapInstanced()
{
    Logger("\n==============================================================\n\n\n");
    Logger("         Creating Pipeline: ShadowMapInstanced              \n\n\n");
    Logger("==============================================================\n");
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Shadow/ShadowmapInstanced.VS.hlsl", L"vs_6_0");

    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT)
        .SetRasterizerState(YoRigine::RasterizerPresets::CreateShadow())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get()
        );

    rootSignatures_["ShadowMapInstanced"] = result.rootSignature;
    pipelineStates_["ShadowMapInstanced"] = result.pipelineState;
    parameterIndices_["ShadowMapInstanced"] = result.parameterIndices;
}

// ============================================================
// 
// GPUParticle : ALLBlendMode
// 
// ============================================================
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

// ============================================================
// 
// CPUParticle : ALLBlendMode
// 
// ============================================================
void YPipelineManager::CreatePSO_YParticleAllBlendModes()
{
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

    // 共通の VS を使い、PS だけ差し替えて「通常」「ソフトパーティクル」2系統の
    // 全ブレンドモード PSO を生成する。リフレクションで root sig / index も自動生成。
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/YParticle.VS.hlsl", L"vs_6_0");

    // isSoft=true のソフト版は「深度なしRT」で描くため、深度テスト無効＋DSVフォーマット UNKNOWN。
    // （null DSV をバインドするには PSO の深度フォーマットも UNKNOWN である必要がある）
    auto buildSet = [&](const std::string& logicalName, const std::wstring& psPath, bool isSoft) {
        auto psBlob = dxCommon_->CompileShader(psPath.c_str(), L"ps_6_0");
        for (const auto& config : configs) {
            ReflectionBasedPipelineBuilder builder;
            builder
                .SetBlendState(config.blendDesc)
                .SetRasterizerState(RasterizerPresets::CreateNoCull());
            if (isSoft) {
                builder.SetDepthStencilState(DepthStencilPresets::CreateDisabled())
                       .SetDepthStencilFormat(DXGI_FORMAT_UNKNOWN);
            } else {
                builder.SetDepthStencilState(DepthStencilPresets::CreateReadOnly());
            }
            auto result = builder.BuildFromCompiledShaders(
                dxCommon_->GetDevice().Get(),
                vsBlob.Get(),
                psBlob.Get()
            );

            pipelineStates_[logicalName + "_" + config.name] = result.pipelineState;
            blendModePipelineStates_[logicalName][config.mode] = result.pipelineState;

            // 最初のモードだけ root sig / index を代表として保存
            if (config.mode == BlendMode::kBlendModeNormal) {
                rootSignatures_[logicalName] = result.rootSignature;
                parameterIndices_[logicalName] = result.parameterIndices;
            }
        }
    };

    buildSet("YParticle",     L"Resources/Shaders/Particle/YParticle.PS.hlsl",     false);
    buildSet("YParticleSoft", L"Resources/Shaders/Particle/YParticleSoft.PS.hlsl", true);
}

// ============================================================
// 
// BaseObject
// 
// ============================================================
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

// ============================================================
// 
// CPUParticle
// 
// ============================================================
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

// ============================================================
// 
// GPUParticle
// 
// ============================================================
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

// ============================================================
// 
// Line
// 
// ============================================================
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

// ============================================================
// 
// InstancedCube
// 
// ============================================================
void YPipelineManager::CreatePSO_InstancedCube()
{
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Primitive/InstancedCube/InstancedCube.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Primitive/InstancedCube/InstancedCube.PS.hlsl", L"ps_6_0");

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

    rootSignatures_["InstancedCube"] = result.rootSignature;
    pipelineStates_["InstancedCube"] = result.pipelineState;
    parameterIndices_["InstancedCube"] = result.parameterIndices;
}

// ============================================================
// 
// CubeMap
// 
// ============================================================
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

// ============================================================
// 
// Effect用Object
// 
// ============================================================
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

// ============================================================
// 
// VFXMesh - Trail
// 
// ============================================================
void YPipelineManager::CreatePSO_VfxMeshTrail() {

    // シェーダーコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh_Trail.PS.hlsl", L"ps_6_0");

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

        std::string psoName = "VfxMeshTrail_" + config.name;
        pipelineStates_[psoName] = result.pipelineState;
        blendModePipelineStates_["VfxMeshTrail"][config.mode] = result.pipelineState;

        // 最初のモードだけルートシグネチャとインデックスを保存
        if (config.mode == BlendMode::kBlendModeNormal) {
            rootSignatures_["VfxMeshTrail"] = result.rootSignature;
            parameterIndices_["VfxMeshTrail"] = result.parameterIndices;
        }
    }

}

// ============================================================
// 
// VFXMesh - Volume
// 
// ============================================================
void YPipelineManager::CreatePSO_VfxMeshVolume() {
    // シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh_Volume.PS.hlsl", L"ps_6_0");

    // リフレクションベースで完全自動生成
    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
		.SetBlendState(BlendPresets::CreateAdditive())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["VfxMeshVolume"] = result.rootSignature;
    pipelineStates_["VfxMeshVolume"] = result.pipelineState;
    parameterIndices_["VfxMeshVolume"] = result.parameterIndices;
}
// ============================================================
// 
// VFXMesh - Smoke
// 
// ============================================================
void YPipelineManager::CreatePSO_VfxMeshSmoke() {
    // Omen 風ボリュームスモーク。半透明スモークなのでアルファブレンド。
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh_Smoke.PS.hlsl", L"ps_6_0");

    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .SetBlendState(BlendPresets::CreateAlphaBlend())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["VfxMeshSmoke"] = result.rootSignature;
    pipelineStates_["VfxMeshSmoke"] = result.pipelineState;
    parameterIndices_["VfxMeshSmoke"] = result.parameterIndices;
}

// ============================================================
// 
// VFXMesh - Lightning
// 
// ============================================================
void YPipelineManager::CreatePSO_VfxMeshLightning() {
    // プロシージャル稲妻。加算ブレンドで芯が光る。
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh_Lightning.PS.hlsl", L"ps_6_0");

    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .SetBlendState(BlendPresets::CreateAdditive())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["VfxMeshLightning"] = result.rootSignature;
    pipelineStates_["VfxMeshLightning"] = result.pipelineState;
    parameterIndices_["VfxMeshLightning"] = result.parameterIndices;
}

// ============================================================
// 
// VFXMesh - ShockWave
// 
// ============================================================
void YPipelineManager::CreatePSO_VfxMeshShockwave() {
    // 爆発の衝撃波リング。加算ブレンド。
    auto vsBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/Shaders/Vfx/VfxMesh/VfxMesh_Shockwave.PS.hlsl", L"ps_6_0");

    ReflectionBasedPipelineBuilder builder;
    auto result = builder
        .SetRasterizerState(RasterizerPresets::CreateNoCull())
        .SetDepthStencilState(DepthStencilPresets::CreateReadOnly())
        .SetBlendState(BlendPresets::CreateAdditive())
        .BuildFromCompiledShaders(
            dxCommon_->GetDevice().Get(),
            vsBlob.Get(),
            psBlob.Get()
        );

    rootSignatures_["VfxMeshShockwave"] = result.rootSignature;
    pipelineStates_["VfxMeshShockwave"] = result.pipelineState;
    parameterIndices_["VfxMeshShockwave"] = result.parameterIndices;
}


// ============================================================
// 
// PostEffect
// 
// ============================================================
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