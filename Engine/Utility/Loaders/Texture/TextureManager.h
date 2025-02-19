#pragma once
// C++
#include <string>
#include <wrl.h>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <d3d12.h>

// Engine
#include "DX./DirectXCommon.h"
#include "DirectXTex.h"
#include "SrvManager./SrvManager.h"


// テクスチャマネージャー
class TextureManager
{
private:
    // テクスチャ1枚分のデータ
    struct TextureData {
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
        uint32_t srvIndex;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
    };

public: // メンバ関数

    /// <summary>
    /// シングルトンインスタンスの取得
    /// </summary>
    static TextureManager* GetInstance();

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize();

    // コンストラクタ
    TextureManager() = default;
    // デストラクタ
    ~TextureManager() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    /// <summary>
    /// テクスチャファイルの読み込み
    /// </summary>
    void LoadTexture(const std::string& filePath);

    /// <summary>
    /// SRVインデックスの取得
    /// </summary>
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);

    /// <summary>
    /// テクスチャ番号からGPUハンドルを取得
    /// </summary>
    D3D12_GPU_DESCRIPTOR_HANDLE GetsrvHandleGPU(const std::string& filePath);

    // 文字列の変換
    std::wstring ConvertString(const std::string& str);
    std::string ConvertString(const std::wstring& str);
    // ログ出力
    void Log(const std::string& message);

    /// <summary>
    /// メタデータの取得
    /// </summary>
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

private: // メンバ変数

    // シングルトンインスタンス
    static std::unique_ptr<TextureManager> instance;
    static std::once_flag initInstanceFlag;

    // コピーコンストラクタと代入演算子を削除
    TextureManager(TextureManager&) = delete;
    TextureManager& operator=(TextureManager&) = delete;

    // テクスチャデータ
    std::unordered_map<std::string, TextureData> textureDatas;

    // DirectX共通オブジェクト
    DirectXCommon* dxCommon_ = nullptr;

    // SRVマネージャー
    SrvManager* srvManager_ = nullptr;

    // SRVインデックスの開始番号
    static uint32_t kSRVIndexTop;
};
