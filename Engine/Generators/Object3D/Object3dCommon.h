#pragma once
// Engine
#include "DX./DirectXCommon.h"
#include "Systems./Camera./Camera.h"

// C++
#include <memory>
#include <mutex>

// 3Dオブジェクト共通部
class DirectXCommon;
class Object3dCommon
{
public: // メンバ関数
    // シングルトンインスタンスの取得
    static Object3dCommon* GetInstance();

    // コンストラクタとデストラクタ
    Object3dCommon() = default;
    ~Object3dCommon() = default;

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
    static std::unique_ptr<Object3dCommon> instance;
    static std::once_flag initInstanceFlag;

    // コピーコンストラクタと代入演算子を削除
    Object3dCommon(Object3dCommon&) = delete;
    Object3dCommon& operator=(Object3dCommon&) = delete;

    // DirectX共通クラスのポインタ
    DirectXCommon* dxCommon_;
    // デフォルトカメラのポインタ
    Camera* defaultCamera_ = nullptr;

    // ルートシグネチャとグラフィックパイプラインステートのポインタ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;
};
