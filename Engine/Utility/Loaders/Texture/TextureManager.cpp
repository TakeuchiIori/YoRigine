#include "TextureManager.h"
// C++
#include <mutex>
#include <assert.h>

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
void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    if (!dxCommon || !srvManager) {
        Log("Error: DirectXCommon or SrvManager is null in TextureManager::Initialize");
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
        Log("Error: srvManager_ or dxCommon_ is null in TextureManager::LoadTexture");
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
    HRESULT hr = DirectX::LoadFromWICFile(filepathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));

    // ミップマップの生成
    DirectX::ScratchImage mipImages{};
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

    // テクスチャデータの追加
    TextureData& textureData = textureDatas[filePath];
    textureData.srvIndex = srvManager_->Allocate();
    textureData.metadata = image.GetMetadata();
    textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
    textureData.intermediateResource = dxCommon_->UploadTextureData(textureData.resource.Get(), image);

    // SRVハンドルの設定
    textureData.srvHandleCPU = srvManager_->GetCPUSRVDescriptorHandle(textureData.srvIndex);
    textureData.srvHandleGPU = srvManager_->GetGPUSRVDescriptorHandle(textureData.srvIndex);

    // SRVの生成
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        UINT(textureData.metadata.mipLevels)
    );
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
    Log("Error: Texture not found for filePath: " + filePath);
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
        Log("Error: Texture not found for filePath: " + filePath);
        throw std::runtime_error("Texture not found for filePath: " + filePath);
    }
    return it->second.srvHandleGPU;
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

void TextureManager::Log(const std::string& message) {
    OutputDebugStringA(message.c_str());
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
        Log("Error: Texture not found for filePath: " + filePath);
        throw std::runtime_error("Texture not found for filePath: " + filePath);
    }
    return it->second.metadata;
}
