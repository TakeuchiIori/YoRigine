#include "OffScreen.h"
#include "DirectXCommon.h"
#include "PipelineManager/YPipelineManager.h"
#include "Loaders/Texture/TextureManager.h"
#include "ComputeShaderManager/ComputeShaderManager.h"

/// <summary>
/// オフスクリーンエフェクト共通初期化
/// </summary>
void OffScreen::Initialize()
{
	dxCommon_ = YoRigine::DirectXCommon::GetInstance();

	auto pipelineManager = YPipelineManager::GetInstance();

	// PS版が残っているのは Copy (バックバッファへの最終 blit 専用) のみ。
	// 他のエフェクトは全て Compute Shader 経路で処理される。
	{
		OffScreenPipeline p;
		p.rootSignature = pipelineManager->GetRootSignature("BaseOffScreen");
		p.pipelineState = pipelineManager->GetPipeLineStateObject("BaseOffScreen");
		pipelineMap_[OffScreenEffectType::Copy] = p;
	}

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
	// 現在 PS 経路は Copy (最終 blit) のみ。他のエフェクトは RenderEffectCompute を使うこと。
	assert(type == OffScreenEffectType::Copy);
	(void)type;

	SetupPipelineAndDraw(OffScreenEffectType::Copy);
	ExecuteCopyEffect(inputSRV);
	dxCommon_->GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

/// <summary>
/// 指定エフェクトがCompute Shader実装を持つかどうか
/// </summary>
bool OffScreen::HasComputeImplementation(OffScreenEffectType type) const
{
	// 全エフェクトに CS実装が揃ったので無条件 true
	(void)type;
	return true;
}

/// <summary>
/// Compute Shader経由でエフェクトを実行する。
/// 出力先はUAVなので、呼び出し側が UNORDERED_ACCESS 状態に遷移済みであること。
/// </summary>
void OffScreen::RenderEffectCompute(
	OffScreenEffectType type,
	D3D12_GPU_DESCRIPTOR_HANDLE inputSRV,
	D3D12_GPU_DESCRIPTOR_HANDLE outputUAV,
	uint32_t width,
	uint32_t height)
{
	auto cmd = dxCommon_->GetCommandList();
	auto cs = ComputeShaderManager::GetInstance();

	// 共通: PSOキーとRSキーをエフェクト毎に決定
	const char* psoKey = nullptr;
	const char* rsKey = nullptr;
	switch (type) {
	case OffScreenEffectType::Copy:               psoKey = "PostEffectCopyCS";        rsKey = "PostEffectRS_Simple"; break;
	case OffScreenEffectType::Sepia:              psoKey = "PostEffectSepiaCS";       rsKey = "PostEffectRS_Simple"; break;
	case OffScreenEffectType::Grayscale:          psoKey = "PostEffectGrayscaleCS";   rsKey = "PostEffectRS_Simple"; break;
	case OffScreenEffectType::Vignette:           psoKey = "PostEffectVignetteCS";    rsKey = "PostEffectRS_Simple"; break;
	case OffScreenEffectType::GaussSmoothing:     psoKey = "PostEffectGaussCS";       rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::RadialBlur:         psoKey = "PostEffectRadialBlurCS";  rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::ToneMapping:        psoKey = "PostEffectToneMapCS";     rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::Chromatic:          psoKey = "PostEffectChromaticCS";   rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::Posterize:          psoKey = "PostEffectPosterizeCS";   rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::Kuwahara:           psoKey = "PostEffectKuwaharaCS";    rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::Halftone:           psoKey = "PostEffectHalftoneCS";    rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::CrossHatch:         psoKey = "PostEffectCrossHatchCS";  rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::ColorGrade:         psoKey = "PostEffectColorGradeCS";  rsKey = "PostEffectRS_CB"; break;
	case OffScreenEffectType::ColorAdjust:        psoKey = "PostEffectColorAdjustCS"; rsKey = "PostEffectRS_CB2"; break;
	case OffScreenEffectType::DepthOutline:       psoKey = "PostEffectDepthOutlineCS"; rsKey = "PostEffectRS_DepthNormal"; break;
	case OffScreenEffectType::NormalVisualize:    psoKey = "PostEffectNormalVisualizeCS"; rsKey = "PostEffectRS_DepthNormal"; break;
	case OffScreenEffectType::Fog:                psoKey = "PostEffectFogCS";          rsKey = "PostEffectRS_Depth"; break;
	case OffScreenEffectType::GodRays:            psoKey = "PostEffectGodRaysCS";      rsKey = "PostEffectRS_Depth"; break;
	case OffScreenEffectType::Dissolve:           psoKey = "PostEffectDissolveCS";     rsKey = "PostEffectRS_Tex"; break;
	case OffScreenEffectType::ShatterTransition:  psoKey = "PostEffectShatterCS";      rsKey = "PostEffectRS_Tex"; break;
	default:
		assert(false && "Unknown effect type");
		return;
	}

	cmd->SetPipelineState(cs->GetComputePipelineState(psoKey));
	cmd->SetComputeRootSignature(cs->GetRootSignature(rsKey));

	// DepthOutline のみ projectionInverse 行列を毎フレーム更新 (SetupPipelineAndDraw 相当)
	if (type == OffScreenEffectType::DepthOutline && materialData_) {
		materialData_->Inverse = Inverse(projectionInverse_);
	}

	// RS毎にバインドが異なる: パラメータ番号は CreatePostEffectCS の RS 定義と一致させる
	switch (type) {
	// === RS_Simple: SRV(0), UAV(1) ===
	case OffScreenEffectType::Copy:
	case OffScreenEffectType::Sepia:
	case OffScreenEffectType::Grayscale:
	case OffScreenEffectType::Vignette:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		break;

	// === RS_CB: SRV(0), UAV(1), CBV(2) ===
	case OffScreenEffectType::GaussSmoothing:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, gaussResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::RadialBlur:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, radialBlurResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::ToneMapping:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, toneMappingResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::Chromatic:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, chromaticResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::Posterize:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, posterizeResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::Kuwahara:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, kuwaharaResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::Halftone:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, halftoneResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::CrossHatch:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, crossHatchResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::ColorGrade:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, colorGradeResource_->GetGPUVirtualAddress());
		break;

	// === RS_CB2: SRV(0), UAV(1), CBV(2), CBV(3) ===
	case OffScreenEffectType::ColorAdjust:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, outputUAV);
		cmd->SetComputeRootConstantBufferView(2, colorAdjustResource_->GetGPUVirtualAddress());
		cmd->SetComputeRootConstantBufferView(3, toneParamsResource_->GetGPUVirtualAddress());
		break;

	// === RS_DepthNormal: SRV(0)=color, SRV(1)=depth, SRV(2)=normal, UAV(3), CBV(4) ===
	case OffScreenEffectType::DepthOutline:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, dxCommon_->GetDepthGPUHandle());
		cmd->SetComputeRootDescriptorTable(2, dxCommon_->GetNormalGPUHandle());
		cmd->SetComputeRootDescriptorTable(3, outputUAV);
		cmd->SetComputeRootConstantBufferView(4, materialResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::NormalVisualize:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, dxCommon_->GetDepthGPUHandle());
		cmd->SetComputeRootDescriptorTable(2, dxCommon_->GetNormalGPUHandle());
		cmd->SetComputeRootDescriptorTable(3, outputUAV);
		// CBV は未使用だが RS が要求するためダミーで materialResource_ をバインド
		cmd->SetComputeRootConstantBufferView(4, materialResource_->GetGPUVirtualAddress());
		break;

	// === RS_Depth: SRV(0)=color, SRV(1)=depth, UAV(2), CBV(3) ===
	case OffScreenEffectType::Fog:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, dxCommon_->GetDepthGPUHandle());
		cmd->SetComputeRootDescriptorTable(2, outputUAV);
		cmd->SetComputeRootConstantBufferView(3, fogResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::GodRays:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, dxCommon_->GetDepthGPUHandle());
		cmd->SetComputeRootDescriptorTable(2, outputUAV);
		cmd->SetComputeRootConstantBufferView(3, godRaysResource_->GetGPUVirtualAddress());
		break;

	// === RS_Tex: SRV(0)=color, SRV(1)=secondary, UAV(2), CBV(3) ===
	case OffScreenEffectType::Dissolve:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, TextureManager::GetInstance()->GetsrvHandleGPU(maskTexturePath_));
		cmd->SetComputeRootDescriptorTable(2, outputUAV);
		cmd->SetComputeRootConstantBufferView(3, dissolveResource_->GetGPUVirtualAddress());
		break;
	case OffScreenEffectType::ShatterTransition:
		cmd->SetComputeRootDescriptorTable(0, inputSRV);
		cmd->SetComputeRootDescriptorTable(1, TextureManager::GetInstance()->GetsrvHandleGPU(shatterTexturePath_));
		cmd->SetComputeRootDescriptorTable(2, outputUAV);
		cmd->SetComputeRootConstantBufferView(3, shatterTransitionResource_->GetGPUVirtualAddress());
		break;

	default:
		assert(false);
		return;
	}

	// threadgroup size (8,8,1)
	const uint32_t groupsX = (width + 7) / 8;
	const uint32_t groupsY = (height + 7) / 8;
	cmd->Dispatch(groupsX, groupsY, 1);
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
	bloomBrightCB_.Reset();
	bloomDownCB_.Reset();
	bloomUpCB_.Reset();
	bloomCompositeCB_.Reset();
	posterizeResource_.Reset();
	kuwaharaResource_.Reset();
	halftoneResource_.Reset();
	crossHatchResource_.Reset();
	colorGradeResource_.Reset();
	fogResource_.Reset();
	godRaysResource_.Reset();

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
	bloomBrightData_ = nullptr;
	bloomDownData_ = nullptr;
	bloomUpData_ = nullptr;
	bloomCompositeData_ = nullptr;
	posterizeData_ = nullptr;
	kuwaharaData_ = nullptr;
	halftoneData_ = nullptr;
	crossHatchData_ = nullptr;
	colorGradeData_ = nullptr;
	fogData_ = nullptr;
	godRaysData_ = nullptr;
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
void OffScreen::SetDepthOutlineParams(int kernelSize, const Vector4& color,
	const DepthOutlineParams& params)
{
	if (materialData_) {
		materialData_->kernelSize = kernelSize;
		materialData_->outlineColor = color;
		materialData_->useDepth = params.useDepth ? 1 : 0;
		materialData_->useNormal = params.useNormal ? 1 : 0;
		materialData_->useLuminance = params.useLuminance ? 1 : 0;
		materialData_->depthWeight = params.depthWeight;
		materialData_->normalWeight = params.normalWeight;
		materialData_->luminanceWeight = params.luminanceWeight;
		materialData_->edgeStrength = params.edgeStrength;
		materialData_->depthThreshold = params.depthThreshold;
		materialData_->normalThreshold = params.normalThreshold;
		materialData_->luminanceThreshold = params.luminanceThreshold;
		materialData_->normalWidth = params.normalWidth;
		materialData_->luminanceWidth = params.luminanceWidth;
		materialData_->useDistanceFade = params.useDistanceFade ? 1 : 0;
		materialData_->distanceFadeStart = params.distanceFadeStart;
		materialData_->distanceFadeEnd = params.distanceFadeEnd;
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

/// <summary>ブルーム設定（Dual Kawase 各段の CB を更新）</summary>
void OffScreen::SetBloomParams(const BloomParams& params)
{
	// BrightPass 段（mip0 生成時のみ高輝度抽出）
	if (bloomBrightData_) {
		bloomBrightData_->threshold = params.threshold;
		bloomBrightData_->doThreshold = 1.0f;
	}
	// 純ダウンサンプル段（深いミップ・しきい値なし）
	if (bloomDownData_) {
		bloomDownData_->threshold = 0.0f;
		bloomDownData_->doThreshold = 0.0f;
	}
	// アップサンプル段（にじみ半径。全段共通の定数）
	if (bloomUpData_) {
		bloomUpData_->filterRadius = 1.0f;
	}
	// 合成段
	if (bloomCompositeData_) {
		bloomCompositeData_->intensity = params.intensity;
		bloomCompositeData_->colorTemperature = params.colorTemperature;
	}
}

/// <summary>ポスタリゼーション設定</summary>
void OffScreen::SetPosterizeParams(const PosterizeParams& params)
{
	if (posterizeData_) {
		posterizeData_->steps = params.steps;
		posterizeData_->saturationBoost = params.saturationBoost;
	}
}

/// <summary>クワハラ（油絵）フィルター設定</summary>
void OffScreen::SetKuwaharaParams(const KuwaharaParams& params)
{
	if (kuwaharaData_) {
		kuwaharaData_->radius = params.radius;
		kuwaharaData_->sharpness = params.sharpness;
	}
}

/// <summary>ハーフトーン設定</summary>
void OffScreen::SetHalftoneParams(const HalftoneParams& params)
{
	if (halftoneData_) {
		halftoneData_->dotSize = params.dotSize;
		halftoneData_->angle = params.angle;
		halftoneData_->strength = params.strength;
		halftoneData_->threshold = params.threshold;
	}
}

/// <summary>クロスハッチング設定</summary>
void OffScreen::SetCrossHatchParams(const CrossHatchParams& params)
{
	if (crossHatchData_) {
		crossHatchData_->lineSpacing = params.lineSpacing;
		crossHatchData_->lineWidth = params.lineWidth;
		crossHatchData_->strength = params.strength;
	}
}

/// <summary>カラーグレーディング設定</summary>
void OffScreen::SetColorGradeParams(const ColorGradeParams& params)
{
	if (colorGradeData_) {
		colorGradeData_->shadowColor    = params.shadowColor;
		colorGradeData_->splitBalance   = params.splitBalance;
		colorGradeData_->highlightColor = params.highlightColor;
		colorGradeData_->splitStrength  = params.splitStrength;
		colorGradeData_->vibrance       = params.vibrance;
		colorGradeData_->colorTemp      = params.colorTemp;
		colorGradeData_->colorTint      = params.colorTint;
	}
}

/// <summary>フォグ パラメータ設定 (色・密度などチェーン側で編集する値)</summary>
void OffScreen::SetFogParams(const FogParams& params)
{
	if (!fogData_) return;
	fogData_->fogColor                = params.fogColor;
	fogData_->fogDensity              = params.fogDensity;
	fogData_->fogStart                = params.fogStart;
	fogData_->skyFogClamp             = params.skyFogClamp;
	fogData_->sunInscatterStrength    = params.sunInscatterStrength;
	fogData_->sunColor                = params.sunColor;
	fogData_->heightFogTop            = params.heightFogTop;
	fogData_->heightFogBottom         = params.heightFogBottom;
	fogData_->heightFogDensity        = params.heightFogDensity;
	fogData_->heightFogDistanceScale  = params.heightFogDistanceScale;
}

/// <summary>フォグ カメラ・ライト情報設定 (毎フレーム シーン側から渡す)</summary>
void OffScreen::SetFogCameraAndLight(const Matrix4x4& viewProjectionInverse,
	const Vector3& cameraPos, const Vector3& lightDir)
{
	if (!fogData_) return;
	fogData_->viewProjectionInverse = viewProjectionInverse;
	fogData_->cameraPos             = cameraPos;
	fogData_->sunDirection          = lightDir;
}

/// <summary>GodRays パラメータ設定 (チェーン側で編集する値)</summary>
void OffScreen::SetGodRaysParams(const GodRaysParams& params)
{
	if (!godRaysData_) return;
	godRaysData_->sunColor     = params.sunColor;
	godRaysData_->density      = params.density;
	godRaysData_->weight       = params.weight;
	godRaysData_->decay        = params.decay;
	godRaysData_->exposure     = params.exposure;
	godRaysData_->numSamples   = params.numSamples;
	godRaysData_->skyThreshold = params.skyThreshold;
}

/// <summary>GodRays の太陽スクリーン位置・可視度設定 (毎フレーム)</summary>
void OffScreen::SetGodRaysSun(const Vector2& sunUV, float sunVisibility)
{
	if (!godRaysData_) return;
	godRaysData_->sunUV         = sunUV;
	godRaysData_->sunVisibility = sunVisibility;
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
	CreateBloomResource();
	CreatePosterizeResource();
	CreateKuwaharaResource();
	CreateHalftoneResource();
	CreateCrossHatchResource();
	CreateColorGradeResource();
	CreateFogResource();
	CreateGodRaysResource();
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
	materialData_->useDepth = 1;
	materialData_->useNormal = 0;
	materialData_->useLuminance = 0;
	materialData_->depthWeight = 1.0f;
	materialData_->normalWeight = 1.0f;
	materialData_->luminanceWeight = 1.0f;
	materialData_->edgeStrength = 1.0f;
	materialData_->depthThreshold = 1.0f;
	materialData_->normalThreshold = 1.0f;
	materialData_->luminanceThreshold = 1.0f;
	materialData_->normalWidth = 1.0f;
	materialData_->luminanceWidth = 1.0f;
	materialData_->useDistanceFade = 0;
	materialData_->distanceFadeStart = 30.0f;
	materialData_->distanceFadeEnd = 80.0f;
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

/// <summary>ブルーム用バッファ</summary>
void OffScreen::CreateBloomResource()
{
	// Dual Kawase ブルームは段ごとに別 CB を持つ。1 フレーム内で複数回ディスパッチ
	// するが、各段内では値が一定なので段ごとに 1 個ずつ CB を用意すれば
	// 「同一 CB へ異なる値を書いてハザード」を避けられる。
	bloomBrightCB_ = dxCommon_->CreateBufferResource(sizeof(BloomDownForGPU));
	bloomBrightCB_->Map(0, nullptr, reinterpret_cast<void**>(&bloomBrightData_));
	bloomBrightData_->threshold = 1.0f;
	bloomBrightData_->doThreshold = 1.0f;

	bloomDownCB_ = dxCommon_->CreateBufferResource(sizeof(BloomDownForGPU));
	bloomDownCB_->Map(0, nullptr, reinterpret_cast<void**>(&bloomDownData_));
	bloomDownData_->threshold = 0.0f;
	bloomDownData_->doThreshold = 0.0f;

	bloomUpCB_ = dxCommon_->CreateBufferResource(sizeof(BloomUpForGPU));
	bloomUpCB_->Map(0, nullptr, reinterpret_cast<void**>(&bloomUpData_));
	bloomUpData_->filterRadius = 1.0f;

	bloomCompositeCB_ = dxCommon_->CreateBufferResource(sizeof(BloomCompositeForGPU));
	bloomCompositeCB_->Map(0, nullptr, reinterpret_cast<void**>(&bloomCompositeData_));
	bloomCompositeData_->intensity = 0.5f;
	bloomCompositeData_->colorTemperature = 0.3f;
}

/// <summary>Dual Kawase: ダウンサンプル 1 段（bright=true で mip0 の高輝度抽出兼用）</summary>
void OffScreen::DispatchBloomDown(
	D3D12_GPU_DESCRIPTOR_HANDLE srcSRV,
	D3D12_GPU_DESCRIPTOR_HANDLE dstUAV,
	uint32_t width, uint32_t height, bool bright)
{
	auto cmd = dxCommon_->GetCommandList();
	auto cs = ComputeShaderManager::GetInstance();

	cmd->SetPipelineState(cs->GetComputePipelineState("PostEffectBloomDownCS"));
	cmd->SetComputeRootSignature(cs->GetRootSignature("PostEffectRS_CB"));
	cmd->SetComputeRootDescriptorTable(0, srcSRV);
	cmd->SetComputeRootDescriptorTable(1, dstUAV);
	cmd->SetComputeRootConstantBufferView(2,
		(bright ? bloomBrightCB_ : bloomDownCB_)->GetGPUVirtualAddress());

	cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

/// <summary>Dual Kawase: アップサンプル 1 段（下位ミップを対象ミップへ加算合成）</summary>
void OffScreen::DispatchBloomUp(
	D3D12_GPU_DESCRIPTOR_HANDLE lowerSRV,
	D3D12_GPU_DESCRIPTOR_HANDLE dstUAV,
	uint32_t width, uint32_t height)
{
	auto cmd = dxCommon_->GetCommandList();
	auto cs = ComputeShaderManager::GetInstance();

	cmd->SetPipelineState(cs->GetComputePipelineState("PostEffectBloomUpCS"));
	cmd->SetComputeRootSignature(cs->GetRootSignature("PostEffectRS_CB"));
	cmd->SetComputeRootDescriptorTable(0, lowerSRV);
	cmd->SetComputeRootDescriptorTable(1, dstUAV);
	cmd->SetComputeRootConstantBufferView(2, bloomUpCB_->GetGPUVirtualAddress());

	cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

/// <summary>Dual Kawase: 合成（フル解像度シーン＋ブルーム mip0 を加算して出力へ）</summary>
void OffScreen::DispatchBloomComposite(
	D3D12_GPU_DESCRIPTOR_HANDLE sceneSRV,
	D3D12_GPU_DESCRIPTOR_HANDLE bloomSRV,
	D3D12_GPU_DESCRIPTOR_HANDLE dstUAV,
	uint32_t width, uint32_t height)
{
	auto cmd = dxCommon_->GetCommandList();
	auto cs = ComputeShaderManager::GetInstance();

	cmd->SetPipelineState(cs->GetComputePipelineState("PostEffectBloomCompCS"));
	cmd->SetComputeRootSignature(cs->GetRootSignature("PostEffectRS_Tex"));
	cmd->SetComputeRootDescriptorTable(0, sceneSRV);   // t0 = シーン
	cmd->SetComputeRootDescriptorTable(1, bloomSRV);   // t1 = ブルーム mip0
	cmd->SetComputeRootDescriptorTable(2, dstUAV);     // u0 = 出力
	cmd->SetComputeRootConstantBufferView(3, bloomCompositeCB_->GetGPUVirtualAddress());

	cmd->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
}

/// <summary>ポスタリゼーション用バッファ</summary>
void OffScreen::CreatePosterizeResource()
{
	posterizeResource_ = dxCommon_->CreateBufferResource(sizeof(PosterizeForGPU));
	posterizeResource_->Map(0, nullptr, reinterpret_cast<void**>(&posterizeData_));
	posterizeData_->steps = 5;
	posterizeData_->saturationBoost = 1.2f;
}

/// <summary>クワハラフィルター用バッファ</summary>
void OffScreen::CreateKuwaharaResource()
{
	kuwaharaResource_ = dxCommon_->CreateBufferResource(sizeof(KuwaharaForGPU));
	kuwaharaResource_->Map(0, nullptr, reinterpret_cast<void**>(&kuwaharaData_));
	kuwaharaData_->radius = 4;
	kuwaharaData_->sharpness = 4.0f;
}

/// <summary>ハーフトーン用バッファ</summary>
void OffScreen::CreateHalftoneResource()
{
	halftoneResource_ = dxCommon_->CreateBufferResource(sizeof(HalftoneForGPU));
	halftoneResource_->Map(0, nullptr, reinterpret_cast<void**>(&halftoneData_));
	halftoneData_->dotSize = 6.0f;
	halftoneData_->angle = 45.0f;
	halftoneData_->strength = 0.85f;
	halftoneData_->threshold = 0.75f;
}

/// <summary>クロスハッチング用バッファ</summary>
void OffScreen::CreateCrossHatchResource()
{
	crossHatchResource_ = dxCommon_->CreateBufferResource(sizeof(CrossHatchForGPU));
	crossHatchResource_->Map(0, nullptr, reinterpret_cast<void**>(&crossHatchData_));
	crossHatchData_->lineSpacing = 8.0f;
	crossHatchData_->lineWidth = 1.0f;
	crossHatchData_->strength = 0.7f;
	crossHatchData_->padding = 0.0f;
}

/// <summary>カラーグレーディング用バッファ</summary>
void OffScreen::CreateColorGradeResource()
{
	colorGradeResource_ = dxCommon_->CreateBufferResource(sizeof(ColorGradeForGPU));
	colorGradeResource_->Map(0, nullptr, reinterpret_cast<void**>(&colorGradeData_));
	colorGradeData_->shadowColor    = { 0.0f, 0.02f, 0.08f };
	colorGradeData_->splitBalance   = 0.45f;
	colorGradeData_->highlightColor = { 0.10f, 0.06f, -0.02f };
	colorGradeData_->splitStrength  = 0.25f;
	colorGradeData_->vibrance       = 0.30f;
	colorGradeData_->colorTemp      = 0.08f;
	colorGradeData_->colorTint      = 0.0f;
	colorGradeData_->padding        = 0.0f;
}

/// <summary>フォグ用バッファ</summary>
void OffScreen::CreateFogResource()
{
	fogResource_ = dxCommon_->CreateBufferResource(sizeof(FogForGPU));
	fogResource_->Map(0, nullptr, reinterpret_cast<void**>(&fogData_));
	// デフォルト値はシーンから上書きされる前提だが、安全に単位行列+ゼロベクトルで初期化
	fogData_->viewProjectionInverse = MakeIdentity4x4();
	fogData_->cameraPos             = { 0.0f, 0.0f, 0.0f };
	fogData_->sunDirection          = { 0.0f, -1.0f, 0.0f };
	fogData_->fogColor              = { 0.55f, 0.65f, 0.75f };
	fogData_->fogDensity            = 0.005f;
	fogData_->fogStart              = 10.0f;
	fogData_->skyFogClamp           = 0.7f;
	fogData_->sunInscatterStrength  = 0.5f;
	fogData_->sunColor              = { 1.0f, 0.85f, 0.6f };
	fogData_->heightFogTop          = 20.0f;
	fogData_->heightFogBottom       = -5.0f;
	fogData_->heightFogDensity      = 0.3f;
	fogData_->heightFogDistanceScale = 0.01f;
}

/// <summary>GodRays 用バッファ</summary>
void OffScreen::CreateGodRaysResource()
{
	godRaysResource_ = dxCommon_->CreateBufferResource(sizeof(GodRaysForGPU));
	godRaysResource_->Map(0, nullptr, reinterpret_cast<void**>(&godRaysData_));
	godRaysData_->sunUV         = { 0.5f, 0.5f };
	godRaysData_->sunVisibility = 0.0f;
	godRaysData_->density       = 1.0f;
	godRaysData_->sunColor      = { 1.0f, 0.85f, 0.6f };
	godRaysData_->weight        = 0.25f;
	godRaysData_->decay         = 0.95f;
	godRaysData_->exposure      = 0.12f;
	godRaysData_->numSamples    = 48;
	godRaysData_->skyThreshold  = 0.999f;
}

// ===============================
// 各エフェクトの GPU コマンド設定
// ===============================

/// <summary>
/// PSO・RS・トポロジのセットアップ (最終 blit Copy 用)
/// </summary>
void OffScreen::SetupPipelineAndDraw(OffScreenEffectType type)
{
	auto& pipeline = pipelineMap_[type];
	auto commandList = dxCommon_->GetCommandList();

	commandList->SetPipelineState(pipeline.pipelineState.Get());
	commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}

// ---- 各エフェクトのルートパラメータ設定 ----

void OffScreen::ExecuteCopyEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV)
{
	auto pm = YPipelineManager::GetInstance();
	const auto& indices = pm->GetParameterIndices("BaseOffScreen");
	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(indices.at("gTexture"), inputSRV);
}

