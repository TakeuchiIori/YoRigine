#include "DebugConsole.h"
#ifdef USE_IMGUI
#include "DirectXCommon.h"
#include "Editor/Editor.h"
#include <imgui.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>

DebugConsole* DebugConsole::instance_ = nullptr;

DebugConsole* DebugConsole::GetInstance()
{
    if (!instance_)
    {
        instance_ = new DebugConsole();
    }
    return instance_;
}

void DebugConsole::Initialize()
{
    // Editorに登録
    Editor::GetInstance()->RegisterGameUI("デバッグ情報", [this]() { DrawDebugWindow(); });

    // 履歴バッファの初期化
    for (size_t i = 0; i < kHistorySize; ++i)
    {
        frameTimeHistory_.push(0.0f);
        cpuTimeHistory_.push(0.0f);
        gpuTimeHistory_.push(0.0f);
    }

    UpdateMemoryInfo();
}

void DebugConsole::Finalize()
{
    Editor::GetInstance()->UnregisterGameUI("デバッグ情報");
    delete instance_;
    instance_ = nullptr;
}

void DebugConsole::BeginFrame()
{
    frameStartTime_ = std::chrono::high_resolution_clock::now();
    cpuStartTime_ = frameStartTime_;

    // フレーム情報リセット
    currentFrame_ = {};
}

void DebugConsole::EndFrame()
{
    auto now = std::chrono::high_resolution_clock::now();

    // フレーム時間計算
    currentFrame_.frameTime = std::chrono::duration<float, std::milli>(now - frameStartTime_).count();
    currentFrame_.cpuTime = std::chrono::duration<float, std::milli>(now - cpuStartTime_).count();

    // 履歴更新
    UpdateHistory(frameTimeHistory_, currentFrame_.frameTime);
    UpdateHistory(cpuTimeHistory_, currentFrame_.cpuTime);
    UpdateHistory(gpuTimeHistory_, currentFrame_.gpuTime);

    // 平均値計算
    avgFrameTime_ = CalculateAverage(frameTimeHistory_);
    avgCpuTime_ = CalculateAverage(cpuTimeHistory_);
    avgGpuTime_ = CalculateAverage(gpuTimeHistory_);
    currentFps_ = 1000.0f / avgFrameTime_;
}

void DebugConsole::RecordDrawCall(uint32_t vertexCount, uint32_t instanceCount)
{
    currentFrame_.drawCallCount++;
    currentFrame_.triangleCount += (vertexCount / 3) * instanceCount;
    currentFrame_.instanceCount += instanceCount;
}

void DebugConsole::RecordVertexCount(uint32_t vertexCount)
{
    currentFrame_.vertexCount += vertexCount;
}

void DebugConsole::RecordComputeDispatch()
{
    currentFrame_.computeDispatchCount++;
}

void DebugConsole::RecordResourceBarrier()
{
    currentFrame_.barrierCount++;
}

void DebugConsole::RecordTextureLoad()
{
    textureCount_++;
}

void DebugConsole::UpdateMemoryInfo()
{
    auto dxCommon = YoRigine::DirectXCommon::GetInstance();
    if (!dxCommon) return;

    // DXGIアダプタから情報取得
    Microsoft::WRL::ComPtr<IDXGIFactory7> factory = dxCommon->GetDeviceManager()->GetDXGIFactory();
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter;

    if (factory && SUCCEEDED(factory->EnumAdapterByGpuPreference(0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter))))
    {
        DXGI_ADAPTER_DESC3 desc;
        if (SUCCEEDED(adapter->GetDesc3(&desc)))
        {
            memoryInfo_.dedicatedVideoMemory = desc.DedicatedVideoMemory;
            memoryInfo_.dedicatedSystemMemory = desc.DedicatedSystemMemory;
            memoryInfo_.sharedSystemMemory = desc.SharedSystemMemory;
        }

        // 現在の使用量
        DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
        if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
        {
            memoryInfo_.currentUsage = memInfo.CurrentUsage;
        }
    }
}

void DebugConsole::DrawDebugWindow()
{
    if (ImGui::BeginTabBar("DebugTabs"))
    {
        if (ImGui::BeginTabItem("パフォーマンス"))
        {
            DrawPerformanceTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("メモリ"))
        {
            DrawMemoryTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("リソース"))
        {
            DrawResourceTab();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("フレームコンテキスト"))
        {
            DrawFrameContextTab();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void DebugConsole::DrawPerformanceTab()
{

    ImGui::Text("CPU時間: %.2f ms (平均: %.2f ms)", currentFrame_.cpuTime, avgCpuTime_);

    // 描画統計
    ImGui::Text("描画コール: %d", currentFrame_.drawCallCount);
    ImGui::Text("全オブジェクトの三角形数: %d", currentFrame_.triangleCount);
    //ImGui::Text("総頂点数: %d", currentFrame_.vertexCount);
    ImGui::Text("インスタンス数: %d", currentFrame_.instanceCount);
    ImGui::Text("バリア数: %d", currentFrame_.barrierCount);
}

void DebugConsole::DrawMemoryTab()
{
    UpdateMemoryInfo();

    // メモリ情報表示
    ImGui::Text("専用ビデオメモリ: %.2f MB", memoryInfo_.dedicatedVideoMemory / (1024.0f * 1024.0f));
    ImGui::Text("専用システムメモリ: %.2f MB", memoryInfo_.dedicatedSystemMemory / (1024.0f * 1024.0f));
    ImGui::Text("共有システムメモリ: %.2f MB", memoryInfo_.sharedSystemMemory / (1024.0f * 1024.0f));

    ImGui::Separator();

    ImGui::Text("現在の使用量: %.2f MB", memoryInfo_.currentUsage / (1024.0f * 1024.0f));

    // 使用率バー
    float usage = static_cast<float>(memoryInfo_.currentUsage) / memoryInfo_.dedicatedVideoMemory;
    ImGui::ProgressBar(usage, ImVec2(-1, 0), "VRAM使用率");
}

void DebugConsole::DrawResourceTab()
{
    auto dxCommon = YoRigine::DirectXCommon::GetInstance();
    if (!dxCommon) return;

    ImGui::Text("テクスチャ数: %d", textureCount_);
    ImGui::Text("バッファ数: %d", bufferCount_);
    ImGui::Text("パイプライン数: %d", pipelineCount_);

    ImGui::Separator();

    // RTV/DSV情報
    //if (auto rtvManager = dxCommon->GetRTVManager())
    //{
    //    ImGui::Text("RTV使用数: %d / %d", rtvManager->GetUsedCount(), rtvManager->GetMaxCount());
    //}

    //if (auto dsvManager = dxCommon->GetDSVManager())
    //{
    //    ImGui::Text("DSV使用数: %d / %d", dsvManager->GetUsedCount(), dsvManager->GetMaxCount());
    //}

    //// SRV情報
    //if (auto srvManager = dxCommon->GetSrvManager())
    //{
    //    ImGui::Text("SRV使用数: %d / %d", srvManager->GetUsedIndex(), srvManager->GetMaxCount());
    //}
}

void DebugConsole::DrawFrameContextTab()
{
    auto dxCommon = YoRigine::DirectXCommon::GetInstance();
    if (!dxCommon) return;

    auto commandManager = dxCommon->GetCommandList();  // CommandManagerへのアクセスが必要

    ImGui::Text("フレームバッファ数: %d", CommandManager::kFrameCount);
    ImGui::Text("現在のフレーム: %d", dxCommon->GetCurrentBackBufferIndex());

    ImGui::Separator();

    // 各フレームの状態
    for (uint32_t i = 0; i < CommandManager::kFrameCount; ++i)
    {
        ImGui::Text("Frame[%d]:", i);
        ImGui::SameLine();

        if (i == dxCommon->GetCurrentBackBufferIndex())
        {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "実行中");
        } else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "待機中");
        }
    }
}

void DebugConsole::UpdateHistory(std::queue<float>& history, float value)
{
    history.pop();
    history.push(value);
}

float DebugConsole::CalculateAverage(const std::queue<float>& history)
{
    std::queue<float> temp = history;
    float sum = 0.0f;
    size_t count = 0;

    while (!temp.empty())
    {
        sum += temp.front();
        temp.pop();
        count++;
    }

    return count > 0 ? sum / count : 0.0f;
}

#endif // _DEBUG