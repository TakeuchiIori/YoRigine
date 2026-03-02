#include "OffScreen.h"
#include "DirectXCommon.h"
#include "PipelineManager/YPipelineManager.h"
#include "Loaders/Texture/TextureManager.h"

/// <summary>
/// オフスクリーンエフェクト共通初期化
/// </summary>
void OffScreen::Initialize()
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();

	auto pipelineManager = YPipelineManager::GetInstance();

	// 各エフェクトに対応する PSO / RootSignature を登録
	auto RegisterPipeline = [&](OffScreenEffectType type, const std::string& name) {
		OffScreenPipeline p;
		p.rootSignature = pipelineManager->GetRootSignature(name);
		p.pipelineState = pipelineManager->GetPipeLineStateObject(name);
		pipelineMap_[type] = p;
		};

	RegisterPipeline(OffScreenEffectType::Copy, "BaseOffScreen");
	RegisterPipeline(OffScreenEffectType::GaussSmoothing, "GaussSmoothing");
	RegisterPipeline(OffScreenEffectType::DepthOutline, "DepthOutLine");
	RegisterPipeline(OffScreenEffectType::Sepia, "Sepia");
	RegisterPipeline(OffScreenEffectType::Grayscale, "Grayscale");
	RegisterPipeline(OffScreenEffectType::Vignette, "Vignette");
	RegisterPipeline(OffScreenEffectType::RadialBlur, "RadialBlur");
	RegisterPipeline(OffScreenEffectType::ToneMapping, "ToneMapping");
	RegisterPipeline(OffScreenEffectType::Dissolve, "Dissolve");
	RegisterPipeline(OffScreenEffectType::Chromatic, "Chromatic");
	RegisterPipeline(OffScreenEffectType::ColorAdjust, "ColorAdjust");
	RegisterPipeline(OffScreenEffectType::ShatterTransition, "ShatterTransition");

	// マスク・破片テクスチャの読み込み
	TextureManager::GetInstance()->LoadTexture(maskTexturePath_);
	TextureManager::GetInstance()->LoadTexture(shatterTexturePath_);

	CreateAllResources();
}

/// <summary>
/// 指定エフェクトを実行し、フルスクリーン三角形を描画
/// </summary>
void OffScreen::RenderEffect(OffScreenEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	// パイプラインセット
	SetupPipelineAndDraw(type);

	// エフェクトごとの GPU パラメータ設定
	switch (type) {
	case OffScreenEffectType::Copy:              ExecuteCopyEffect(inputSRV); break;
	case OffScreenEffectType::GaussSmoothing:    ExecuteGaussSmoothingEffect(inputSRV); break;
	case OffScreenEffectType::DepthOutline:      ExecuteDepthOutlineEffect(inputSRV); break;
	case OffScreenEffectType::Sepia:             ExecuteSepiaEffect(inputSRV); break;
	case OffScreenEffectType::Grayscale:         ExecuteGrayscaleEffect(inputSRV); break;
	case OffScreenEffectType::Vignette:          ExecuteVignetteEffect(inputSRV); break;
	case OffScreenEffectType::RadialBlur:        ExecuteRadialBlurEffect(inputSRV); break;
	case OffScreenEffectType::ToneMapping:       ExecuteToneMappingEffect(inputSRV); break;
	case OffScreenEffectType::Dissolve:          ExecuteDissolveEffect(inputSRV); break;
	case OffScreenEffectType::Chromatic:         ExecuteChromaticEffect(inputSRV); break;
	case OffScreenEffectType::ColorAdjust:       ExecuteColorAdjustEffect(inputSRV); break;
	case OffScreenEffectType::ShatterTransition: ExecuteShatterTransitionEffect(inputSRV); break;
	}

	// フルスクリーン三角形描画
	dxCommon_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

/// <summary>
/// 全リソース解放
/// </summary>
void OffScreen::ReleaseResources()
{
	boxResource_.Reset();
	gaussResource_.Reset();
	materialResource_.Reset();
	radialBlurResource_.Reset();
	toneMappingResource_.Reset();
	dissolveResource_.Reset();
	chromaticResource_.Reset();
	colorAdjustResource_.Reset();
	toneParamsResource_.Reset();
	shatterTransitionResource_.Reset();

	boxData_ = nullptr;
	gaussData_ = nullptr;
	materialData_ = nullptr;
	radialBlurData_ = nullptr;
	toneMappingData_ = nullptr;
	dissolveData_ = nullptr;
	chromaticData_ = nullptr;
	colorAdjustData_ = nullptr;
	toneParamsData_ = nullptr;
	shatterTransitionData_ = nullptr;
}

// =======================
// パラメータ設定系
// =======================

/// <summary>トーンマッピングの露光量設定</summary>
void OffScreen::SetToneMappingExposure(float exposure)
{
	if (toneMappingData_) {
		toneMappingData_->exposure = exposure;
	}
}

/// <summary>ガウスブラーの強さ・カーネル設定</summary>
void OffScreen::SetGaussianBlurParams(float sigma, int kernelSize)
{
	if (gaussData_) {
		gaussData_->sigma = sigma;
		gaussData_->kernelSize = kernelSize;
	}
}

/// <summary>深度アウトラインの描画設定</summary>
void OffScreen::SetDepthOutlineParams(int kernelSize, const Vector4& color)
{
	if (materialData_) {
		materialData_->kernelSize = kernelSize;
		materialData_->outlineColor = color;
	}
}

/// <summary>ラジアルブラー設定</summary>
void OffScreen::SetRadialBlurParams(const RadialBlurPrams& params)
{
	if (radialBlurData_) {
		radialBlurData_->direction = params.direction;
		radialBlurData_->center = params.center;
		radialBlurData_->width = params.width;
		radialBlurData_->sampleCount = params.sampleCount;
		radialBlurData_->isRadial = params.isRadial;
	}
}

/// <summary>ディゾルブ設定</summary>
void OffScreen::SetDissolveParams(const DissolveParams& params)
{
	if (dissolveData_) {
		dissolveData_->threshold = params.threshold;
		dissolveData_->edgeWidth = params.edgeWidth;
		dissolveData_->edgeColor = params.edgeColor;
		dissolveData_->invert = params.invert;
	}
}

/// <summary>色収差エフェクト設定</summary>
void OffScreen::SetChromaticParams(const ChromaticParams& params)
{
	if (chromaticData_) {
		chromaticData_->aberrationStrength = params.aberrationStrength;
		chromaticData_->screenSize = { WinApp::kClientWidth, WinApp::kClientHeight };
		chromaticData_->edgeStrength = params.edgeStrength;
	}
}

/// <summary>カラー調整＋トーンマッピング同時設定</summary>
void OffScreen::SetColorAdjustParams(const ColorAdjustParams& colorParams, const ToneParams& toneParams)
{
	if (colorAdjustData_) {
		colorAdjustData_->brightness = colorParams.brightness;
		colorAdjustData_->contrast = colorParams.contrast;
		colorAdjustData_->saturation = colorParams.saturation;
		colorAdjustData_->hue = colorParams.hue;

		if (toneParamsData_) {
			toneParamsData_->gamma = toneParams.gamma;
			toneParamsData_->exposure = toneParams.exposure;
		}
	}
}

/// <summary>シャッター（画面割れ）エフェクト設定</summary>
void OffScreen::SetShatterTransitionParams(const ShatterTransitionParams& params)
{
	if (shatterTransitionData_) {
		shatterTransitionData_->progress = params.progress;
		shatterTransitionData_->resolution = { WinApp::kClientWidth, WinApp::kClientHeight };
		shatterTransitionData_->time = params.time;
	}
}

// ===========================
// ブラーアニメーション制御
// ===========================

/// <summary>
/// ラジアルブラーの「時間減衰」付きモーション更新
/// </summary>
void OffScreen::UpdateBlur(float deltaTime)
{
	if (isBlurMotion_) {
		blurTime_ += deltaTime;

		float t = std::clamp(blurTime_ / blurDuration_, 0.0f, 1.0f);
		float easeT = 1.0f - t; // 徐々に0へ

		if (radialBlurData_) {
			radialBlurData_->width = initialWidth_ * easeT;
			radialBlurData_->sampleCount =
				(std::max)(1, static_cast<int>(initialSampleCount_ * easeT));
		}

		if (t >= 1.0f) {
			isBlurMotion_ = false;
		}
	}
}

/// <summary>
/// ブラー開始
/// </summary>
void OffScreen::StartBlurMotion(const RadialBlurPrams& params)
{
	radialBlurPrams_ = params;

	blurDuration_ = 1.0f;
	blurTime_ = 0.0f;
	isBlurMotion_ = true;

	initialWidth_ = params.width;
	initialSampleCount_ = params.sampleCount;

	SetRadialBlurParams(params);
}

// ============================
// エフェクト用 GPU リソース生成
// ============================

/// <summary>
/// 全エフェクトのバッファ生成
/// </summary>
void OffScreen::CreateAllResources()
{
	CreateBoxFilterResource();
	CreateGaussFilterResource();
	CreateDepthOutLineResource();
	CreateRadialBlurResource();
	CreateToneMappingResource();
	CreateDissolveResource();
	CreateChromaticResource();
	CreateColorAdjustResource();
	CreateShatterTransitionResource();
}

/// <summary>ボックスフィルタ用バッファ</summary>
void OffScreen::CreateBoxFilterResource()
{
	boxResource_ = dxCommon_->CreateBufferResource(sizeof(KernelForGPU));
	boxResource_->Map(0, nullptr, reinterpret_cast<void**>(&boxData_));
	boxData_->kernelSize = 5;
	boxResource_->Unmap(0, nullptr);
}

/// <summary>ガウスフィルタ用バッファ</summary>
void OffScreen::CreateGaussFilterResource()
{
	gaussResource_ = dxCommon_->CreateBufferResource(sizeof(GaussKernelForGPU));
	gaussResource_->Map(0, nullptr, reinterpret_cast<void**>(&gaussData_));
	gaussData_->kernelSize = 3;
	gaussData_->sigma = 2.0f;
	gaussResource_->Unmap(0, nullptr);
}

/// <summary>深度アウトライン用バッファ</summary>
void OffScreen::CreateDepthOutLineResource()
{
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->Inverse = MakeIdentity4x4();
	materialData_->kernelSize = 3;
	materialData_->outlineColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	materialResource_->Unmap(0, nullptr);
}

/// <summary>ラジアルブラー用バッファ</summary>
void OffScreen::CreateRadialBlurResource()
{
	radialBlurResource_ = dxCommon_->CreateBufferResource(sizeof(RadialBlurForGPU));
	radialBlurResource_->Map(0, nullptr, reinterpret_cast<void**>(&radialBlurData_));
	radialBlurData_->direction = { 0.0f, 0.0f };
	radialBlurData_->center = { 0.5f, 0.5f };
	radialBlurData_->width = 0.001f;
	radialBlurData_->sampleCount = 10;
	radialBlurData_->isRadial = true;
	radialBlurResource_->Unmap(0, nullptr);
}

/// <summary>トーンマッピング用バッファ</summary>
void OffScreen::CreateToneMappingResource()
{
	toneMappingResource_ = dxCommon_->CreateBufferResource(sizeof(ToneMappingForGPU));
	toneMappingResource_->Map(0, nullptr, reinterpret_cast<void**>(&toneMappingData_));
	toneMappingData_->exposure = 0.25f;
}

/// <summary>ディゾルブ用バッファ</summary>
void OffScreen::CreateDissolveResource()
{
	dissolveResource_ = dxCommon_->CreateBufferResource(sizeof(DissolveForGPU));
	dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
	dissolveData_->threshold = 0.5f;
	dissolveData_->edgeWidth = 0.01f;
	dissolveData_->edgeColor = { 1.0f, 1.0f, 1.0f };
	dissolveData_->invert = 0.0f;
}

/// <summary>色収差用バッファ</summary>
void OffScreen::CreateChromaticResource()
{
	chromaticResource_ = dxCommon_->CreateBufferResource(sizeof(ChromaticForGPU));
	chromaticResource_->Map(0, nullptr, reinterpret_cast<void**>(&chromaticData_));
	chromaticData_->aberrationStrength = 0;
	chromaticData_->screenSize = { WinApp::kClientWidth, WinApp::kClientHeight };
	chromaticData_->edgeStrength = 0;
}

/// <summary>カラー調整用バッファ</summary>
void OffScreen::CreateColorAdjustResource()
{
	colorAdjustResource_ = dxCommon_->CreateBufferResource(sizeof(ColorAdjustForGPU));
	colorAdjustResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorAdjustData_));
	colorAdjustData_->brightness = 0.0f;
	colorAdjustData_->contrast = 1.0f;
	colorAdjustData_->saturation = 1.0f;
	colorAdjustData_->hue = 0.0f;

	toneParamsResource_ = dxCommon_->CreateBufferResource(sizeof(ToneParamsForGPU));
	toneParamsResource_->Map(0, nullptr, reinterpret_cast<void**>(&toneParamsData_));
	toneParamsData_->exposure = 1.0f;
	toneParamsData_->gamma = 2.2f;
}

/// <summary>シャッター（画面割れ）エフェクト用バッファ</summary>
void OffScreen::CreateShatterTransitionResource()
{
	shatterTransitionResource_ = dxCommon_->CreateBufferResource(sizeof(ShatterTransitionForGPU));
	shatterTransitionResource_->Map(0, nullptr, reinterpret_cast<void**>(&shatterTransitionData_));
	shatterTransitionData_->progress = 0.0f;
	shatterTransitionData_->resolution = { WinApp::kClientWidth, WinApp::kClientHeight };
	shatterTransitionData_->time = 0.0f;
}

// ===============================
// 各エフェクトの GPU コマンド設定
// ===============================

/// <summary>
/// PSO・RS・トポロジのセットアップ  
/// DepthOutline だけは逆行列更新も行う
/// </summary>
void OffScreen::SetupPipelineAndDraw(OffScreenEffectType type)
{
	auto& pipeline = pipelineMap_[type];
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetPipelineState(pipeline.pipelineState.Get());
	commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	if (type == OffScreenEffectType::DepthOutline && materialData_) {
		materialData_->Inverse = Inverse(projectionInverse_);
	}
}

// ---- 各エフェクトのルートパラメータ設定 ----

void OffScreen::ExecuteCopyEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("BaseOffScreen");
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
}

void OffScreen::ExecuteGaussSmoothingEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("GaussSmoothing");
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootConstantBufferView(indices.at("KernelSettings"), gaussResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteDepthOutlineEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("DepthOutLine");
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootDescriptorTable(indices.at("gDepthTexture"), dxCommon_->GetDepthGPUHandle());
	cmd->SetGraphicsRootConstantBufferView(indices.at("gMaterial"), materialResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteSepiaEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Sepia");
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
}

void OffScreen::ExecuteGrayscaleEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Grayscale");
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
}

void OffScreen::ExecuteVignetteEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Vignette");
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
}

void OffScreen::ExecuteRadialBlurEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("RadialBlur");
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootConstantBufferView(indices.at("gBlurParams"), radialBlurResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteToneMappingEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("ToneMapping");
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootConstantBufferView(indices.at("ExposureBuffer"), toneMappingResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteDissolveEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Dissolve");
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootDescriptorTable(indices.at("gMaskTexture"), TextureManager::GetInstance()->GetsrvHandleGPU(maskTexturePath_));
	cmd->SetGraphicsRootConstantBufferView(indices.at("DissolveParams"), dissolveResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteChromaticEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("Chromatic");
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootConstantBufferView(indices.at("ChromaticParams"), chromaticResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteColorAdjustEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("ColorAdjust");
	auto cmd = dxCommon_->GetCommandList();
	cmd->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
	cmd->SetGraphicsRootConstantBufferView(indices.at("ColorAdjustParams"), colorAdjustResource_->GetGPUVirtualAddress());
	cmd->SetGraphicsRootConstantBufferView(indices.at("ToneParams"), toneParamsResource_->GetGPUVirtualAddress());
}

void OffScreen::ExecuteShatterTransitionEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto cmd = dxCommon_->GetCommandList();
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("ShatterTransition");
	cmd->SetGraphicsRootDescriptorTable(indices.at("sceneTex"), inputSRV);
	cmd->SetGraphicsRootDescriptorTable(indices.at("crackTex"), TextureManager::GetInstance()->GetsrvHandleGPU(shatterTexturePath_));
	cmd->SetGraphicsRootConstantBufferView(indices.at("cbPostEffect"), shatterTransitionResource_->GetGPUVirtualAddress());
}
