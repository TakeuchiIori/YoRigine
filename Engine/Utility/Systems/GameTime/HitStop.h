// HitStop.h
#pragma once
#include <string>
#include <unordered_map>

class HitStop {
public:
    static HitStop* GetInstance() {
        static HitStop instance;
        return &instance;
    }

    // ヒットストップの種類を定義
    enum class HitStopType {
        None,           // ヒットストップなし
        Light,          // 軽い攻撃のヒットストップ
        Medium,         // 中程度の攻撃のヒットストップ
        Heavy,          // 強い攻撃のヒットストップ
        Custom          // カスタム設定のヒットストップ
    };

    // ヒットストップを開始（種類を指定）
    void Start(const std::string& targetID, HitStopType type) {
        float duration = 0.0f;
        switch (type) {
        case HitStopType::Light:
            duration = 0.05f;
            break;
        case HitStopType::Medium:
            duration = 0.1f;
            break;
        case HitStopType::Heavy:
            duration = 0.2f;
            break;
        case HitStopType::None:
            return; // ヒットストップなしの場合は何もしない
        default:
            return;
        }
        StartWithDuration(targetID, duration);
    }

    // カスタム時間でヒットストップを開始
    void StartWithDuration(const std::string& targetID, float duration) {
        HitStopData data;
        data.duration = duration;
        data.currentTime = 0.0f;
        data.isActive = true;
        data.timeScale = 0.0f; // 完全停止
        hitStopData_[targetID] = data;
    }

    void Stop(const std::string& targetID) {
        auto it = hitStopData_.find(targetID);
        if (it != hitStopData_.end()) {
            it->second.isActive = false;
        }
    }

    void StopAll() {
        for (auto& [id, data] : hitStopData_) {
            data.isActive = false;
        }
    }

    void Update(float deltaTime) {
        for (auto& [id, data] : hitStopData_) {
            if (data.isActive) {
                data.currentTime += deltaTime;
                if (data.currentTime >= data.duration) {
                    data.isActive = false;
                }
            }
        }
    }

    bool IsActive(const std::string& targetID) const {
        auto it = hitStopData_.find(targetID);
        return it != hitStopData_.end() && it->second.isActive;
    }

    float GetTimeScale(const std::string& targetID) const {
        auto it = hitStopData_.find(targetID);
        if (it != hitStopData_.end() && it->second.isActive) {
            return 0.0f;  // ヒットストップ中
        }
        return 1.0f;  // 通常時
    }

private:
    struct HitStopData {
        float duration = 0.0f;    // ヒットストップの継続時間
        float currentTime = 0.0f; // 経過時間
        float timeScale = 1.0f;   // 時間スケール（0=停止、1=通常）
        bool isActive = false;    // アクティブフラグ
    };

    std::unordered_map<std::string, HitStopData> hitStopData_;

    HitStop() = default;
    ~HitStop() = default;
    HitStop(const HitStop&) = delete;
    HitStop& operator=(const HitStop&) = delete;
};