#pragma once

// C++
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <filesystem>
#include <functional>
#include <algorithm>
#include <cassert>
#include <fstream>

// DirectXTex
#include "DirectXTex.h"
#include <unordered_set>

/// <summary>
/// テクスチャの事前読み込みユーティリティ
/// 
/// 【処理の流れ】
///  ┌─────────────────────────────────────────────────────┐
///  │ワーカースレッド（複数並列）                            │
///  │  ファイルI/O + ミップマップ生成 → pendingUploads_ に積む│
///  └─────────────────────────────────────────────────────┘
///                        ↓ (スレッドセーフなキュー)
///  ┌─────────────────────────────────────────────────────┐
///  │メインスレッド (FlushPendingUploads() を毎フレーム呼ぶ) │
///  │  SRV確保 → GPUリソース生成 → GPU転送 → SRV生成       │
///  └─────────────────────────────────────────────────────┘
/// </summary>
class TexturePreloader
{
public:
    // 進捗コールバック: (完了数, 総数, 最後に完了したファイルパス)
    using ProgressCallback = std::function<void(size_t completed, size_t total, const std::string& lastPath)>;

    // 全ロード完了コールバック
    using OnCompleteCallback = std::function<void()>;

    TexturePreloader() = default;
    ~TexturePreloader() { WaitAll(); }

    TexturePreloader(const TexturePreloader&) = delete;
    TexturePreloader& operator=(const TexturePreloader&) = delete;

    // -----------------------------------------------------------------------
    // 事前読み込み開始
    // -----------------------------------------------------------------------

    /// <summary>
    /// 指定ディレクトリ以下のテクスチャを非同期で全読み込み開始
    /// 実際のGPUアップロードは FlushPendingUploads() で行う
    /// </summary>
    /// <param name="directory">スキャン対象ディレクトリ</param>
    /// <param name="recursive">サブフォルダも含めるか</param>
    /// <param name="threadCount">使用スレッド数 (0=ハードウェア並列数)</param>
    void PreloadDirectory(
        const std::string& directory,
        bool recursive = true,
        uint32_t threadCount = 0
    );

    /// <summary>
    /// ファイルリストを指定して非同期読み込み開始
    /// </summary>
    void PreloadFiles(
        const std::vector<std::string>& filePaths,
        uint32_t threadCount = 0
    );

    // -----------------------------------------------------------------------
    // メインスレッドから毎フレーム呼ぶ
    // -----------------------------------------------------------------------

    /// <summary>
    /// CPUロード済みのデータをGPUにアップロードする
    /// DirectX12のCommandListを使うため必ずメインスレッドから呼ぶこと
    /// </summary>
    /// <param name="maxUploadsPerFrame">1フレームに処理する最大数 (0=全処理)</param>
    /// <returns>このフレームにアップロードした枚数</returns>
    size_t FlushPendingUploads(size_t maxUploadsPerFrame = 0);

    // -----------------------------------------------------------------------
    // 同期・状態取得
    // -----------------------------------------------------------------------

    /// 全スレッドの完了を待機（ブロッキング）
    void WaitAll();

    /// CPUロードが全て完了しているか
    bool IsCpuLoadComplete() const { return cpuLoadComplete_; }

    /// GPU アップロードも含めて全処理完了か
    bool IsAllComplete() const { return cpuLoadComplete_ && pendingCount_.load() == 0; }

    /// 総ファイル数
    size_t GetTotalCount()     const { return totalCount_.load(); }

    /// CPUロード完了数
    size_t GetCompletedCount() const { return completedCount_.load(); }

    /// GPUアップロード待ちキューの残り数
    size_t GetPendingUploadCount() const { return pendingCount_.load(); }

    /// 進捗 [0.0, 1.0]（GPU完了まで含む）
    float GetProgress() const
    {
        size_t total = totalCount_.load();
        if (total == 0) return 1.0f;
        return static_cast<float>(completedCount_.load()) / static_cast<float>(total);
    }

    // -----------------------------------------------------------------------
    // コールバック設定
    // -----------------------------------------------------------------------

    void SetProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }
    void SetOnCompleteCallback(OnCompleteCallback cb) { onCompleteCb_ = std::move(cb); }

private:
    // -----------------------------------------------------------------------
    // GPUアップロード待ちデータ
    // -----------------------------------------------------------------------
    struct PendingUpload
    {
        std::string             filePath;
        DirectX::ScratchImage   mipImages;  // CPUロード済みデータ
    };

    // -----------------------------------------------------------------------
    // 内部処理
    // -----------------------------------------------------------------------

    /// ワーカースレッド: ファイルI/O + ミップマップ生成
    void WorkerThread(
        const std::vector<std::string>& files,
        size_t startIndex,
        size_t endIndex
    );

    /// 対応拡張子かチェック
    static bool IsSupportedExtension(const std::string& ext);

    // -----------------------------------------------------------------------
    // メンバ変数
    // -----------------------------------------------------------------------

    std::vector<std::thread>        workers_;

    // スレッドセーフなアップロード待ちキュー
    std::vector<PendingUpload>      pendingUploads_;
    std::mutex                      pendingMutex_;
    std::atomic<size_t>             pendingCount_{ 0 };

    // 進捗カウンタ
    std::atomic<size_t>             totalCount_{ 0 };
    std::atomic<size_t>             completedCount_{ 0 };

    std::atomic<bool>               cpuLoadComplete_{ false };

    ProgressCallback                progressCb_ = nullptr;
    OnCompleteCallback              onCompleteCb_ = nullptr;

    // TexturePreloader.h 内の private に追加
    std::unordered_set<std::string> reservedPaths_; // #include <unordered_set> を忘れずに
};