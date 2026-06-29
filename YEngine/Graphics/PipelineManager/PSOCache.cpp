#include "PSOCache.h"
#include <cassert>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <iterator>
#include "Debugger/Logger.h"

namespace YoRigine {
    // バージョン番号（フォーマット変更時にインクリメント）
    // v3: 入力ハッシュ(inputHash)をヘッダーに追加
    static const uint32_t CACHE_VERSION = 3;

    // ============================================================================
    // CompletePipelineCache 実装
    // ============================================================================

    uint64_t CompletePipelineCache::HashData(const void* data, size_t size, uint64_t seed)
    {
        // FNV-1a
        uint64_t hash = seed;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    void CompletePipelineCache::Initialize(ID3D12Device* device, const std::filesystem::path& cacheDirectory)
    {
        device_ = device;
        cacheDirectory_ = cacheDirectory;

        if (!std::filesystem::exists(cacheDirectory_)) {
            std::filesystem::create_directories(cacheDirectory_);
        }

        char buf[512];
        sprintf_s(buf, "\n╔════════════════════════════════════════════════════════════════╗\n");
        Logger(buf);
        sprintf_s(buf, "║     Complete Pipeline Cache System Initialized                 ║\n");
        Logger(buf);
        sprintf_s(buf, "╚════════════════════════════════════════════════════════════════╝\n");
        Logger(buf);
        sprintf_s(buf, "[CompletePipelineCache] Directory: %s\n\n", cacheDirectory_.string().c_str());
        Logger(buf);

        // 起動時に自動読み込み
        LoadAll();
    }

    CompletePipelineCache::PipelineData* CompletePipelineCache::Get(const std::string& key)
    {
        // メモリキャッシュをチェック
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            stats_.memoryHits++;
            char buf[256];
            sprintf_s(buf, "[CompletePipelineCache] Memory HIT: '%s'\n", key.c_str());
            Logger(buf);
            return &it->second;
        }

        stats_.memoryMisses++;

        // ディスクから読み込み試行
        if (LoadFromFile(key)) {
            stats_.diskHits++;
            char buf[256];
            sprintf_s(buf, "[CompletePipelineCache] Disk HIT: '%s'\n", key.c_str());
            Logger(buf);
            return &cache_[key];
        }

        stats_.diskMisses++;
        char buf[256];
        sprintf_s(buf, "[CompletePipelineCache] MISS: '%s' (not found)\n", key.c_str());
        Logger(buf);
        return nullptr;
    }

    void CompletePipelineCache::Add(const std::string& key, const PipelineData& data)
    {
        cache_[key] = data;
        stats_.totalPipelines++;

        // ディスクにも保存
        if (SaveToFile(key, data)) {
            char buf[512];
            sprintf_s(buf, "[CompletePipelineCache] ✅ Added & Saved: '%s' (Parameters: %zu, Total: %zu)\n",
                key.c_str(), data.parameterIndices.size(), stats_.totalPipelines);
            Logger(buf);
        }
        else {
            char buf[512];
            sprintf_s(buf, "[CompletePipelineCache] ⚠️ Added to memory but disk save failed: '%s'\n", key.c_str());
            Logger(buf);
        }
    }

    void CompletePipelineCache::SaveAll()
    {
        Logger("\n[CompletePipelineCache] Saving all pipelines to disk...\n");

        size_t savedCount = 0;
        for (const auto& [key, data] : cache_) {
            if (SaveToFile(key, data)) {
                savedCount++;
            }
        }

        char buf[256];
        sprintf_s(buf, "[CompletePipelineCache] 💾 Saved %zu/%zu pipelines\n\n",
            savedCount, cache_.size());
        Logger(buf);
    }

    void CompletePipelineCache::LoadAll()
    {
        Logger("[CompletePipelineCache] Loading cached pipelines from disk...\n");

        if (!std::filesystem::exists(cacheDirectory_)) {
            Logger("[CompletePipelineCache] Cache directory not found, skipping load\n");
            return;
        }

        size_t loadedCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(cacheDirectory_)) {
            if (entry.path().extension() == ".plcache") {
                std::string key = entry.path().stem().string();
                if (LoadFromFile(key)) {
                    loadedCount++;
                }
            }
        }

        char buf[256];
        sprintf_s(buf, "[CompletePipelineCache] 📂 Loaded %zu pipelines from disk\n\n", loadedCount);
        Logger(buf);
    }

    void CompletePipelineCache::ClearCache()
    {
        Logger("\n[CompletePipelineCache] Clearing all caches...\n");

        cache_.clear();

        if (std::filesystem::exists(cacheDirectory_)) {
            size_t deletedFiles = 0;
            for (const auto& entry : std::filesystem::directory_iterator(cacheDirectory_)) {
                if (entry.path().extension() == ".plcache") {
                    std::filesystem::remove(entry.path());
                    deletedFiles++;
                }
            }
            char buf[256];
            sprintf_s(buf, "[CompletePipelineCache] 🗑️ Deleted %zu cache files\n", deletedFiles);
            Logger(buf);
        }

        stats_ = Stats{};
        Logger("[CompletePipelineCache] Cache cleared\n\n");
    }

    std::filesystem::path CompletePipelineCache::GetCacheFilePath(const std::string& key) const
    {
        return cacheDirectory_ / (key + ".plcache");
    }

    bool CompletePipelineCache::SaveToFile(const std::string& key, const PipelineData& data)
    {
        auto filePath = GetCacheFilePath(key);

        try {
            // ===== 入力ハッシュによる早期スキップ =====
            // 既存ファイルが同じバージョン＆同じ入力ハッシュなら、パイプラインは
            // 変わっていない。出力PSOブロブはドライバ依存で毎回変化するため、
            // ここでファイルを書き換えない（更新時刻も維持し、git差分も出さない）。
            if (std::filesystem::exists(filePath)) {
                std::ifstream existing(filePath, std::ios::binary);
                if (existing.is_open()) {
                    uint32_t existingVersion = 0;
                    uint64_t existingHash = 0;
                    existing.read(reinterpret_cast<char*>(&existingVersion), sizeof(existingVersion));
                    existing.read(reinterpret_cast<char*>(&existingHash), sizeof(existingHash));
                    if (existing.good()
                        && existingVersion == CACHE_VERSION
                        && existingHash == data.inputHash
                        && data.inputHash != 0) {
                        // 入力に変化なし → 書き換え不要
                        return true;
                    }
                }
            }

            // メモリ上のバッファへシリアライズしてから一括書き込みする。
            std::ostringstream oss(std::ios::binary);

            // ===== ヘッダー =====
            oss.write(reinterpret_cast<const char*>(&CACHE_VERSION), sizeof(CACHE_VERSION));
            oss.write(reinterpret_cast<const char*>(&data.inputHash), sizeof(data.inputHash));

            // ===== ルートシグネチャのシリアライズデータ =====
            if (data.rootSignature) {
                // ルートシグネチャのシリアライズデータを取得
                // 注: これは作成時のシリアライズデータが必要
                // 実装の簡略化のため、ここでは再シリアライズは行わず、
                // パラメータインデックスとPSOのみ保存
                uint32_t hasRootSig = 1;
                oss.write(reinterpret_cast<const char*>(&hasRootSig), sizeof(hasRootSig));

                // TODO: 完全な実装では、ルートシグネチャの再シリアライズが必要
                // 現時点ではスキップマーカーのみ書き込み
                uint64_t rootSigSize = 0;
                oss.write(reinterpret_cast<const char*>(&rootSigSize), sizeof(rootSigSize));
            }
            else {
                uint32_t hasRootSig = 0;
                oss.write(reinterpret_cast<const char*>(&hasRootSig), sizeof(hasRootSig));
            }

            // ===== PSOのキャッシュブロブ =====
            if (data.pipelineState) {
                Microsoft::WRL::ComPtr<ID3DBlob> psoBlob;
                HRESULT hr = data.pipelineState->GetCachedBlob(&psoBlob);

                if (SUCCEEDED(hr) && psoBlob) {
                    uint64_t psoSize = psoBlob->GetBufferSize();
                    oss.write(reinterpret_cast<const char*>(&psoSize), sizeof(psoSize));
                    oss.write(static_cast<const char*>(psoBlob->GetBufferPointer()), psoSize);
                }
                else {
                    uint64_t psoSize = 0;
                    oss.write(reinterpret_cast<const char*>(&psoSize), sizeof(psoSize));
                }
            }
            else {
                uint64_t psoSize = 0;
                oss.write(reinterpret_cast<const char*>(&psoSize), sizeof(psoSize));
            }

            // ===== パラメータインデックス =====
            // unordered_map は反復順が不定なので、キー順にソートして
            // 出力を決定的にする（同じ内容なら毎回同じバイト列になるようにする）
            std::vector<std::pair<std::string, UINT>> sortedParams(
                data.parameterIndices.begin(), data.parameterIndices.end());
            std::sort(sortedParams.begin(), sortedParams.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            uint32_t paramCount = static_cast<uint32_t>(sortedParams.size());
            oss.write(reinterpret_cast<const char*>(&paramCount), sizeof(paramCount));

            for (const auto& [name, index] : sortedParams) {
                // 名前の長さ
                uint32_t nameLen = static_cast<uint32_t>(name.size());
                oss.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));

                // 名前
                oss.write(name.c_str(), nameLen);

                // インデックス
                oss.write(reinterpret_cast<const char*>(&index), sizeof(index));
            }

            // ===== インプットレイアウト =====
            uint32_t layoutCount = static_cast<uint32_t>(data.inputLayout.size());
            oss.write(reinterpret_cast<const char*>(&layoutCount), sizeof(layoutCount));

            for (size_t i = 0; i < data.inputLayout.size(); ++i) {
                const auto& element = data.inputLayout[i];

                // SemanticName（対応するsemanticNamesから取得）
                std::string semanticName;
                if (i < data.semanticNames.size()) {
                    semanticName = data.semanticNames[i];
                }
                else {
                    semanticName = element.SemanticName ? element.SemanticName : "";
                }

                uint32_t nameLen = static_cast<uint32_t>(semanticName.size());
                oss.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
                oss.write(semanticName.c_str(), nameLen);

                // その他のフィールド
                oss.write(reinterpret_cast<const char*>(&element.SemanticIndex), sizeof(element.SemanticIndex));
                oss.write(reinterpret_cast<const char*>(&element.Format), sizeof(element.Format));
                oss.write(reinterpret_cast<const char*>(&element.InputSlot), sizeof(element.InputSlot));
                oss.write(reinterpret_cast<const char*>(&element.AlignedByteOffset), sizeof(element.AlignedByteOffset));
                oss.write(reinterpret_cast<const char*>(&element.InputSlotClass), sizeof(element.InputSlotClass));
                oss.write(reinterpret_cast<const char*>(&element.InstanceDataStepRate), sizeof(element.InstanceDataStepRate));
            }

            const std::string newContent = oss.str();

            // ===== 入力が変化した or ファイル未作成 → 書き込み =====
            std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) {
                return false;
            }
            file.write(newContent.data(), static_cast<std::streamsize>(newContent.size()));

            return true;

        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "[CompletePipelineCache] Save failed '%s': %s\n",
                key.c_str(), e.what());
            Logger(buf);
            return false;
        }
    }

    bool CompletePipelineCache::LoadFromFile(const std::string& key)
    {
        auto filePath = GetCacheFilePath(key);

        if (!std::filesystem::exists(filePath)) {
            return false;
        }

        try {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                return false;
            }

            // ===== ヘッダー =====
            uint32_t version;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));

            if (version != CACHE_VERSION) {
                char buf[256];
                sprintf_s(buf, "[CompletePipelineCache] ⚠️ Version mismatch: '%s' (cache v%u, expected v%u)\n",
                    key.c_str(), version, CACHE_VERSION);
                Logger(buf);

                // 古いキャッシュを削除
                file.close();
                std::filesystem::remove(filePath);
                return false;
            }

            PipelineData data;

            // ===== 入力ハッシュ =====
            // 次回保存時の「変化なし判定」に使うので読み込んで保持する
            file.read(reinterpret_cast<char*>(&data.inputHash), sizeof(data.inputHash));

            // ===== ルートシグネチャ =====
            uint32_t hasRootSig;
            file.read(reinterpret_cast<char*>(&hasRootSig), sizeof(hasRootSig));

            if (hasRootSig) {
                uint64_t rootSigSize;
                file.read(reinterpret_cast<char*>(&rootSigSize), sizeof(rootSigSize));

                if (rootSigSize > 0) {
                    // TODO: ルートシグネチャの復元
                    // 現時点ではスキップ（パラメータインデックスのみ使用）
                }
            }

            // ===== PSO =====
            uint64_t psoSize;
            file.read(reinterpret_cast<char*>(&psoSize), sizeof(psoSize));

            if (psoSize > 0) {
                // TODO: PSOの復元
                // 現時点ではスキップ（元のdescriptorが必要なため）
                // 実用的には、メタデータも保存してdescを再構築する必要がある
                std::vector<uint8_t> psoData(psoSize);
                file.read(reinterpret_cast<char*>(psoData.data()), psoSize);
                // ここでは読み飛ばすのみ
            }

            // ===== パラメータインデックス =====
            uint32_t paramCount;
            file.read(reinterpret_cast<char*>(&paramCount), sizeof(paramCount));

            for (uint32_t i = 0; i < paramCount; ++i) {
                // 名前の長さ
                uint32_t nameLen;
                file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));

                // 名前
                std::string name(nameLen, '\0');
                file.read(&name[0], nameLen);

                // インデックス
                UINT index;
                file.read(reinterpret_cast<char*>(&index), sizeof(index));

                data.parameterIndices[name] = index;
            }

            // ===== インプットレイアウト =====
            uint32_t layoutCount;
            file.read(reinterpret_cast<char*>(&layoutCount), sizeof(layoutCount));

            data.inputLayout.resize(layoutCount);
            data.semanticNames.resize(layoutCount);

            for (uint32_t i = 0; i < layoutCount; ++i) {
                // SemanticName
                uint32_t nameLen;
                file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));

                data.semanticNames[i].resize(nameLen);
                file.read(&data.semanticNames[i][0], nameLen);

                // ポインタを設定
                data.inputLayout[i].SemanticName = data.semanticNames[i].c_str();

                // その他のフィールド
                file.read(reinterpret_cast<char*>(&data.inputLayout[i].SemanticIndex), sizeof(data.inputLayout[i].SemanticIndex));
                file.read(reinterpret_cast<char*>(&data.inputLayout[i].Format), sizeof(data.inputLayout[i].Format));
                file.read(reinterpret_cast<char*>(&data.inputLayout[i].InputSlot), sizeof(data.inputLayout[i].InputSlot));
                file.read(reinterpret_cast<char*>(&data.inputLayout[i].AlignedByteOffset), sizeof(data.inputLayout[i].AlignedByteOffset));
                file.read(reinterpret_cast<char*>(&data.inputLayout[i].InputSlotClass), sizeof(data.inputLayout[i].InputSlotClass));
                file.read(reinterpret_cast<char*>(&data.inputLayout[i].InstanceDataStepRate), sizeof(data.inputLayout[i].InstanceDataStepRate));
            }

            // メモリキャッシュに追加
            cache_[key] = std::move(data);

            return true;

        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "[CompletePipelineCache] ❌ Load failed '%s': %s\n",
                key.c_str(), e.what());
            Logger(buf);
            return false;
        }
    }
    // ==============================================
    void PSOCache::Initialize(ID3D12Device* device, const std::filesystem::path& cacheDirectory) {
        device_ = device;
        cacheDirectory_ = cacheDirectory;

        // キャッシュディレクトリを作成
        if (!std::filesystem::exists(cacheDirectory_)) {
            std::filesystem::create_directories(cacheDirectory_);
        }

        stats_ = Stats{};

        // 初期化ログ
        char buf[512];
        sprintf_s(buf, "[PSOCache] Initialized. Cache directory: %s\n",
            cacheDirectory_.string().c_str());
        Logger(buf);
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOCache::GetOrCreate(
        const std::string& key,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc
    ) {
        assert(device_ != nullptr && "PSOCache not initialized");

        char logBuf[512];

        // メモリキャッシュをチェック
        auto it = psoCache_.find(key);
        if (it != psoCache_.end()) {
            stats_.cacheHits++;
            sprintf_s(logBuf, "[PSOCache] Memory cache HIT: '%s' (Total hits: %zu)\n",
                key.c_str(), stats_.cacheHits);
            Logger(logBuf);
            return it->second;
        }

        stats_.cacheMisses++;
        sprintf_s(logBuf, "[PSOCache] Memory cache MISS: '%s'\n", key.c_str());
        Logger(logBuf);

        // ディスクキャッシュをチェック
        auto pso = LoadFromDisk(key, desc);
        if (pso) {
            psoCache_[key] = pso;
            stats_.diskCacheHits++;
            sprintf_s(logBuf, "[PSOCache] Disk cache HIT: '%s' (Loaded from disk, Total disk hits: %zu)\n",
                key.c_str(), stats_.diskCacheHits);
            Logger(logBuf);
            return pso;
        }

        // 新規作成
        sprintf_s(logBuf, "[PSOCache] Creating NEW PSO: '%s'...\n", key.c_str());
        Logger(logBuf);

        Microsoft::WRL::ComPtr<ID3D12PipelineState> newPSO;
        HRESULT hr = device_->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&newPSO));

        if (FAILED(hr)) {
            sprintf_s(logBuf, "[PSOCache] ERROR: Failed to create PSO '%s' (HRESULT: 0x%08X)\n",
                key.c_str(), hr);
            Logger(logBuf);
            throw std::runtime_error("Failed to create graphics pipeline state: " + key);
        }

        // キャッシュに保存
        psoCache_[key] = newPSO;
        stats_.totalPSOs++;

        sprintf_s(logBuf, "[PSOCache] PSO created successfully: '%s' (Total PSOs: %zu)\n",
            key.c_str(), stats_.totalPSOs);
        Logger(logBuf);

        // ディスクに保存
        SaveToDisk(key, newPSO.Get());

        return newPSO;
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOCache::GetOrCreate(
        const std::string& key,
        const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc
    ) {
        assert(device_ != nullptr && "PSOCache not initialized");

        char logBuf[512];

        auto it = psoCache_.find(key);
        if (it != psoCache_.end()) {
            stats_.cacheHits++;
            sprintf_s(logBuf, "[PSOCache] Memory cache HIT (Compute): '%s'\n", key.c_str());
            Logger(logBuf);
            return it->second;
        }

        stats_.cacheMisses++;
        sprintf_s(logBuf, "[PSOCache] Memory cache MISS (Compute): '%s'\n", key.c_str());
        Logger(logBuf);

        auto pso = LoadFromDiskCompute(key, desc);
        if (pso) {
            psoCache_[key] = pso;
            stats_.diskCacheHits++;
            sprintf_s(logBuf, "[PSOCache] Disk cache HIT (Compute): '%s'\n", key.c_str());
            Logger(logBuf);
            return pso;
        }

        sprintf_s(logBuf, "[PSOCache] Creating NEW Compute PSO: '%s'...\n", key.c_str());
        Logger(logBuf);

        Microsoft::WRL::ComPtr<ID3D12PipelineState> newPSO;
        HRESULT hr = device_->CreateComputePipelineState(&desc, IID_PPV_ARGS(&newPSO));

        if (FAILED(hr)) {
            sprintf_s(logBuf, "[PSOCache] ERROR: Failed to create Compute PSO '%s'\n", key.c_str());
            Logger(logBuf);
            throw std::runtime_error("Failed to create compute pipeline state: " + key);
        }

        psoCache_[key] = newPSO;
        stats_.totalPSOs++;

        sprintf_s(logBuf, "[PSOCache] Compute PSO created: '%s'\n", key.c_str());
        Logger(logBuf);

        SaveToDiskCompute(key, newPSO.Get());

        return newPSO;
    }

    void PSOCache::SaveAll() {
        char buf[256];
        sprintf_s(buf, "[PSOCache] Saving all PSOs to disk... (Total: %zu)\n", psoCache_.size());
        Logger(buf);

        size_t savedCount = 0;
        for (auto& [key, pso] : psoCache_) {
            auto filePath = GetCacheFilePath(key);

            // すでにファイルが存在する場合はスキップ
            if (std::filesystem::exists(filePath)) {
                continue;
            }

            // PSOからキャッシュ用バイナリを取得
            Microsoft::WRL::ComPtr<ID3DBlob> blob;
            if (SUCCEEDED(pso->GetCachedBlob(&blob))) {
                try {
                    std::ofstream file(filePath, std::ios::binary);
                    if (file.is_open()) {
                        // バージョン情報を書き込み
                        uint32_t version = 1;
                        file.write(reinterpret_cast<const char*>(&version), sizeof(version));

                        // バイナリサイズを書き込み
                        uint64_t blobSize = blob->GetBufferSize();
                        file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));

                        // バイナリデータを書き込み
                        file.write(reinterpret_cast<const char*>(blob->GetBufferPointer()), blobSize);
                        file.close();

                        savedCount++;
                    }
                }
                catch (const std::exception& e) {
                    char errBuf[512];
                    sprintf_s(errBuf, "[PSOCache] ERROR: Failed to save '%s': %s\n", key.c_str(), e.what());
                    Logger(errBuf);
                }
            }
        }

        sprintf_s(buf, "[PSOCache] Saved %zu PSO cache files.\n", savedCount);
        Logger(buf);
    }

    void PSOCache::ClearCache() {
        Logger("[PSOCache] Clearing cache...\n");

        psoCache_.clear();

        // ディスクキャッシュも削除
        if (std::filesystem::exists(cacheDirectory_)) {
            size_t deletedFiles = 0;
            for (const auto& entry : std::filesystem::directory_iterator(cacheDirectory_)) {
                if (entry.path().extension() == ".pso") {
                    std::filesystem::remove(entry.path());
                    deletedFiles++;
                }
            }
            char buf[256];
            sprintf_s(buf, "[PSOCache] Deleted %zu cache files.\n", deletedFiles);
            Logger(buf);
        }

        stats_ = Stats{};
        Logger("[PSOCache] Cache cleared.\n");
    }

    std::filesystem::path PSOCache::GetCacheFilePath(const std::string& key) const {
        return cacheDirectory_ / (key + ".pso");
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOCache::LoadFromDisk(
        const std::string& key,
        const D3D12_GRAPHICS_PIPELINE_STATE_DESC& originalDesc
    ) {
        auto filePath = GetCacheFilePath(key);

        if (!std::filesystem::exists(filePath)) {
            return nullptr;
        }

        try {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                return nullptr;
            }

            // バージョン確認
            uint32_t version = 0;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));

            const uint32_t CURRENT_VERSION = 1;
            if (version != CURRENT_VERSION) {
                char buf[512];
                sprintf_s(buf,
                    "[PSOCache] Version mismatch for '%s' (File: %u, Current: %u). Regenerating...\n",
                    key.c_str(), version, CURRENT_VERSION);
                Logger(buf);
                file.close();
                std::filesystem::remove(filePath);
                return nullptr;
            }

            // バイナリサイズを読み込み
            uint64_t blobSize = 0;
            file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));

            if (blobSize == 0 || blobSize > 100 * 1024 * 1024) { // 100MB制限
                char buf[256];
                sprintf_s(buf, "[PSOCache] Invalid blob size for '%s': %llu bytes\n", key.c_str(), blobSize);
                Logger(buf);
                file.close();
                std::filesystem::remove(filePath);
                return nullptr;
            }

            // バイナリデータを読み込み
            std::vector<char> blobData(blobSize);
            file.read(blobData.data(), blobSize);
            file.close();

            // キャッシュされたバイナリからPSOを作成
            D3D12_GRAPHICS_PIPELINE_STATE_DESC cachedDesc = originalDesc;
            cachedDesc.CachedPSO.pCachedBlob = blobData.data();
            cachedDesc.CachedPSO.CachedBlobSizeInBytes = blobSize;

            Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
            HRESULT hr = device_->CreateGraphicsPipelineState(&cachedDesc, IID_PPV_ARGS(&pso));

            if (SUCCEEDED(hr)) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] ✅ Successfully loaded PSO from cache: '%s'\n", key.c_str());
                Logger(buf);
                return pso;
            }
            else {
                char buf[512];
                sprintf_s(buf, "[PSOCache] ⚠️ Failed to create PSO from cache '%s' (HRESULT: 0x%08X). Regenerating...\n",
                    key.c_str(), hr);
                Logger(buf);
                std::filesystem::remove(filePath);
                return nullptr;
            }
        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "[PSOCache] Exception while loading '%s': %s\n", key.c_str(), e.what());
            Logger(buf);

            // 破損したキャッシュファイルを削除
            if (std::filesystem::exists(filePath)) {
                std::filesystem::remove(filePath);
            }
            return nullptr;
        }
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSOCache::LoadFromDiskCompute(
        const std::string& key,
        const D3D12_COMPUTE_PIPELINE_STATE_DESC& originalDesc
    ) {
        auto filePath = GetCacheFilePath(key);

        if (!std::filesystem::exists(filePath)) {
            return nullptr;
        }

        try {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                return nullptr;
            }

            // バージョン確認
            uint32_t version = 0;
            file.read(reinterpret_cast<char*>(&version), sizeof(version));

            const uint32_t CURRENT_VERSION = 1;
            if (version != CURRENT_VERSION) {
                char buf[512];
                sprintf_s(buf,
                    "[PSOCache] Version mismatch for compute PSO '%s' (File: %u, Current: %u). Regenerating...\n",
                    key.c_str(), version, CURRENT_VERSION);
                Logger(buf);
                file.close();
                std::filesystem::remove(filePath);
                return nullptr;
            }

            // バイナリサイズを読み込み
            uint64_t blobSize = 0;
            file.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));

            if (blobSize == 0 || blobSize > 100 * 1024 * 1024) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] Invalid blob size for compute PSO '%s': %llu bytes\n",
                    key.c_str(), blobSize);
                Logger(buf);
                file.close();
                std::filesystem::remove(filePath);
                return nullptr;
            }

            // バイナリデータを読み込み
            std::vector<char> blobData(blobSize);
            file.read(blobData.data(), blobSize);
            file.close();

            // キャッシュされたバイナリからPSOを作成
            D3D12_COMPUTE_PIPELINE_STATE_DESC cachedDesc = originalDesc;
            cachedDesc.CachedPSO.pCachedBlob = blobData.data();
            cachedDesc.CachedPSO.CachedBlobSizeInBytes = blobSize;

            Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
            HRESULT hr = device_->CreateComputePipelineState(&cachedDesc, IID_PPV_ARGS(&pso));

            if (SUCCEEDED(hr)) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] ✅ Successfully loaded compute PSO from cache: '%s'\n", key.c_str());
                Logger(buf);
                return pso;
            }
            else {
                char buf[512];
                sprintf_s(buf, "[PSOCache] ⚠️ Failed to create compute PSO from cache '%s' (HRESULT: 0x%08X). Regenerating...\n",
                    key.c_str(), hr);
                Logger(buf);
                std::filesystem::remove(filePath);
                return nullptr;
            }
        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "[PSOCache] Exception while loading compute PSO '%s': %s\n", key.c_str(), e.what());
            Logger(buf);

            if (std::filesystem::exists(filePath)) {
                std::filesystem::remove(filePath);
            }
            return nullptr;
        }
    }

    void PSOCache::SaveToDisk(
        const std::string& key,
        ID3D12PipelineState* pso
    ) {
        if (!pso) {
            return;
        }

        auto filePath = GetCacheFilePath(key);

        try {
            // PSOからキャッシュ用バイナリを取得
            Microsoft::WRL::ComPtr<ID3DBlob> blob;
            HRESULT hr = pso->GetCachedBlob(&blob);

            if (FAILED(hr)) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] ⚠️ Failed to get cached blob for '%s' (HRESULT: 0x%08X)\n",
                    key.c_str(), hr);
                Logger(buf);
                return;
            }

            std::ofstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] ⚠️ Failed to open file for writing: '%s'\n", key.c_str());
                Logger(buf);
                return;
            }

            // バージョン情報を書き込み
            uint32_t version = 1;
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));

            // バイナリサイズを書き込み
            uint64_t blobSize = blob->GetBufferSize();
            file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));

            // バイナリデータを書き込み
            file.write(reinterpret_cast<const char*>(blob->GetBufferPointer()), blobSize);
            file.close();

            char buf[512];
            sprintf_s(buf, "[PSOCache] 💾 Saved PSO to disk: '%s' (%llu bytes)\n",
                key.c_str(), blobSize);
            Logger(buf);
        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "[PSOCache] Exception while saving '%s': %s\n", key.c_str(), e.what());
            Logger(buf);
        }
    }

    void PSOCache::SaveToDiskCompute(
        const std::string& key,
        ID3D12PipelineState* pso
    ) {
        if (!pso) {
            return;
        }

        auto filePath = GetCacheFilePath(key);

        try {
            Microsoft::WRL::ComPtr<ID3DBlob> blob;
            HRESULT hr = pso->GetCachedBlob(&blob);

            if (FAILED(hr)) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] ⚠️ Failed to get cached blob for compute PSO '%s' (HRESULT: 0x%08X)\n",
                    key.c_str(), hr);
                Logger(buf);
                return;
            }

            std::ofstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                char buf[256];
                sprintf_s(buf, "[PSOCache] ⚠️ Failed to open file for writing compute PSO: '%s'\n", key.c_str());
                Logger(buf);
                return;
            }

            // バージョン情報を書き込み
            uint32_t version = 1;
            file.write(reinterpret_cast<const char*>(&version), sizeof(version));

            // バイナリサイズを書き込み
            uint64_t blobSize = blob->GetBufferSize();
            file.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));

            // バイナリデータを書き込み
            file.write(reinterpret_cast<const char*>(blob->GetBufferPointer()), blobSize);
            file.close();

            char buf[512];
            sprintf_s(buf, "[PSOCache] 💾 Saved compute PSO to disk: '%s' (%llu bytes)\n",
                key.c_str(), blobSize);
            Logger(buf);
        }
        catch (const std::exception& e) {
            char buf[512];
            sprintf_s(buf, "[PSOCache] Exception while saving compute PSO '%s': %s\n", key.c_str(), e.what());
            Logger(buf);
        }
    }

    uint64_t PSOCache::CalculateHash(const void* data, size_t size) const {
        // 簡易的なハッシュ関数（FNV-1a）
        uint64_t hash = 0xcbf29ce484222325ULL;
        const uint8_t* bytes = static_cast<const uint8_t*>(data);

        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= 0x100000001b3ULL;
        }

        return hash;
    }

    // ========== GraphicsPipelineBuilder 実装 ==========

    GraphicsPipelineBuilder::GraphicsPipelineBuilder() {
        ZeroMemory(&desc_, sizeof(desc_));
        desc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRootSignature(ID3D12RootSignature* rootSignature) {
        desc_.pRootSignature = rootSignature;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetVertexShader(const void* bytecode, size_t size) {
        desc_.VS.pShaderBytecode = bytecode;
        desc_.VS.BytecodeLength = size;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetVertexShader(ID3DBlob* blob) {
        if (blob) {
            desc_.VS.pShaderBytecode = blob->GetBufferPointer();
            desc_.VS.BytecodeLength = blob->GetBufferSize();
        }
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPixelShader(const void* bytecode, size_t size) {
        desc_.PS.pShaderBytecode = bytecode;
        desc_.PS.BytecodeLength = size;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPixelShader(ID3DBlob* blob) {
        if (blob) {
            desc_.PS.pShaderBytecode = blob->GetBufferPointer();
            desc_.PS.BytecodeLength = blob->GetBufferSize();
        }
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetGeometryShader(const void* bytecode, size_t size) {
        desc_.GS.pShaderBytecode = bytecode;
        desc_.GS.BytecodeLength = size;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetInputLayout(
        const D3D12_INPUT_ELEMENT_DESC* elements,
        UINT numElements
    ) {
        inputElements_.assign(elements, elements + numElements);
        desc_.InputLayout.pInputElementDescs = inputElements_.data();
        desc_.InputLayout.NumElements = numElements;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetBlendState(const D3D12_BLEND_DESC& blendDesc) {
        desc_.BlendState = blendDesc;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRasterizerState(const D3D12_RASTERIZER_DESC& rasterizerDesc) {
        desc_.RasterizerState = rasterizerDesc;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& depthStencilDesc) {
        desc_.DepthStencilState = depthStencilDesc;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetRenderTargetFormats(
        const DXGI_FORMAT* formats,
        UINT numRenderTargets
    ) {
        desc_.NumRenderTargets = numRenderTargets;
        for (UINT i = 0; i < numRenderTargets && i < 8; ++i) {
            desc_.RTVFormats[i] = formats[i];
        }
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetDepthStencilFormat(DXGI_FORMAT format) {
        desc_.DSVFormat = format;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE topology) {
        desc_.PrimitiveTopologyType = topology;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::SetSampleDesc(UINT count, UINT quality) {
        desc_.SampleDesc.Count = count;
        desc_.SampleDesc.Quality = quality;
        return *this;
    }

    GraphicsPipelineBuilder& GraphicsPipelineBuilder::ApplyDefaultSettings() {
        desc_.BlendState = BlendPresets::CreateAlphaBlend();
        desc_.RasterizerState = RasterizerPresets::CreateDefault();
        desc_.DepthStencilState = DepthStencilPresets::CreateDefault();
        desc_.NumRenderTargets = 1;
        desc_.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        desc_.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc_.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc_.SampleDesc.Count = 1;
        desc_.SampleDesc.Quality = 0;
        desc_.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
        return *this;
    }

    // ========== Presets実装 ==========

    namespace BlendPresets {

        D3D12_BLEND_DESC CreateNone() {
            D3D12_BLEND_DESC desc = {};
            desc.RenderTarget[0].BlendEnable = FALSE;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return desc;
        }

        D3D12_BLEND_DESC CreateAlphaBlend() {
            D3D12_BLEND_DESC desc = {};
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return desc;
        }

        D3D12_BLEND_DESC CreateAdditive() {
            D3D12_BLEND_DESC desc = {};
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return desc;
        }

        D3D12_BLEND_DESC CreateSubtractive() {
            D3D12_BLEND_DESC desc = {};
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_MAX;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return desc;
        }

        D3D12_BLEND_DESC CreateMultiply() {
            D3D12_BLEND_DESC desc = {};
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return desc;
        }

        D3D12_BLEND_DESC CreateScreen() {
            D3D12_BLEND_DESC desc = {};
            desc.RenderTarget[0].BlendEnable = TRUE;
            desc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
            desc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
            desc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            desc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            return desc;
        }

    } // namespace BlendPresets

    namespace RasterizerPresets {

        D3D12_RASTERIZER_DESC CreateDefault() {
            D3D12_RASTERIZER_DESC desc = {};
            desc.FillMode = D3D12_FILL_MODE_SOLID;
            desc.CullMode = D3D12_CULL_MODE_BACK;
            return desc;
        }

        D3D12_RASTERIZER_DESC CreateNoCull() {
            auto desc = CreateDefault();
            desc.CullMode = D3D12_CULL_MODE_NONE;
            desc.FillMode = D3D12_FILL_MODE_SOLID;
            return desc;
        }

        D3D12_RASTERIZER_DESC CreateWireframe() {
            auto desc = CreateDefault();
            desc.FillMode = D3D12_FILL_MODE_WIREFRAME;
            return desc;
        }

        D3D12_RASTERIZER_DESC CreateShadow() {
            auto desc = CreateDefault();
            desc.DepthBias = 100000;
            desc.DepthBiasClamp = 0.0f;
            desc.SlopeScaledDepthBias = 1.0f;
            return desc;
        }

    } // namespace RasterizerPresets

    namespace DepthStencilPresets {

        D3D12_DEPTH_STENCIL_DESC CreateDefault() {
            D3D12_DEPTH_STENCIL_DESC desc = {};
            desc.DepthEnable = TRUE;
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            return desc;
        }

        D3D12_DEPTH_STENCIL_DESC CreateReadOnly() {
            auto desc = CreateDefault();
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            return desc;
        }

        D3D12_DEPTH_STENCIL_DESC CreateDisabled() {
            D3D12_DEPTH_STENCIL_DESC desc = {};
            desc.DepthEnable = FALSE;
            desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            desc.StencilEnable = FALSE;
            return desc;
        }

        D3D12_DEPTH_STENCIL_DESC CreateWriteOnly() {
            auto desc = CreateDefault();
            desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
            return desc;
        }

    } // namespace DepthStencilPresets

    namespace SamplerPresets
    {
        D3D12_STATIC_SAMPLER_DESC CreateDefault()
        {
            return D3D12_STATIC_SAMPLER_DESC();
        }

        D3D12_STATIC_SAMPLER_DESC CreateShadowMap()
        {
            return D3D12_STATIC_SAMPLER_DESC();
        }
    }

    namespace PrimitiveTopologyPresets
    {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE Triangle()
        {
            D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            return topology;
        }

        D3D12_PRIMITIVE_TOPOLOGY_TYPE Line()
        {
            D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            return topology;
        }

        D3D12_PRIMITIVE_TOPOLOGY_TYPE Point()
        {
            D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            return topology;
        }

        D3D12_PRIMITIVE_TOPOLOGY_TYPE Patch()
        {
            D3D12_PRIMITIVE_TOPOLOGY_TYPE topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            return topology;
        }
    }
} // namespace YoRigine

