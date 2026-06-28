#ifdef USE_IMGUI

#include "PickBuffer.h"

#include <DirectX/DirectXCommon.h>
#include <WinApp/WinApp.h>
#include <iostream>
#include <cassert>

namespace YoRigine {

    void PickBuffer::Initialize()
    {
        // 1. まず最初にチェック。既に初期化済みなら即帰る
        if (pickRT_ != nullptr) return;

        dxCommon_ = DirectXCommon::GetInstance();
        assert(dxCommon_);

        // 2. 初回のみサイズを取得してリソースを作成
        RECT rect{};
        if (GetClientRect(WinApp::GetInstance()->GetHwnd(), &rect)) {
            width_ = static_cast<uint32_t>(rect.right - rect.left);
            height_ = static_cast<uint32_t>(rect.bottom - rect.top);
        }
        viewportW_ = width_;
        viewportH_ = height_;

        CreatePickRenderTarget();
        CreateDepthBuffer();
        CreateReadbackBuffer();
        CreatePickPipeline();

        HRESULT hr = dxCommon_->GetDevice()->CreateFence(
            0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
        assert(SUCCEEDED(hr));
        fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        assert(fenceEvent_);
    }
    void PickBuffer::Resize(uint32_t width, uint32_t height)
    {
        if (width_ == width && height_ == height) return;

        // リソース再作成（GPU同期は呼び出し側が責任を持つ）
        width_ = width;
        height_ = height;

        // 状態リセット
        hasPendingPick_ = false;
        readbackReady_ = false;

        pickRT_.Reset();
        depthBuffer_.Reset();
        CreatePickRenderTarget();
        CreateDepthBuffer();
    }

    // =========================================================================
    // RequestPick  ─  クリックされたフレームに座標を登録
    // =========================================================================
    void PickBuffer::RequestPick(float screenX, float screenY)
    {
        pendingPickX_ = screenX;
        pendingPickY_ = screenY;
        hasPendingPick_ = true;
        readbackReady_ = false;
    }

    // =========================================================================
    // BeginPickPass
    // =========================================================================
    void PickBuffer::BeginPickPass()
    {
        auto* cmd = dxCommon_->GetCommandList().Get();

        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = pickRT_.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &b);

        const float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
        cmd->ClearRenderTargetView(rtvHandle_, clearColor, 0, nullptr);
        cmd->ClearDepthStencilView(dsvHandle_, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        cmd->OMSetRenderTargets(1, &rtvHandle_, FALSE, &dsvHandle_);

        D3D12_VIEWPORT vp = { 0.f, 0.f,
            static_cast<float>(viewportW_), static_cast<float>(viewportH_), 0.f, 1.f };
        D3D12_RECT sc = { 0, 0,
            static_cast<LONG>(viewportW_), static_cast<LONG>(viewportH_) };
        cmd->RSSetViewports(1, &vp);
        cmd->RSSetScissorRects(1, &sc);

        cmd->SetGraphicsRootSignature(pickRootSig_.Get());
        cmd->SetPipelineState(pickPSO_.Get());
    }

    // =========================================================================
    // EndPickPass
    //   hasPendingPick_ が true のときだけ 1px コピーを積んで Signal する。
    //   クリックしていないフレームはコピーしないので無駄なGPU待ちが発生しない。
    // =========================================================================
    void PickBuffer::EndPickPass()
    {
        auto* cmd = dxCommon_->GetCommandList().Get();

        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = pickRT_.Get();
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd->ResourceBarrier(1, &b);

        if (!hasPendingPick_) return; // クリックなし → コピーしない

        // ウィンドウ座標をクランプ
        uint32_t px = static_cast<uint32_t>(std::max(0.f, pendingPickX_));
        uint32_t py = static_cast<uint32_t>(std::max(0.f, pendingPickY_));
        px = std::min(px, width_ - 1);
        py = std::min(py, height_ - 1);

        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = pickRT_.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = readbackBuffer_.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLoc.PlacedFootprint.Offset = 0;
        dstLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_UINT;
        dstLoc.PlacedFootprint.Footprint.Width = 1;
        dstLoc.PlacedFootprint.Footprint.Height = 1;
        dstLoc.PlacedFootprint.Footprint.Depth = 1;
        dstLoc.PlacedFootprint.Footprint.RowPitch = READBACK_ROW_PITCH;

        D3D12_BOX box = { px, py, 0, px + 1, py + 1, 1 };
        cmd->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &box);

        // メインの ExecuteCommandList の後に完了するよう Signal
        dxCommon_->GetCommandQueue()->Signal(fence_.Get(), ++fenceValue_);

        readbackReady_ = true;
        hasPendingPick_ = false;
    }

    // =========================================================================
    // ReadPickResult  ─  コピー完了後に呼んでIDを返す
    // =========================================================================
    int PickBuffer::ReadPickResult()
    {
        if (!readbackReady_) return -1;

        WaitForGPU();

        void* mapped = nullptr;
        D3D12_RANGE readRange = { 0, sizeof(uint32_t) };
        HRESULT hr = readbackBuffer_->Map(0, &readRange, &mapped);
        if (FAILED(hr)) {
            std::cout << "[PickBuffer] Map failed\n";
            return -1;
        }

        uint32_t encoded = *reinterpret_cast<uint32_t*>(mapped);

        D3D12_RANGE writeRange = { 0, 0 };
        readbackBuffer_->Unmap(0, &writeRange);

        readbackReady_ = false;

        if (encoded == 0u) return -1;
        return static_cast<int>(encoded) - 1;
    }

    void PickBuffer::Reset()
    {
        // 状態のみクリア（GPU同期は呼び出し側が行う前提）
        hasPendingPick_ = false;
        readbackReady_ = false;
        pendingPickX_ = 0.f;
        pendingPickY_ = 0.f;
    }

    void PickBuffer::CreatePickRenderTarget()
    {
        auto device = dxCommon_->GetDevice();

        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        d.NumDescriptors = 1;
        device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&rtvHeap_));
        rtvHandle_ = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width = width_;
        rd.Height = height_;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_R32_UINT;
        rd.SampleDesc = { 1, 0 };
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_CLEAR_VALUE cv{};
        cv.Format = DXGI_FORMAT_R32_UINT;
        cv.Color[0] = cv.Color[1] = cv.Color[2] = cv.Color[3] = 0.f;

        D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_DEFAULT };
        HRESULT hr = device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_SOURCE, &cv, IID_PPV_ARGS(&pickRT_));
        assert(SUCCEEDED(hr));

        D3D12_RENDER_TARGET_VIEW_DESC rtvd{};
        rtvd.Format = DXGI_FORMAT_R32_UINT;
        rtvd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(pickRT_.Get(), &rtvd, rtvHandle_);
    }

    void PickBuffer::CreateDepthBuffer()
    {
        auto device = dxCommon_->GetDevice();

        D3D12_DESCRIPTOR_HEAP_DESC d{};
        d.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        d.NumDescriptors = 1;
        device->CreateDescriptorHeap(&d, IID_PPV_ARGS(&dsvHeap_));
        dsvHandle_ = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        rd.Width = width_;
        rd.Height = height_;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_D32_FLOAT;
        rd.SampleDesc = { 1, 0 };
        rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE cv{};
        cv.Format = DXGI_FORMAT_D32_FLOAT;
        cv.DepthStencil.Depth = 1.0f;

        D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_DEFAULT };
        HRESULT hr = device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv, IID_PPV_ARGS(&depthBuffer_));
        assert(SUCCEEDED(hr));

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(depthBuffer_.Get(), &dsvd, dsvHandle_);
    }

    void PickBuffer::CreateReadbackBuffer()
    {
        D3D12_HEAP_PROPERTIES hp{ D3D12_HEAP_TYPE_READBACK };

        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = READBACK_ROW_PITCH;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.Format = DXGI_FORMAT_UNKNOWN;
        rd.SampleDesc = { 1, 0 };
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer_));
        assert(SUCCEEDED(hr));
    }

    void PickBuffer::CreatePickPipeline()
    {
        auto device = dxCommon_->GetDevice();

        D3D12_ROOT_PARAMETER rp[2]{};
        rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rp[0].Descriptor.ShaderRegister = 0;
        rp[0].Descriptor.RegisterSpace = 0;
        rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rp[1].Constants.ShaderRegister = 1;
        rp[1].Constants.RegisterSpace = 0;
        rp[1].Constants.Num32BitValues = 1;
        rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_ROOT_SIGNATURE_DESC rsd{};
        rsd.NumParameters = 2;
        rsd.pParameters = rp;
        rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> sigBlob, errBlob;
        HRESULT hr = D3D12SerializeRootSignature(
            &rsd, D3D_ROOT_SIGNATURE_VERSION_1_0, &sigBlob, &errBlob);
        assert(SUCCEEDED(hr));

        hr = device->CreateRootSignature(
            0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
            IID_PPV_ARGS(&pickRootSig_));
        assert(SUCCEEDED(hr));

        Microsoft::WRL::ComPtr<IDxcBlob> vsBlob, psBlob;
        vsBlob = DirectXCommon::GetInstance()->CompileShader(
            L"Resources/Shaders/Pick/Picks.VS.hlsl", L"vs_6_0");
        psBlob = DirectXCommon::GetInstance()->CompileShader(
            L"Resources/Shaders/Pick/Picks.PS.hlsl", L"ps_6_0");

        D3D12_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
        pso.pRootSignature = pickRootSig_.Get();
        pso.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        pso.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        pso.InputLayout = { layout, _countof(layout) };
        pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso.NumRenderTargets = 1;
        pso.RTVFormats[0] = DXGI_FORMAT_R32_UINT;
        pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        pso.SampleDesc = { 1, 0 };
        pso.SampleMask = UINT_MAX;

        pso.BlendState.IndependentBlendEnable = FALSE;
        pso.BlendState.AlphaToCoverageEnable = FALSE;
        pso.BlendState.RenderTarget[0].BlendEnable = FALSE;
        pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        pso.DepthStencilState.DepthEnable = TRUE;
        pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso.DepthStencilState.StencilEnable = FALSE;

        pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        pso.RasterizerState.DepthClipEnable = TRUE;
        pso.RasterizerState.FrontCounterClockwise = FALSE;

        hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pickPSO_));
        assert(SUCCEEDED(hr));
    }

    void PickBuffer::WaitForGPU()
    {
        if (fence_->GetCompletedValue() < fenceValue_) {
            fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
            WaitForSingleObject(fenceEvent_, INFINITE);
        }
    }

} // namespace YoRigine

#endif // USE_IMGUI