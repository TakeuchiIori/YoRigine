#include "TexturePreloader.h"
#include "TextureManager.h"
#include "Debugger/Logger.h"



// -----------------------------------------------------------------------
// IsSupportedExtension
// -----------------------------------------------------------------------
bool TexturePreloader::IsSupportedExtension(const std::string& ext)
{
    // 小文字変換済みで渡される想定
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg"
        || ext == ".dds" || ext == ".tga" || ext == ".bmp";
}

// -----------------------------------------------------------------------
// PreloadDirectory
// -----------------------------------------------------------------------
void TexturePreloader::PreloadDirectory(
    const std::string& directory,
    bool recursive,
    uint32_t threadCount)
{
    namespace fs = std::filesystem;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        Logger("TexturePreloader: ディレクトリが存在しません: " + directory);
        return;
    }

    std::vector<std::string> files;
    auto collect = [&](const fs::path& path) {
        if (!fs::is_regular_file(path)) return;

        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (IsSupportedExtension(ext)) {
            files.push_back(path.string());
        }
        };

    // アクセス不可フォルダをスキップするオプションを追加
    auto options = fs::directory_options::skip_permission_denied;

    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(directory, options))
                collect(entry.path());
        }
        else {
            for (const auto& entry : fs::directory_iterator(directory, options))
                collect(entry.path());
        }
    }
    catch (const fs::filesystem_error& e) {
        Logger("TexturePreloader: 探索中にエラーが発生しました: " + std::string(e.what()));
    }

    // 0件でもPreloadFilesに渡し、あちらで安全に処理させる
    PreloadFiles(files, threadCount);
}
void TexturePreloader::PreloadFiles(
    const std::vector<std::string>& filePaths,
    uint32_t threadCount)
{
    // 既存ワーカーを完了させてからリセット
    WaitAll();

    // ★ 0件でも絶対に初期化を行う
    totalCount_ = filePaths.size();
    completedCount_ = 0;
    pendingCount_ = 0;

    if (filePaths.empty()) {
        cpuLoadComplete_ = true;
        // コールバックを呼んで早期リターン（二重呼び出し防止）
        if (onCompleteCb_) {
            onCompleteCb_();
            onCompleteCb_ = nullptr;
        }
        return;
    }

    cpuLoadComplete_ = false;

    // スレッド数を決定（ハードウェア並列数を上限に）
    uint32_t hwThreads = std::max(1u, std::thread::hardware_concurrency());
    uint32_t useThreads = (threadCount == 0) ? hwThreads : std::min(threadCount, hwThreads);

    // ファイル数がスレッド数より少なければ調整
    useThreads = std::min(useThreads, static_cast<uint32_t>(filePaths.size()));

    size_t total = filePaths.size();
    size_t perThread = total / useThreads;

    workers_.clear();
    workers_.reserve(useThreads);

    for (uint32_t t = 0; t < useThreads; ++t) {
        size_t start = t * perThread;
        size_t end = (t == useThreads - 1) ? total : start + perThread;

        workers_.emplace_back([this, filePaths, start, end]() {
            WorkerThread(filePaths, start, end);
            });
    }

    // 全ワーカー終了監視スレッド
    std::thread([this]() {
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
        cpuLoadComplete_ = true;
        Logger("TexturePreloader: CPU ロード完了 (" + std::to_string(completedCount_.load()) + " 枚)");
        }).detach();
}

// -----------------------------------------------------------------------
// WorkerThread
// -----------------------------------------------------------------------
void TexturePreloader::WorkerThread(
    const std::vector<std::string>& files,
    size_t startIndex,
    size_t endIndex)
{
    for (size_t i = startIndex; i < endIndex; ++i)
    {
        const std::string& filePath = files[i];

        // ── 既にロード済みならスキップ ──────────────────
        // TextureManager への問い合わせ自体は読み取りのみなので
        // contains() 相当の軽いチェックをここで行う
        // （実際の重複チェックは FlushPendingUploads 側でも行う）
        {
            std::lock_guard<std::mutex> lk(pendingMutex_);
            // pendingUploads_ に同パスがあれば積まない
            if (reservedPaths_.contains(filePath)) {
                ++completedCount_;
                continue;
            }
            reservedPaths_.insert(filePath);
        }

        // ── ファイル読み込み（ここは並列OK） ───────────────
        std::wstring filePathW(filePath.begin(), filePath.end());

        DirectX::ScratchImage image;
        HRESULT hr;

        if (filePathW.ends_with(L".dds")) {
            hr = DirectX::LoadFromDDSFile(
                filePathW.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
        }
        else {
            hr = DirectX::LoadFromWICFile(
                filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
        }

        if (FAILED(hr)) {
            Logger("TexturePreloader: 読み込み失敗: " + filePath);
            ++completedCount_;
            continue;
        }

        // ── ミップマップ生成（ここも並列OK） ───────────────
        DirectX::ScratchImage mipImages;
        if (DirectX::IsCompressed(image.GetMetadata().format)) {
            mipImages = std::move(image);
        }
        else {
            hr = DirectX::GenerateMipMaps(
                image.GetImages(), image.GetImageCount(),
                image.GetMetadata(),
                DirectX::TEX_FILTER_SRGB, 0, mipImages);
        }

        if (FAILED(hr)) {
            Logger("TexturePreloader: ミップマップ生成失敗: " + filePath);
            ++completedCount_;
            continue;
        }

        // ── GPU アップロード待ちキューに積む ────────────────
        {
            std::lock_guard<std::mutex> lk(pendingMutex_);
            PendingUpload upload;
            upload.filePath = filePath;
            upload.mipImages = std::move(mipImages);
            pendingUploads_.push_back(std::move(upload));
            ++pendingCount_;
        }

        ++completedCount_;

        // 進捗コールバック（スレッドから呼ぶため軽い処理のみ推奨）
        if (progressCb_) {
            progressCb_(completedCount_.load(), totalCount_.load(), filePath);
        }
    }
}

// -----------------------------------------------------------------------
// FlushPendingUploads  ★ 必ずメインスレッドから呼ぶ
// -----------------------------------------------------------------------
size_t TexturePreloader::FlushPendingUploads(size_t maxUploadsPerFrame)
{
    // CPU側のロードが完了し、かつキューも空なら完了コールバックを呼ぶ
    if (pendingCount_.load() == 0) {
        if (cpuLoadComplete_ && onCompleteCb_) {
            onCompleteCb_();
            onCompleteCb_ = nullptr;
        }
        return 0;
    }

    std::vector<PendingUpload> batch;
    {
        std::lock_guard<std::mutex> lk(pendingMutex_);

        // 再度空チェック（配列外参照の最終防衛ライン）
        if (pendingUploads_.empty()) {
            pendingCount_ = 0;
            return 0;
        }

        size_t available = pendingUploads_.size();
        size_t count = (maxUploadsPerFrame == 0) ? available : std::min(maxUploadsPerFrame, available);
        batch.reserve(count);

        // ★ 後ろから安全に取り出す（最速・安全）
        for (size_t i = 0; i < count; ++i) {
            batch.push_back(std::move(pendingUploads_.back()));
            pendingUploads_.pop_back();
        }

        pendingCount_ = pendingUploads_.size();
    }

    // ── GPU アップロード ──────────────
    auto* tm = TextureManager::GetInstance();
    for (auto& upload : batch) {
        if (tm->IsLoaded(upload.filePath)) continue;
        tm->UploadFromPreloader(upload.filePath, upload.mipImages);
    }

    // 全完了コールバック
    if (cpuLoadComplete_ && pendingCount_.load() == 0 && onCompleteCb_) {
        onCompleteCb_();
        onCompleteCb_ = nullptr;
    }

    return batch.size();
}
// -----------------------------------------------------------------------
// WaitAll
// -----------------------------------------------------------------------
void TexturePreloader::WaitAll()
{
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
    workers_.clear();
}