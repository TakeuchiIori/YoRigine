#include "TextureManager.h"
#include "Debugger/Logger.h"


// C++
#include <mutex>
#include <assert.h>
#include <d3dx12.h>

// シングルトンインスタンスの初期化
std::unique_ptr<TextureManager> TextureManager::instance = nullptr;
std::once_flag TextureManager::initInstanceFlag;

/// <summary>
/// シングルトンインスタンスの取得
/// </summary>
TextureManager* TextureManager::GetInstance()
{
	std::call_once(initInstanceFlag, []() {
		instance = std::make_unique<TextureManager>();
		});
	return instance.get();
}

/// <summary>
/// 終了処理
/// </summary>
void TextureManager::Finalize()
{

	instance.reset(); // インスタンスをリセットし、メモリを解放
}

/// <summary>
/// 初期化処理
/// </summary>
/// <param name="dxCommon">DirectX共通オブジェクト</param>
/// <param name="srvManager">SRVマネージャー</param>
void TextureManager::Initialize(YoRigine::DirectXCommon* dxCommon, SrvManager* srvManager)
{
	if (!dxCommon || !srvManager) {
		Logger("Error: DirectXCommon or SrvManager is null in TextureManager::Initialize");
		return;
	}
	// DirectXCommonの設定
	dxCommon_ = dxCommon;

	// SRVマネージャーの設定
	srvManager_ = srvManager;

	// テクスチャデータのバケット数を予約
	textureDatas.reserve(SrvManager::kMaxSRVCount_);
}

/// <summary>
/// テクスチャファイルの読み込み
/// </summary>
/// <param name="filePath">読み込むファイルパス</param>
void TextureManager::LoadTexture(const std::string& filePath)
{

	if (!srvManager_ || !dxCommon_) {
		Logger("Error: srvManager_ or dxCommon_ is null in TextureManager::LoadTexture");
		return;
	}

	// 既に読み込み済みであれば早期リターン
	if (textureDatas.contains(filePath)) {
		return;
	}

	// テクスチャ上限枚数チェック
	assert(srvManager_->IsAllocation());

	// テクスチャファイルをWICから読み込み
	DirectX::ScratchImage image{};
	std::wstring filepathW = ConvertString(filePath);

	HRESULT hr;

	if (filepathW.ends_with(L".dds")) {
		hr = DirectX::LoadFromDDSFile(filepathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	} else {
		hr = DirectX::LoadFromWICFile(filepathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}
	assert(SUCCEEDED(hr));

	if (image.GetMetadata().width == 0 || image.GetMetadata().height == 0) {
		Logger("Error: Invalid image dimensions (width or height is 0): " + filePath);
		assert(false);
		return;
	}

	// ミップマップの生成
	DirectX::ScratchImage mipImages{};
	if (DirectX::IsCompressed(image.GetMetadata().format)) {
		mipImages = std::move(image);
	} else {
		hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	}
	assert(SUCCEEDED(hr));

	// テクスチャデータの追加
	TextureData& textureData = textureDatas[filePath];
	textureData.srvIndex = srvManager_->Allocate();
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = CreateTextureResource(textureData.metadata);
	textureData.intermediateResource = UploadTextureData(textureData.resource.Get(), mipImages);

	// SRVハンドルの設定
	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	// SRVの生成
	srvManager_->CreateSRVforTexture2D(
		textureData.srvIndex,
		textureData.resource.Get(),
		textureData.metadata);
}

/// <summary>
/// ファイルパスからテクスチャのSRVインデックスを取得
/// </summary>
/// <param name="filePath">テクスチャファイルのパス</param>
/// <returns>SRVインデックス</returns>
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
	auto it = textureDatas.find(filePath);
	if (it != textureDatas.end()) {
		return it->second.srvIndex;
	}
	Logger("Error: Texture not found for filePath: " + filePath);
	assert(0);
	return 0;
}

/// <summary>
/// GPUハンドルを取得
/// </summary>
/// <param name="filePath">テクスチャファイルのパス</param>
/// <returns>GPUハンドル</returns>
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetsrvHandleGPU(const std::string& filePath)
{
	auto it = textureDatas.find(filePath);
	if (it == textureDatas.end()) {
		Logger("Error: Texture not found for filePath: " + filePath);
		throw std::runtime_error("Texture not found for filePath: " + filePath);
	}
	return it->second.srvHandleGPU;
}

D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::GetsrvHandleCPU(const std::string& filePath)
{
	auto it = textureDatas.find(filePath);
	if (it == textureDatas.end()) {
		Logger("Error: Texture not found for filePath: " + filePath);
		throw std::runtime_error("Texture not found for filePath: " + filePath);
	}
	return it->second.srvHandleCPU;
}

std::wstring TextureManager::ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}
	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string TextureManager::ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}
	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &result[0], sizeNeeded, NULL, NULL);
	return result;
}

/// <summary>
/// メタデータの取得
/// </summary>
/// <param name="filePath">テクスチャファイルのパス</param>
/// <returns>メタデータ</returns>
const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath)
{
	auto it = textureDatas.find(filePath);
	if (it == textureDatas.end()) {
		Logger("Error: Texture not found for filePath: " + filePath);
		throw std::runtime_error("Texture not found for filePath: " + filePath);
	}
	return it->second.metadata;
}

void TextureManager::UploadFromPreloader(
	const std::string& filePath,
	const DirectX::ScratchImage& mipImages)
{
	if (!srvManager_ || !dxCommon_) {
		Logger("Error: srvManager_ or dxCommon_ is null in UploadFromPreloader");
		return;
	}

	// 重複チェック（FlushPendingUploads 側でもチェックしているが念のため）
	if (textureDatas.contains(filePath)) return;

	// テクスチャ上限チェック
	assert(srvManager_->IsAllocation());

	// ── 以降は既存 LoadTexture の後半と同じ GPU 処理 ──────
	TextureData& textureData = textureDatas[filePath];
	textureData.srvIndex = srvManager_->Allocate();
	textureData.metadata = mipImages.GetMetadata();
	textureData.resource = CreateTextureResource(textureData.metadata);
	textureData.intermediateResource =
		UploadTextureData(textureData.resource.Get(), mipImages);

	textureData.srvHandleCPU =
		srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);
	textureData.srvHandleGPU =
		srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	srvManager_->CreateSRVforTexture2D(
		textureData.srvIndex,
		textureData.resource.Get(),
		textureData.metadata);
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
	// 1. medataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);									 // Textureの幅
	resourceDesc.Height = UINT(metadata.height);								 // Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);						 // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);					 // 奥行き　or　配列Textureの配列数
	resourceDesc.Format = metadata.format;										 // TextureのFormat
	resourceDesc.SampleDesc.Count = 1;											 // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);		 // textureの次元数。普段使っているのは２次元
	// 2. 利用するHeapの設定。非常に特殊な運用。 02_04で一般ていなケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;								 // 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;		 // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;					 // プロセッサの近くに配置
	// Resourceの生成　( VRAM )
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = YoRigine::DirectXCommon::GetInstance()->GetDevice()->CreateCommittedResource(
		&heapProperties,						// heapの設定
		D3D12_HEAP_FLAG_NONE,					// heaoの特殊な設定。特になし。
		&resourceDesc,							// Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,			// データ移送される設定
		nullptr,								// Clear最低値。使わないのでnullptr
		IID_PPV_ARGS(&resource));				// 作成するResourceポインタへのポインタ
	if (FAILED(hr)) {
		assert(SUCCEEDED(hr));
	}
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> TextureManager::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	// 読み込んだデータからDirectX12用のSubresourceの配列を作成
	DirectX::PrepareUpload(YoRigine::DirectXCommon::GetInstance()->GetDevice().Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	// Subresourceの数を基に、コピー元となるIntermediateResourceに必要なサイズを計算する。
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	// 計算したサイズでIntermediateResourceを作成
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = YoRigine::DirectXCommon::GetInstance()->CreateBufferResource(intermediateSize);
	// IntermediateResourceにSubresourceのデータ書き込み、tectureに転送するコマンドを積む

	auto commandList_ = YoRigine::DirectXCommon::GetInstance()->GetCommandList();
	UpdateSubresources(commandList_.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

	//Tetureへの転送後は利用できるよう、D3D12_RESOURCE_ STATE_ COPY_DESTからD3012_RESOURCE_STATE GENERIC_READへResourcestateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList_->ResourceBarrier(1, &barrier);
	return intermediateResource;
}
