#pragma once
#include "DX./DirectXCommon.h"
#include <memory>
#include <mutex>

class SpriteCommon
{
public: // メンバ関数
    /// <summary>
    /// シングルトンのインスタンスを取得
    /// </summary>
    static SpriteCommon* GetInstance();


    // コンストラクタ
    SpriteCommon() = default;
    ~SpriteCommon() = default;

    /// <summary>
    /// 初期化
    /// </summary>
    void Initialize(DirectXCommon* dxCommon);

    /// <summary>
    /// 共通描画設定
    /// </summary>
    void DrawPreference();

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

public: // アクセッサ

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    // シングルトンインスタンス
    static std::unique_ptr<SpriteCommon> instance;
    static std::once_flag initInstanceFlag;

    // コピーコンストラクタと代入演算子を削除
    SpriteCommon(SpriteCommon&) = delete;
    SpriteCommon& operator=(const SpriteCommon&) = delete;

    DirectXCommon* dxCommon_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_= nullptr;

};
