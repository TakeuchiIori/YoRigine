#pragma once

#ifdef USE_IMGUI

#include <cstdint>
#include <wrl.h>
#include <d3d12.h>

namespace YoRigine {

    class DirectXCommon;

    /// <summary>
    /// GPU Pick Buffer
    ///
    /// ■ 正しいフロー
    ///
    ///   [毎フレーム]
    ///     BeginPickPass()
    ///     DrawForPick()        ← 全オブジェクトを PickPSO で描画
    ///     EndPickPass()        ← クリックがあった場合だけ 1px コピー + Signal
    ///
    ///   [クリック検出時（ObjectSelector）]
    ///     RequestPick(x, y)    ← 座標を登録（EndPickPass でコピーされる）
    ///
    ///   [次フレームのクリック判定]
    ///     ReadPickResult()     ← WaitForGPU → Map → objectID 返却
    ///
    /// ■ 座標について
    ///   x, y はウィンドウ全体の左上を原点としたスクリーン座標で渡す。
    ///   ゲームビュー相対座標 → ウィンドウ座標への変換は呼び出し側で行うこと。
    ///   （viewportPos を加算する）
    /// </summary>
    class PickBuffer
    {
    public:
        static PickBuffer* GetInstance()
        {
            static PickBuffer inst;
            return &inst;
        }

        void Initialize();

        /// ウィンドウリサイズ時に呼ぶ（RT 再生成）
        void Resize(uint32_t width, uint32_t height);

        /// ゲームビューのサイズが変わったときに呼ぶ（RT 再生成なし・ビューポートのみ更新）
        /// BeginPickPass の直前に ObjectSelector から毎フレーム呼ばれる。
        void SetViewport(uint32_t viewW, uint32_t viewH)
        {
            viewportW_ = viewW;
            viewportH_ = viewH;
        }

        // 毎フレーム呼ぶ
        void BeginPickPass();
        void EndPickPass();

        /// クリックされたフレームに座標を登録する
        /// x, y = ウィンドウ全体座標（ゲームビュー左上を原点にした値 + viewportPos）
        void RequestPick(float screenX, float screenY);

        /// EndPickPass の次フレーム以降に呼んでIDを取得する（-1 = 空選択）
        int  ReadPickResult();

        /// シーン遷移時などに状態をリセット（GPU完了待機含む）
        void Reset();

        ID3D12RootSignature* GetPickRootSignature() const { return pickRootSig_.Get(); }
        ID3D12PipelineState* GetPickPipelineState() const { return pickPSO_.Get(); }

        static constexpr uint32_t ROOT_PARAM_MVP_CBV = 0; // b0: WVP (VS)
        static constexpr uint32_t ROOT_PARAM_OBJECT_ID = 1; // b1: objectID+1 (PS)

    private:
        PickBuffer() = default;
        ~PickBuffer() = default;
        PickBuffer(const PickBuffer&) = delete;
        PickBuffer& operator=(const PickBuffer&) = delete;

        void CreatePickRenderTarget();
        void CreateDepthBuffer();
        void CreateReadbackBuffer();
        void CreatePickPipeline();
        void WaitForGPU();

        DirectXCommon* dxCommon_ = nullptr;
        uint32_t width_ = 1; // RT のフルサイズ（ウィンドウサイズ）
        uint32_t height_ = 1;
        uint32_t viewportW_ = 1; // 実際のゲームビューサイズ（ビューポート用）
        uint32_t viewportH_ = 1;

        Microsoft::WRL::ComPtr<ID3D12Resource>       pickRT_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
        D3D12_CPU_DESCRIPTOR_HANDLE                  rtvHandle_{};

        Microsoft::WRL::ComPtr<ID3D12Resource>       depthBuffer_;
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
        D3D12_CPU_DESCRIPTOR_HANDLE                  dsvHandle_{};

        Microsoft::WRL::ComPtr<ID3D12Resource>       readbackBuffer_;
        static constexpr uint32_t READBACK_ROW_PITCH = 256;

        Microsoft::WRL::ComPtr<ID3D12RootSignature>  pickRootSig_;
        Microsoft::WRL::ComPtr<ID3D12PipelineState>  pickPSO_;

        Microsoft::WRL::ComPtr<ID3D12Fence>          fence_;
        uint64_t fenceValue_ = 0;
        void* fenceEvent_ = nullptr;

        float pendingPickX_ = 0.f;
        float pendingPickY_ = 0.f;
        bool  hasPendingPick_ = false; // RequestPick が呼ばれたフレーム = true
        bool  readbackReady_ = false; // EndPickPass でコピー済み = true
    };

} // namespace YoRigine

#endif // USE_IMGUI