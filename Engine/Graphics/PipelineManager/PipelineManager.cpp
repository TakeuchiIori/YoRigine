#include "PipelineManager.h"
#include "DX./DirectXCommon.h"
#include <d3dx12.h>

namespace {
	const std::wstring DEFAULT_VS_PATH = L"Resources/Shaders/PostEffect/FullScreen/FullScreen.VS.hlsl";
	const std::wstring DEFAULT_PS_PATH = L"Resources/Shaders/PostEffect/CopyImage/CopyImage.PS.hlsl";
}

PipelineManager* PipelineManager::GetInstance()
{
	static PipelineManager instance;
	return &instance;
}

void PipelineManager::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();

	CreatePSO_Sprite();
	CreatePSO_Object();
	CreatePSO_Line();
	CreatePSO_Animation();
	//CreatePSO_BaseOffScreen();
	//CreatePSO_BaseOffScreen(
	//	L"Resources/Shaders/PostEffect/Grayscale/Grayscale.PS.hlsl",
	//	"OffScreen_Grayscale");
	//CreatePSO_BaseOffScreen(
	//	L"Resources/Shaders/PostEffect/Sepia/Sepia.PS.hlsl",
	//	"OffScreen_Sepia");
	//CreatePSO_BaseOffScreen(
	//	L"Resources/Shaders/PostEffect/Vignette/Vignette.PS.hlsl",
	//	"OffScreen_Vignette");
	CreatePSO_Smoothing(
		L"Resources/Shaders/PostEffect/Smoothing/BoxFilter.PS.hlsl",
		"OffScreen_Smoothing");
	//CreatePSO_Particle();
}

ID3D12RootSignature* PipelineManager::GetRootSignature(const std::string& key)
{
	auto it = rootSignatures_.find(key);
	if (it != rootSignatures_.end()) {
		return rootSignatures_[key].Get();
	}
	else {
		return nullptr;
	}
}

ID3D12PipelineState* PipelineManager::GetPipeLineStateObject(const std::string& key)
{
	auto it = pipelineStates_.find(key);
	if (it != pipelineStates_.end()) {
		return pipelineStates_[key].Get();
	}
	else {
		return nullptr;
	}
}



void PipelineManager::CreatePSO_Sprite()
{
	HRESULT hr;
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	// マテリアル
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;		 			// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	// 座標変換行列
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;				// VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	// テクスチャ用ディスクリプターテーブル
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;		// DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;				// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);	// Tableで利用する数

	// 平行光源
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// VertexShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;									// レジスタ番号1を使う

	//// 鏡面反射
	//rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	//rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// VertexShaderで使う
	//rootParameters[4].Descriptor.ShaderRegister = 2;

	//// ポイントライト
	//rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	//rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	//rootParameters[5].Descriptor.ShaderRegister = 3;									// レジスタ番号1を使う

	//// スポットライト
	//rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	//rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	//rootParameters[6].Descriptor.ShaderRegister = 4;									// レジスタ番号4を使う

	descriptionRootSignature.pParameters = rootParameters;								// ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);					// 配列の長さ

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;							// バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;						// 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;						// 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;										// ありったけのMipmapｗｐ使う
	staticSamplers[0].ShaderRegister = 0;												// レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	signatureBlob = nullptr;
	errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatures_["Sprite"]));
	assert(SUCCEEDED(hr));


	// InputLayoutの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendDtateの設定
	D3D12_BLEND_DESC blendDesc{};
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;


	// RasterrizerStateの設定
	D3D12_RASTERIZER_DESC rasterrizerDesc{};
	rasterrizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterrizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Sprite/Sprite.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Sprite/Sprite.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;





	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignatures_["Sprite"].Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterrizerDesc;
	// Depthstencitの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むのか設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_["Sprite"].GetAddressOf()));
	assert(SUCCEEDED(hr));

}

void PipelineManager::CreatePSO_Object()
{
	//===============================================================================//
	/*								ルートシグネチャ									*/
	//===============================================================================//

	HRESULT hr;
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算


	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	//=================== RootParameter ===================//
	D3D12_ROOT_PARAMETER rootParameters[7] = {};
	// マテリアル　＋　鏡面反射
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;		 			// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	// 座標変換行列
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;				// VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	// テクスチャ用ディスクリプターテーブル
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;		// DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;				// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);	// Tableで利用する数

	// 平行光源
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;									// レジスタ番号1を使う

	// カメラ
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[4].Descriptor.ShaderRegister = 2;									// レジスタ番号2を使う


	// ポイントライト
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[5].Descriptor.ShaderRegister = 3;									// レジスタ番号3を使う

	// スポットライト
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[6].Descriptor.ShaderRegister = 4;									// レジスタ番号4を使う


	descriptionRootSignature.pParameters = rootParameters;								// ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);					// 配列の長さ


	//=================== Sampler ===================//
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;							// バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;						// 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;						// 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;										// ありったけのMipmapｗｐ使う
	staticSamplers[0].ShaderRegister = 0;												// レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う

	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatures_["Object"]));
	assert(SUCCEEDED(hr));

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	//=================== InputLayoutの設定 ===================//
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	//=================== BlendDtateの設定 ===================//
	D3D12_BLEND_DESC blendDesc{};
	// 全ての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// RasterrizerStateの設定
	// 裏面（時計回り）を表示しない  [カリング]
	D3D12_RASTERIZER_DESC rasterrizerDesc{};
	rasterrizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	// 三角形の中を塗りつぶす
	rasterrizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//=================== Shaderをコンパイルする ===================//
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;

	vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3d/Object3d.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Object3d/Object3d.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	//=================== DepthStencilStateの設定 ===================//
	// Depthの機能を有効化する
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;


	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignatures_["Object"].Get();	 // Rootsignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;					 // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };										 // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };											 // PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;							 // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterrizerDesc;   				 // RasterrizerState
	// Depthstencitの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むのか設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_["Object"].GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void PipelineManager::CreatePSO_Animation()
{
	HRESULT hr;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算


	D3D12_DESCRIPTOR_RANGE descriptorRangeBone[1] = {};
	descriptorRangeBone[0].BaseShaderRegister = 0;
	descriptorRangeBone[0].NumDescriptors = 1;
	descriptorRangeBone[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeBone[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;


	//=================== RootParameter 複数設定できるので配列 ===================//

	D3D12_ROOT_PARAMETER rootParameters[8] = {};
	// マテリアル
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;		 			// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	// 座標変換行列
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;				// VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	// テクスチャ用ディスクリプターテーブル
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;		// DescriptorTableを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;				// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);	// Tableで利用する数

	// 平行光源
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;									// レジスタ番号1を使う

	// カメラ
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[4].Descriptor.ShaderRegister = 2;									// レジスタ番号2を使う


	// ポイントライト
	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[5].Descriptor.ShaderRegister = 3;									// レジスタ番号3を使う

	// スポットライト
	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters[6].Descriptor.ShaderRegister = 4;									// レジスタ番号4を使う

	// アニメーション
	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ストラクチャーバッファー
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[7].DescriptorTable.pDescriptorRanges = descriptorRangeBone;
	rootParameters[7].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeBone);


	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;								// ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);					// 配列の長さ


	//=================== Samplerの設定 ===================//
	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;							// バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;						// 0~1の範囲外をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;						// 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;										// ありったけのMipmapｗｐ使う
	staticSamplers[0].ShaderRegister = 0;												// レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignatures_["Animation"].GetAddressOf()));
	assert(SUCCEEDED(hr));

	//=================== BlendDtateの設定 ===================//
	D3D12_BLEND_DESC blendDesc_{};
	// 全ての色要素を書き込む
	blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	// RasterrizerStateの設定
	D3D12_RASTERIZER_DESC rasterrizerDesc_{};
	rasterrizerDesc_.CullMode = D3D12_CULL_MODE_NONE;
	rasterrizerDesc_.FillMode = D3D12_FILL_MODE_SOLID;

	//=================== Shaderをコンパイルする ===================//

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Skinning/Skinning.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Skinning/Skinning.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	//=================== DepthStencilStateの設定 ===================//
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// Depthの機能を有効化する
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;






	//=================== InputLayoutの設定 ===================//
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[5] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[3].SemanticName = "WEIGHT";
	inputElementDescs[3].SemanticIndex = 0;
	inputElementDescs[3].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[3].InputSlot = 1;
	inputElementDescs[3].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[4].SemanticName = "INDEX";
	inputElementDescs[4].SemanticIndex = 0;
	inputElementDescs[4].Format = DXGI_FORMAT_R32G32B32A32_SINT;
	inputElementDescs[4].InputSlot = 1;
	inputElementDescs[4].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);


	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignatures_["Animation"].Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc_;
	graphicsPipelineStateDesc.RasterizerState = rasterrizerDesc_;
	// Depthstencitの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むのか設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_["Animation"].GetAddressOf()));
	assert(SUCCEEDED(hr));


}

void PipelineManager::CreatePSO_Line()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// マテリアル
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;		 		// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;				// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;								// レジスタ番号0とバインド

	// 座標変換行列
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;				// CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;			// VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 1;								// レジスタ番号0とバインド

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	descriptionRootSignature.pParameters = rootParameters;						// ルートパラメーター配列へのポインタ
	descriptionRootSignature.NumParameters = _countof(rootParameters);			// 配列の長さ


	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成
	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignatures_["Line"]));
	assert(SUCCEEDED(hr));

	// BlendDtateの設定
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterrizerStateの設定
	D3D12_RASTERIZER_DESC rasterrizerDesc{};
	rasterrizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterrizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Line/Line.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Line/Line.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilStateの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;


	//===========================================================================//
	/*								パイプライン									*/
	//===========================================================================//


	D3D12_INPUT_ELEMENT_DESC inputElementDescs[1] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignatures_["Line"].Get();
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterrizerDesc;
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	// どのように画面に色を打ち込むのか設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_["Line"].GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void PipelineManager::CreatePSO_Particle()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // 0から始まる
	descriptorRange[0].NumDescriptors = 1; // 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算


	// 1. RootSignatureの作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature_{};
	descriptionRootSignature_.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters_[4] = {};
	rootParameters_[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;		 			// CBVを使う
	rootParameters_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	rootParameters_[0].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	rootParameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;				// VertexShaderで使う
	rootParameters_[1].Descriptor.ShaderRegister = 0;									// レジスタ番号0とバインド

	rootParameters_[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;		// DescriptorTableを使う
	rootParameters_[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;				// PixelShaderで使う
	rootParameters_[2].DescriptorTable.pDescriptorRanges = descriptorRange;				// Tableの中身の配列を指定
	rootParameters_[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);	// Tableで利用する数

	rootParameters_[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;					// CBVを使う
	rootParameters_[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// VertexShaderで使う
	rootParameters_[3].Descriptor.ShaderRegister = 1;									// レジスタ番号1を使う
	descriptionRootSignature_.pParameters = rootParameters_;								// ルートパラメーター配列へのポインタ
	descriptionRootSignature_.NumParameters = _countof(rootParameters_);					// 配列の長さ


	// パーティクルのRootSignatureの作成
	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing_[1] = {};
	descriptorRangeForInstancing_[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing_[0].NumDescriptors = 1;
	descriptorRangeForInstancing_[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing_[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParameters_[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters_[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters_[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing_;
	rootParameters_[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing_);


	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC staticSamplers_[1] = {};
	staticSamplers_[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;							// バイリニアフィルタ
	staticSamplers_[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;						// 0~1の範囲外をリピート
	staticSamplers_[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers_[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;						// 比較しない
	staticSamplers_[0].MaxLOD = D3D12_FLOAT32_MAX;										// ありったけのMipmapｗｐ使う
	staticSamplers_[0].ShaderRegister = 0;												// レジスタ番号0を使う
	staticSamplers_[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;					// PixelShaderで使う
	descriptionRootSignature_.pStaticSamplers = staticSamplers_;
	descriptionRootSignature_.NumStaticSamplers = _countof(staticSamplers_);

	// シリアライズしてバイナリにする
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature_,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	// バイナリを元に生成

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignatures_["Particle"].GetAddressOf()));
	assert(SUCCEEDED(hr));


	// InputLayoutの設定
	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[3] = {};
	inputElementDescs_[0].SemanticName = "POSITION";
	inputElementDescs_[0].SemanticIndex = 0;
	inputElementDescs_[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs_[1].SemanticName = "TEXCOORD";
	inputElementDescs_[1].SemanticIndex = 0;
	inputElementDescs_[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs_[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs_[2].SemanticName = "NORMAL";
	inputElementDescs_[2].SemanticIndex = 0;
	inputElementDescs_[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs_[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs_;
	inputLayoutDesc.NumElements = _countof(inputElementDescs_);

	//// 3. BlendDtateの設定
	//blendDesc_.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	//blendDesc_.RenderTarget[0].BlendEnable = true;

	//SetBlendMode(blendDesc_, blendMode_);
	//currentBlendMode_ = kBlendModeAdd;  // 現在のブレンドモード
	//// α値のブレンド
	//blendDesc_.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	//blendDesc_.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	//blendDesc_.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	// RasterrizerStateの設定
	// 裏面（時計回り）を表示しない  [カリング]
	D3D12_RASTERIZER_DESC rasterrizerDesc{};
	rasterrizerDesc.CullMode = D3D12_CULL_MODE_NONE;/* D3D12_CULL_MODE_*/
	// 三角形の中を塗りつぶす
	rasterrizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// Shaderをコンパイルする
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob;
	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob;
	vertexShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/Particle.VS.hlsl",
		L"vs_6_0");
	assert(vertexShaderBlob != nullptr);
	pixelShaderBlob = dxCommon_->CompileShader(L"Resources/Shaders/Particle/Particle.PS.hlsl",
		L"ps_6_0");
	assert(pixelShaderBlob != nullptr);

	// DepthStencilStateの設定
	// Depthの機能を有効化する
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	//書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	//比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;


	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};//graphicsPipelineState_
	graphicsPipelineStateDesc.pRootSignature = rootSignatures_["Patrticle"].Get();					 // Rootsignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;					 // InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(),
	vertexShaderBlob->GetBufferSize() };										 // VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(),
	pixelShaderBlob->GetBufferSize() };											 // PixelShader
	//graphicsPipelineStateDesc.BlendState = blendDesc;							 // BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterrizerDesc;   				 // RasterrizerState
	// Depthstencitの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むのか設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_["Patricle"].GetAddressOf()));
	assert(SUCCEEDED(hr));
}
//
//void PipelineManager::CreatePSO_OffScreen()
//{
//	// ルートシグネチャの設定
//	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
//	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
//
//	// ディスクリプタレンジの設定
//	D3D12_DESCRIPTOR_RANGE descriptorRange = {};
//	descriptorRange.BaseShaderRegister = 0;
//	descriptorRange.NumDescriptors = 1;
//	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
//	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
//
//	// ルートパラメータの設定
//	D3D12_ROOT_PARAMETER rootParameter = {};
//	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
//	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
//	rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;
//	rootParameter.DescriptorTable.NumDescriptorRanges = 1;
//	rootSignatureDesc.pParameters = &rootParameter;
//	rootSignatureDesc.NumParameters = 1;
//
//	// 静的サンプラーの設定
//	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
//	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
//	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
//	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
//	staticSampler.ShaderRegister = 0;
//	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
//	rootSignatureDesc.pStaticSamplers = &staticSampler;
//	rootSignatureDesc.NumStaticSamplers = 1;
//
//	// ルートシグネチャのシリアライズ
//	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
//	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
//	HRESULT hr = D3D12SerializeRootSignature(
//		&rootSignatureDesc,
//		D3D_ROOT_SIGNATURE_VERSION_1,
//		&signatureBlob,
//		&errorBlob
//	);
//
//	if (FAILED(hr)) {
//		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
//		assert(false);
//	}
//
//	// ルートシグネチャの作成
//	hr = dxCommon_->GetDevice()->CreateRootSignature(
//		0,
//		signatureBlob->GetBufferPointer(),
//		signatureBlob->GetBufferSize(),
//		IID_PPV_ARGS(rootSignatures_["OffScreen"].GetAddressOf())
//	);
//	assert(SUCCEEDED(hr));
//
//	// 入力レイアウトの設定
//	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
//	inputLayoutDesc.pInputElementDescs = nullptr;
//	inputLayoutDesc.NumElements = 0;
//
//	// シェーダーのコンパイル
//	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
//		L"Resources/Shaders/FullScreen/FullScreen.VS.hlsl",
//		L"vs_6_0"
//	);
//	assert(vertexShaderBlob != nullptr);
//
//	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
//		L"Resources/Shaders/CopyImage/CopyImage.PS.hlsl",
//		L"ps_6_0"
//	);
//	assert(pixelShaderBlob != nullptr);
//
//	// 深度ステンシルステートの設定
//	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
//	depthStencilDesc.DepthEnable = false;
//	depthStencilDesc.StencilEnable = false;
//	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
//	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
//
//	// グラフィックスパイプラインステートの設定
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
//	pipelineStateDesc.pRootSignature = rootSignatures_["OffScreen"].Get();
//	pipelineStateDesc.InputLayout = inputLayoutDesc;
//	pipelineStateDesc.VS = {
//		vertexShaderBlob->GetBufferPointer(),
//		vertexShaderBlob->GetBufferSize()
//	};
//	pipelineStateDesc.PS = {
//		pixelShaderBlob->GetBufferPointer(),
//		pixelShaderBlob->GetBufferSize()
//	};
//	pipelineStateDesc.DepthStencilState = depthStencilDesc;
//	pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
//	pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
//	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	pipelineStateDesc.NumRenderTargets = 1;
//	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
//	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
//	pipelineStateDesc.SampleDesc.Count = 1;
//	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
//
//	// パイプラインステートの作成
//	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
//		&pipelineStateDesc,
//		IID_PPV_ARGS(pipelineStates_["OffScreen"].GetAddressOf())
//	);
//	assert(SUCCEEDED(hr));
//}

void PipelineManager::CreatePSO_BaseOffScreen(const std::wstring& pixelShaderPath, const std::string& pipelineKey)
{
	// 引数が空の場合はデフォルト値を使用
	std::wstring vsPath = DEFAULT_VS_PATH;
	std::wstring psPath = pixelShaderPath.empty() ? DEFAULT_PS_PATH : pixelShaderPath;
	std::string key = pipelineKey.empty() ? "OffScreen" : pipelineKey;
	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange = {};
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.NumDescriptors = 1;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータの設定
	D3D12_ROOT_PARAMETER rootParameter = {};
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;
	rootParameter.DescriptorTable.NumDescriptorRanges = 1;
	rootSignatureDesc.pParameters = &rootParameter;
	rootSignatureDesc.NumParameters = 1;

	// 静的サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.NumStaticSamplers = 1;

	// ルートシグネチャのシリアライズ
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャの作成 (キーを指定できるよう変更)
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignatures_[key].GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	// 入力レイアウトの設定
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	// 指定されたパスからシェーダーをコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
		vsPath.c_str(),
		L"vs_6_0"
	);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
		psPath.c_str(),
		L"ps_6_0"
	);
	assert(pixelShaderBlob != nullptr);

	// 深度ステンシルステートの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// グラフィックスパイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = rootSignatures_[key].Get();
	pipelineStateDesc.InputLayout = inputLayoutDesc;
	pipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};
	pipelineStateDesc.PS = {
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize()
	};
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// パイプラインステートの作成 (キーを指定できるよう変更)
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_[key].GetAddressOf())
	);
	assert(SUCCEEDED(hr));
}

void PipelineManager::CreatePSO_Smoothing(const std::wstring& pixelShaderPath, const std::string& pipelineKey)
{
	// 引数が空の場合はデフォルト値を使用
	std::wstring vsPath = DEFAULT_VS_PATH;
	std::wstring psPath = pixelShaderPath.empty() ? DEFAULT_PS_PATH : pixelShaderPath;
	std::string key = pipelineKey.empty() ? "Smoothing" : pipelineKey;
	// ルートシグネチャの設定
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange = {};
	descriptorRange.BaseShaderRegister = 0;
	descriptorRange.NumDescriptors = 1;
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ルートパラメータの設定
	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
	rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = _countof(rootParameters);

	// 静的サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC staticSampler = {};
	staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
	staticSampler.ShaderRegister = 0;
	staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootSignatureDesc.pStaticSamplers = &staticSampler;
	rootSignatureDesc.NumStaticSamplers = 1;

	// ルートシグネチャのシリアライズ
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob
	);

	if (FAILED(hr)) {
		DirectXCommon::Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}

	// ルートシグネチャの作成 (キーを指定できるよう変更)
	hr = dxCommon_->GetDevice()->CreateRootSignature(
		0,
		signatureBlob->GetBufferPointer(),
		signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(rootSignatures_[key].GetAddressOf())
	);
	assert(SUCCEEDED(hr));

	// 入力レイアウトの設定
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = nullptr;
	inputLayoutDesc.NumElements = 0;

	// 指定されたパスからシェーダーをコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(
		vsPath.c_str(),
		L"vs_6_0"
	);
	assert(vertexShaderBlob != nullptr);

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(
		psPath.c_str(),
		L"ps_6_0"
	);
	assert(pixelShaderBlob != nullptr);

	// 深度ステンシルステートの設定
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = false;
	depthStencilDesc.StencilEnable = false;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// グラフィックスパイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
	pipelineStateDesc.pRootSignature = rootSignatures_[key].Get();
	pipelineStateDesc.InputLayout = inputLayoutDesc;
	pipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer(),
		vertexShaderBlob->GetBufferSize()
	};
	pipelineStateDesc.PS = {
		pixelShaderBlob->GetBufferPointer(),
		pixelShaderBlob->GetBufferSize()
	};
	pipelineStateDesc.DepthStencilState = depthStencilDesc;
	pipelineStateDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pipelineStateDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateDesc.NumRenderTargets = 1;
	pipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineStateDesc.SampleDesc.Count = 1;
	pipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	// パイプラインステートの作成 (キーを指定できるよう変更)
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
		&pipelineStateDesc,
		IID_PPV_ARGS(pipelineStates_[key].GetAddressOf())
	);
	assert(SUCCEEDED(hr));
}


D3D12_BLEND_DESC PipelineManager::GetBlendDesc(BlendMode _mode)
{
	D3D12_BLEND_DESC blendDesc{};

	switch (_mode)
	{
	case PipelineManager::BlendMode::kBlendModeNormal:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;
	case PipelineManager::BlendMode::kBlendModeAdd:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	case PipelineManager::BlendMode::kBlendModeSubtract:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	case PipelineManager::BlendMode::kBlendModeMultiply:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
		break;
	case PipelineManager::BlendMode::kBlendModeScreen:
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;
	default:
		break;
	}
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	return blendDesc;
}
