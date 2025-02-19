#pragma once

class ObjectTime {
public:
    ObjectTime() : time_(0.0f), timeScale_(1.0f), isPaused_(false) {}

    void Update(float deltaTime) {
        if (!isPaused_) {
            time_ += deltaTime * timeScale_;
        }
    }

    float GetTime() const { return time_; }
    float GetTimeScale() const { return timeScale_; }
    bool IsPaused() const { return isPaused_; }

    void SetTimeScale(float scale) { timeScale_ = scale; }
    void SetPaused(bool paused) { isPaused_ = paused; }
    void Reset() { time_ = 0.0f; }

private:
    float time_;      // オブジェクトの経過時間
    float timeScale_; // 時間スケール（1.0が通常速度）
    bool isPaused_;   // 一時停止フラグ
};
