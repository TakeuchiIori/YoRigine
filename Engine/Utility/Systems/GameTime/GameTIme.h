#pragma once
#include "ObjectTime.h"
#include "HitStop.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <map>

class GameTime {
public:
    static GameTime* GetInstance() {
        static GameTime instance;
        return &instance;
    }

    void Initialize() {
        globalTime_ = 0.0f;
        globalTimeScale_ = 1.0f;
        isPaused_ = false;
        objectTimes_.clear();
        hitStop_ = HitStop::GetInstance();
    }

    void RegisterObject(const std::string& id) {
        objectTimes_[id] = std::make_unique<ObjectTime>();
        prevTimes_[id] = 0.0f;  // 前回の時間も初期化
    }

    void UnregisterObject(const std::string& id) {
        objectTimes_.erase(id);
    }

    void GameUpdate(float deltaTime) {
        const float DELTA_SPEED = 1.0f;  // 時間の進行速度（調整可能）

        // グローバル時間の更新
        globalTime_ += deltaTime * DELTA_SPEED;

        // 各オブジェクトの時間を更新
        for (auto& [id, objTime] : objectTimes_) {
            if (objTime) {
                // ヒットストップ中かチェック
                float timeScale = HitStop::GetInstance()->GetTimeScale(id);
                if (timeScale > 0.001f) {  // ヒットストップ中でなければ
                    objTime->Update(deltaTime * DELTA_SPEED);  // 時間を更新
                }
            }
        }
    }

    // オブジェクトの時間取得（存在しない場合は0.0fを返す）
    float GetObjectTime(const std::string& id) const {
        auto it = objectTimes_.find(id);
        if (it != objectTimes_.end() && it->second) {
            return it->second->GetTime();
        }
        return 0.0f;
    }

    // オブジェクトのデルタタイム取得
    float GetDeltaTime(const std::string& id) const {
        const float BASE_DELTA = 1.0f;  // 基準デルタタイム
        const float SPEED_MULTIPLIER = 1.0f;     // 速度倍率（調整可能）

        auto it = objectTimes_.find(id);
        if (it != objectTimes_.end() && it->second) {
            float timeScale = HitStop::GetInstance()->GetTimeScale(id);
            if (timeScale < 0.001f) {
                return 0.0f;  // ヒットストップ中
            }
            return BASE_DELTA * SPEED_MULTIPLIER;  // 通常時
        }
        return BASE_DELTA * SPEED_MULTIPLIER;
    }

    // グローバル時間関連
    float GetGlobalTime() const { return globalTime_; }
    void SetGlobalTimeScale(float scale) { globalTimeScale_ = scale; }
    float GetGlobalTimeScale() const { return globalTimeScale_; }

    // 一時停止関連
    void SetPaused(bool paused) { isPaused_ = paused; }
    bool IsPaused() const { return isPaused_; }

    // オブジェクト個別の一時停止
    void SetObjectPaused(const std::string& id, bool paused) {
        auto it = objectTimes_.find(id);
        if (it != objectTimes_.end() && it->second) {
            it->second->SetPaused(paused);
        }
    }

    // 指定したオブジェクトの時間をリセット
    void ResetObjectTime(const std::string& id) {
        auto it = objectTimes_.find(id);
        if (it != objectTimes_.end() && it->second) {
            it->second->Reset();
        }
    }

private:
    float globalTime_;      // グローバル時間
    float globalTimeScale_; // グローバル時間スケール
    bool isPaused_;         // グローバル一時停止フラグ
    mutable std::unordered_map<std::string, float> prevTimes_;  // mutableを追加
    HitStop* hitStop_;      // HitStopインスタンス
    std::unordered_map<std::string, std::unique_ptr<ObjectTime>> objectTimes_; // オブジェクト時間管理

    GameTime() = default;
    ~GameTime() = default;
    GameTime(const GameTime&) = delete;
    GameTime& operator=(const GameTime&) = delete;
};