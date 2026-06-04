#pragma once

// C++
#include <wrl.h>
#include <d3d12.h>
#include <unordered_map>

#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector4.h"

#include "WinApp/WinApp.h"


namespace YoRigine {
	class DirectXCommon;
}


/// <summary>
/// オフスクリーン生成クラス
/// </summary>
class OffScreen
{
public:

	// 利用するエフェクトの種類
	enum class OffScreenEffectType {
		Copy,
		GaussSmoothing,
		DepthOutline,
		Sepia,
		Grayscale,
		Vignette,
		RadialBlur,
		ToneMapping,
		Dissolve,
		Chromatic,
		ColorAdjust,
		ShatterTransition,
		Bloom,
		Posterize,
		Kuwahara,
		Halftone,
		CrossHatch,
		ColorGrade,
		Fog,
		GodRays,
	};
	///************************* パラメータ調整 *************************///
	struct RadialBlurPrams
	{
		Vector2 direction = { 0.0f, 0.0f };
		Vector2 center = { 0.5f, 0.5f };
		float width = 0.001f;
		int sampleCount = 10;
		bool isRadial = true;
	};

	struct DissolveParams {
		float threshold;
		float edgeWidth;
		Vector3 edgeColor;
		float invert;
	};

	static OffScreen* GetInstance() {
		static OffScreen instance;
		return &instance;
	}

	struct ChromaticParams {
		float aberrationStrength = 0.0f;
		Vector2 screenSize = { 0.0f,0.0f };
		float edgeStrength = 0.0f;
	};

	struct ColorAdjustParams {
		float brightness = 0.0f;        // 明るさ
		float contrast = 1.0f;          // コントラスト
		float saturation = 1.0f;        // 彩度
		float hue = 0.0f;               // 色相
	};

	struct ToneParams {
		float gamma = 2.2f;             // ガンマ補正
		float exposure = 1.0f;          // エクスポージャー
	};

	struct ShatterTransitionParams {
		float progress = 0.0f;
		Vector2 resolution = { 0.0f,0.0f };
		float time = 0.0f;
	};

	struct BloomParams {
		float threshold = 0.6f;          // 輝度しきい値
		float intensity = 0.5f;          // ブルームの強さ
		float spread = 6.0f;             // サンプリング半径 [texels]
		float colorTemperature = 0.3f;   // 暖色(+) / 寒色(-)
	};

	struct PosterizeParams {
		int   steps = 5;                 // カラーバンド数
		float saturationBoost = 1.2f;    // 彩度ブースト
	};

	struct KuwaharaParams {
		int   radius = 4;                // フィルター半径 (1〜8)
		float sharpness = 4.0f;          // エッジのシャープさ
	};

	struct HalftoneParams {
		float dotSize = 6.0f;            // ドットセルサイズ [pixels]
		float angle = 45.0f;             // グリッド回転角 [degrees]
		float strength = 0.85f;          // エフェクト強度
		float threshold = 0.75f;         // 適用する輝度しきい値
	};

	struct CrossHatchParams {
		float lineSpacing = 8.0f;        // ハッチング間隔 [pixels]
		float lineWidth = 1.0f;          // 線の太さ
		float strength = 0.7f;           // 強度
	};

	struct ColorGradeParams {
		Vector3 shadowColor = { 0.0f, 0.02f, 0.08f };    // 影の色オフセット (デフォルト: 青緑)
		float   splitBalance = 0.45f;                      // 影/ハイライト境界
		Vector3 highlightColor = { 0.10f, 0.06f, -0.02f }; // ハイライトの色オフセット (デフォルト: 暖色)
		float   splitStrength = 0.25f;                     // スプリット強度
		float   vibrance = 0.30f;                          // バイブランス
		float   colorTemp = 0.08f;                         // 色温度 (+暖色/-寒色)
		float   colorTint = 0.0f;                          // ティント (+紫/-緑)
	};

	struct FogParams {
		Vector3 fogColor = { 0.55f, 0.65f, 0.75f };       // フォグの色 (青みがかった灰)
		float   fogDensity = 0.005f;                       // 指数フォグの係数 (1/単位)
		float   fogStart = 10.0f;                          // この距離からフォグ開始
		float   skyFogClamp = 0.7f;                        // 空のフォグ最大量 (1.0でホワイトアウト)

		float   sunInscatterStrength = 0.5f;               // 太陽方向のグロー強度 (0で無効)
		Vector3 sunColor = { 1.0f, 0.85f, 0.6f };          // インスキャッタの色

		float   heightFogTop = 20.0f;                      // 上端 (これより上は heightFog 寄与なし)
		float   heightFogBottom = -5.0f;                   // 下端 (これより下で最大)
		float   heightFogDensity = 0.3f;                   // 高さフォグの強さ
		float   heightFogDistanceScale = 0.01f;            // 距離スケール
	};

	struct GodRaysParams {
		Vector3 sunColor = { 1.0f, 0.85f, 0.6f };          // 光の色 (暖色がドラマ)
		float   density = 1.0f;                            // 各サンプル間隔のスケール
		float   weight = 0.25f;                            // 1サンプル寄与
		float   decay = 0.95f;                             // 1サンプルごとの減衰倍率
		float   exposure = 0.12f;                          // 最終強度倍率（高いと白飛び）
		int     numSamples = 48;                           // サンプル数 (32〜64推奨, max 64)
		float   skyThreshold = 0.999f;                     // 深度がこれ以上で「空」扱い
	};

	///************************* 基本関数 *************************///

	// 初期化
	void Initialize();

	// 指定されたエフェクトで描画（新しいメインインターフェース）
	void RenderEffect(OffScreenEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);

	///************************* パラメータ設定 *************************///

	// プロジェクション行列のセット
	void SetProjection(const Matrix4x4& projectionMatrix) { projectionInverse_ = projectionMatrix; }

	// 全開放
	void ReleaseResources();

	// トーンマッピングのパラメータを設定
	void SetToneMappingExposure(float exposure);

	// ガウシアンブラーのパラメータを設定
	void SetGaussianBlurParams(float sigma, int kernelSize);

	// デプスアウトラインのパラメータを設定
	void SetDepthOutlineParams(int kernelSize, const Vector4& color);

	// ラジアルブラーのパラメータを設定
	void SetRadialBlurParams(const RadialBlurPrams& params);

	// ディゾルブのパラメータを設定
	void SetDissolveParams(const DissolveParams& params);

	// クロマチックアバーレーションのパラメータを設定
	void SetChromaticParams(const ChromaticParams& params);

	// 色調整のパラメータを設定
	void SetColorAdjustParams(const ColorAdjustParams& colorParams, const ToneParams& toneParams);

	// 破壊シーン遷移のパラメータを設定
	void SetShatterTransitionParams(const ShatterTransitionParams& params);

	// ブルームのパラメータを設定
	void SetBloomParams(const BloomParams& params);

	// ポスタリゼーションのパラメータを設定
	void SetPosterizeParams(const PosterizeParams& params);

	// 油絵フィルターのパラメータを設定
	void SetKuwaharaParams(const KuwaharaParams& params);

	// ハーフトーンのパラメータを設定
	void SetHalftoneParams(const HalftoneParams& params);

	// クロスハッチングのパラメータを設定
	void SetCrossHatchParams(const CrossHatchParams& params);

	// カラーグレーディングのパラメータを設定
	void SetColorGradeParams(const ColorGradeParams& params);

	// フォグのパラメータを設定（PostEffectChain 経由で毎フレーム呼ばれる想定）
	void SetFogParams(const FogParams& params);

	// フォグに必要なカメラ・ライト情報を渡す（シーン側から毎フレーム呼ぶ）
	void SetFogCameraAndLight(const Matrix4x4& viewProjectionInverse,
		const Vector3& cameraPos,
		const Vector3& lightDir);

	// GodRays パラメータ設定（PostEffectChain 経由）
	void SetGodRaysParams(const GodRaysParams& params);

	// GodRays に必要な太陽スクリーン位置と可視度を渡す（シーン側から毎フレーム呼ぶ）
	//   sunUV : 太陽の方向を画面 UV に投影した点
	//   sunVisibility : 0 (カメラの後ろ等で不可視) ～ 1 (正面)
	void SetGodRaysSun(const Vector2& sunUV, float sunVisibility);
	///************************* ゲーム用機能（時間経過ブラー） *************************///

	// ブラーの更新（時間経過による減衰）
	void UpdateBlur(float deltaTime);

	// ブラー演出を開始する（最初強→最後弱）
	void StartBlurMotion(const RadialBlurPrams& params);

	// ブラー演出が実行中かどうか
	bool IsBlurMotionActive() const { return isBlurMotion_; }

	// ブラー演出を停止
	void StopBlurMotion() { isBlurMotion_ = false; }

private:
	///************************* 内部処理 *************************///

	OffScreen() = default;
	~OffScreen() = default;
	OffScreen(const OffScreen&) = delete;
	OffScreen& operator=(const OffScreen&) = delete;

	// リソース作成
	void CreateAllResources();
	void CreateBoxFilterResource();
	void CreateGaussFilterResource();
	void CreateDepthOutLineResource();
	void CreateRadialBlurResource();
	void CreateToneMappingResource();
	void CreateDissolveResource();
	void CreateChromaticResource();
	void CreateColorAdjustResource();
	void CreateShatterTransitionResource();
	void CreateBloomResource();
	void CreatePosterizeResource();
	void CreateKuwaharaResource();
	void CreateHalftoneResource();
	void CreateCrossHatchResource();
	void CreateColorGradeResource();
	void CreateFogResource();
	void CreateGodRaysResource();

	// エフェクト別の描画処理
	void ExecuteCopyEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteGaussSmoothingEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteDepthOutlineEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteSepiaEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteGrayscaleEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteVignetteEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteRadialBlurEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteToneMappingEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteDissolveEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteChromaticEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteColorAdjustEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteShatterTransitionEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteBloomEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecutePosterizeEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteKuwaharaEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteHalftoneEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteCrossHatchEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteColorGradeEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteFogEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);
	void ExecuteGodRaysEffect(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV);

	// 共通描画処理
	void SetupPipelineAndDraw(OffScreenEffectType type);

private:
	///************************* GPU リソース構造体 *************************///

	struct KernelForGPU {
		int kernelSize;
		int padding[3];
	};

	struct GaussKernelForGPU {
		int kernelSize;
		float sigma;
		float padding[2];
	};

	struct Material {
		Matrix4x4 Inverse;
		int kernelSize;
		int padding[3];
		Vector4 outlineColor;
	};

	struct RadialBlurForGPU {
		Vector2 direction;
		Vector2 center;
		float width;
		int sampleCount;
		bool isRadial;
		float padding[1];
	};

	struct ToneMappingForGPU {
		float exposure;
		float padding[3];
	};

	struct DissolveForGPU
	{
		float threshold;
		float edgeWidth;
		float padding[2];

		Vector3 edgeColor;
		float invert;

		float padding1[3];
	};

	struct ChromaticForGPU {
		float aberrationStrength;
		Vector2 screenSize;
		float edgeStrength;
	};


	struct ColorAdjustForGPU {
		float brightness;
		float contrast;
		float saturation;
		float hue;
	};

	struct ToneParamsForGPU {
		float gamma;
		float exposure;
		float padding[2];
	};
	struct ShatterTransitionForGPU {
		float progress;
		Vector2 resolution;
		float time;
		float padding;
	};

	struct BloomForGPU {
		float threshold;
		float intensity;
		float spread;
		float colorTemperature;
	};

	struct PosterizeForGPU {
		int   steps;
		float saturationBoost;
		float padding[2];
	};

	struct KuwaharaForGPU {
		int   radius;
		float sharpness;
		float padding[2];
	};

	struct HalftoneForGPU {
		float dotSize;
		float angle;
		float strength;
		float threshold;
	};

	struct CrossHatchForGPU {
		float lineSpacing;
		float lineWidth;
		float strength;
		float padding;
	};

	struct ColorGradeForGPU {
		Vector3 shadowColor;
		float   splitBalance;
		Vector3 highlightColor;
		float   splitStrength;
		float   vibrance;
		float   colorTemp;
		float   colorTint;
		float   padding;
	};

	// Fog の CB。HLSL 側 (FogParams) と完全一致させること
	struct FogForGPU {
		Matrix4x4 viewProjectionInverse;       // 64

		Vector3   cameraPos;                   // 12
		float     fogDensity;                  //  4

		Vector3   fogColor;                    // 12
		float     fogStart;                    //  4

		Vector3   sunDirection;                // 12
		float     sunInscatterStrength;        //  4

		Vector3   sunColor;                    // 12
		float     skyFogClamp;                 //  4

		float     heightFogTop;                //  4
		float     heightFogBottom;             //  4
		float     heightFogDensity;            //  4
		float     heightFogDistanceScale;      //  4
	};

	// GodRays の CB。HLSL 側 (GodRaysParams) と完全一致
	struct GodRaysForGPU {
		Vector2 sunUV;                         //  8
		float   sunVisibility;                 //  4
		float   density;                       //  4

		Vector3 sunColor;                      // 12
		float   weight;                        //  4

		float   decay;                         //  4
		float   exposure;                      //  4
		int32_t numSamples;                    //  4
		float   skyThreshold;                  //  4
	};


	///************************* パイプライン管理 *************************///

	struct OffScreenPipeline {
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
	};

	YoRigine::DirectXCommon* dxCommon_ = nullptr;
	std::unordered_map<OffScreenEffectType, OffScreenPipeline> pipelineMap_;

	///************************* GPU リソース *************************///

	// ぼかし用
	Microsoft::WRL::ComPtr<ID3D12Resource> boxResource_;
	KernelForGPU* boxData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> gaussResource_;
	GaussKernelForGPU* gaussData_ = nullptr;

	// デプスアウトライン用
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Matrix4x4 projectionInverse_;

	// ラジアルブラー用
	Microsoft::WRL::ComPtr<ID3D12Resource> radialBlurResource_;
	RadialBlurForGPU* radialBlurData_ = nullptr;

	// トーンマッピング用
	Microsoft::WRL::ComPtr<ID3D12Resource> toneMappingResource_;
	ToneMappingForGPU* toneMappingData_ = nullptr;

	// ディゾルブ用
	Microsoft::WRL::ComPtr<ID3D12Resource> dissolveResource_;
	DissolveForGPU* dissolveData_ = nullptr;
	std::string maskTexturePath_ = "Resources/images/noise0.png";

	// クロマチックアバーレーション用
	Microsoft::WRL::ComPtr<ID3D12Resource> chromaticResource_;
	ChromaticForGPU* chromaticData_ = nullptr;

	// 色調整用
	Microsoft::WRL::ComPtr<ID3D12Resource> colorAdjustResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> toneParamsResource_;
	ColorAdjustForGPU* colorAdjustData_ = nullptr;
	ToneParamsForGPU* toneParamsData_ = nullptr;

	// 破壊シーン遷移用
	Microsoft::WRL::ComPtr<ID3D12Resource> shatterTransitionResource_;
	ShatterTransitionForGPU* shatterTransitionData_ = nullptr;
	std::string shatterTexturePath_ = "Resources/images/break.png";

	// ブルーム用
	Microsoft::WRL::ComPtr<ID3D12Resource> bloomResource_;
	BloomForGPU* bloomData_ = nullptr;

	// ポスタリゼーション用
	Microsoft::WRL::ComPtr<ID3D12Resource> posterizeResource_;
	PosterizeForGPU* posterizeData_ = nullptr;

	// 油絵フィルター用
	Microsoft::WRL::ComPtr<ID3D12Resource> kuwaharaResource_;
	KuwaharaForGPU* kuwaharaData_ = nullptr;

	// ハーフトーン用
	Microsoft::WRL::ComPtr<ID3D12Resource> halftoneResource_;
	HalftoneForGPU* halftoneData_ = nullptr;

	// クロスハッチング用
	Microsoft::WRL::ComPtr<ID3D12Resource> crossHatchResource_;
	CrossHatchForGPU* crossHatchData_ = nullptr;

	// カラーグレーディング用
	Microsoft::WRL::ComPtr<ID3D12Resource> colorGradeResource_;
	ColorGradeForGPU* colorGradeData_ = nullptr;

	// フォグ用
	Microsoft::WRL::ComPtr<ID3D12Resource> fogResource_;
	FogForGPU* fogData_ = nullptr;

	// GodRays 用
	Microsoft::WRL::ComPtr<ID3D12Resource> godRaysResource_;
	GodRaysForGPU* godRaysData_ = nullptr;

	///************************* ブラー演出用 *************************///

	RadialBlurPrams radialBlurPrams_;
	bool isBlurMotion_ = false;
	float blurTime_ = 0.0f;
	float blurDuration_ = 1.0f;		// ブラー時間（秒）
	float initialWidth_ = 0.01f;	// ブラー初期幅
	int initialSampleCount_ = 16;

	///************************* 破壊シーン遷移用 *************************///

	ShatterTransitionParams shatterParams_;
};