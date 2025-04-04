#pragma once
// C++
#include <stdint.h>
#include <cstdlib>
#include <wrl.h>
#include <memory>
#include <map>
#include <d3d12.h>
#include <vector>



class OcclusionCullingManager {
public:
    static OcclusionCullingManager* GetInstance() {
        static OcclusionCullingManager instance;
        return &instance;
    }

    UINT AddOcclusionQuery() {
        UINT newIndex = currentIndex_++;
        if (newIndex >= queryCount_) {
            RebuildResources(newIndex + 1); // ⬅ 必ず対応
        }
        // フラグやバッファの安全拡張
        queriedFlags_.resize(queryCount_, false);
        invisibleCounts_.resize(queryCount_, 0);
        occlusionResults_.resize(queryCount_, 0);
        return newIndex;
    }


    void Initialize();
    
    void BeginOcclusionQuery(UINT queryIndex);

    void EndOcclusionQuery(UINT queryIndex);

    void ResolvedOcclusionQuery();

    void RebuildResources(UINT newCount);

    UINT64 GetQueryResult(uint32_t index) const {
        if (index < occlusionResults_.size() && queriedFlags_[index]) {
            return occlusionResults_[index];
        }
        return 1; // デフォルト：表示されてる扱い
    }

    bool ShouldDraw(uint32_t index, uint32_t maxSkipFrames = 1) const {
        if (index >= invisibleCounts_.size()) return true;

        // 一度もクエリが実行されてない → 表示する（初期フレーム想定）
        if (!queriedFlags_[index]) return true;

        // クエリされた上で、連続不可視フレームが閾値以内なら表示
        return invisibleCounts_[index] <= maxSkipFrames;
    }



private:
    OcclusionCullingManager() = default;
    ~OcclusionCullingManager() = default;
    OcclusionCullingManager(const OcclusionCullingManager&) = delete;
    OcclusionCullingManager& operator=(const OcclusionCullingManager&) = delete;


    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queryHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> queryResultBuffer_;
    std::vector<bool> queriedFlags_;
    std::vector<uint32_t> invisibleCounts_;
    std::vector<UINT64> occlusionResults_;                          // オブジェクトごとに結果を保持
    UINT queryCount_ = 0;                                           // オクルージョンクエリの数
    UINT currentIndex_ = 0;
    ID3D12GraphicsCommandList* commandList_;
};
