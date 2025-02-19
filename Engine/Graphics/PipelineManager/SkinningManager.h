#pragma once
// Engine
#include "DX./DirectXCommon.h"
#include "Systems./Camera./Camera.h"

// C++
#include <memory>
#include <mutex>

// 3Dオブジェクト共通部
class DirectXCommon;
class SkinningManager
{
public: // メンバ関数
    // シングルトンインスタンスの取得
    static SkinningManager* GetInstance();

    // 終了処理
    void Finalize();

    // コンストラクタとデストラクタ
    SkinningManager() = default;
    ~SkinningManager() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// 共通部描画設定
    /// </summary>
    void DrawPreference();

public: // アクセッサ
    // getter
    Camera* GetDefaultCamera() const { return defaultCamera_; }

    // setter
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    void SetDefaultCamera(Camera* camera) { this->defaultCamera_ = camera; }

private:

    /// <summary>
    /// ルートシグネチャをセット
    /// </summary>
    void SetRootSignature();

    /// <summary>
    /// グラフィックスパイプラインをセット
    /// </summary>
    void SetGraphicsCommand();

    /// <summary>
    /// プリミティブトポロジーをセット
    /// </summary>
    void SetPrimitiveTopology();

private:
    // シングルトンインスタンス
    static std::unique_ptr<SkinningManager> instance;
    static std::once_flag initInstanceFlag;

    // コピーコンストラクタと代入演算子を削除
    SkinningManager(SkinningManager&) = delete;
    SkinningManager& operator=(SkinningManager&) = delete;

    // DirectX共通クラスのポインタ
    DirectXCommon* dxCommon_;
    // デフォルトカメラのポインタ
    Camera* defaultCamera_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};

