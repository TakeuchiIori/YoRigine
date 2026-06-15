#include "AttackCameraComponent.h"
#include "Systems/Camera/Virtuals/FollowCamera/FollowCamera.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <json.hpp>
#include <Easing.h>
#include <Debugger/Logger.h>

// ============================================================
// 初期化
// ============================================================
void AttackCameraComponent::Initialize() {
    phase_ = Phase::Idle;
    playTimer_ = 0.0f;
    returnTimer_ = 0.0f;
    returnDuration_ = 0.2f;
    startInterpolationTimer_ = 0.0f;
    startInterpolationDuration_ = 0.2f;
    ResetValues();
}

// ============================================================
// タイマー前進 + シェイクトリガー
// ============================================================
void AttackCameraComponent::UpdatePre(FollowCamera* followCamera, float dt) {
    if (phase_ != Phase::Playing) return;

    const AttackCameraWork* work = FindWork(currentWorkName_);
    if (!work) { phase_ = Phase::Idle; return; }

    playTimer_ += dt;

    // シェイクは FollowCamera 経由でトリガー（次フレームの FollowProcess で適用）
    for (size_t i = 0; i < work->keyframes.size() && i < shakeTriggered_.size(); ++i) {
        if (!shakeTriggered_[i] && playTimer_ >= work->keyframes[i].time) {
            shakeTriggered_[i] = true;
            if (followCamera && work->keyframes[i].shakeIntensity > 0.0f) {
                followCamera->StartShake(work->keyframes[i].shakeIntensity,
                    work->keyframes[i].shakeDuration);
            }
        }
    }
}

// ============================================================
// 補間値をサンプリング＆フェーズ進行
// ============================================================
void AttackCameraComponent::UpdatePost(float dt) {
    switch (phase_) {

        // ----------------------------------------------------------
    case Phase::Idle:
        ResetValues();
        break;

        // ----------------------------------------------------------
    case Phase::StartInterpolating: {
        startInterpolationTimer_ += dt;
        float t = (startInterpolationDuration_ > 0.0f)
            ? std::clamp(startInterpolationTimer_ / startInterpolationDuration_, 0.0f, 1.0f)
            : 1.0f;

        const AttackCameraWork* work = FindWork(currentWorkName_);
        if (!work) { phase_ = Phase::Idle; ResetValues(); break; }

        // 開始時補間の目標値をサンプリング
        SampleKeyframes(*work, 0.0f);

        // イーズアウトで補間
		float ease = Easing::EaseOutQuad(t);
        currentPosOffset_ = Lerp({}, currentPosOffset_, ease);
        currentRotOffset_ = Lerp({}, currentRotOffset_, ease);
        currentFovDelta_ =  Lerp(0.0f, currentFovDelta_, ease);
        currentTimeScale_ = Lerp(1.0f, currentTimeScale_, ease);

        if (t >= 1.0f) {
            phase_ = Phase::Playing;
            playTimer_ = 0.0f;
        }
        break;
    }

                                  // ----------------------------------------------------------
    case Phase::Playing: {
        const AttackCameraWork* work = FindWork(currentWorkName_);
        if (!work) { phase_ = Phase::Idle; ResetValues(); break; }

        SampleKeyframes(*work, playTimer_);

        if (playTimer_ >= work->totalDuration) {
            if (work->resetOnFinish) {
                EnterReturning(*work);
            }
            else {
                phase_ = Phase::Idle;
            }
        }
        break;
    }

                       // ----------------------------------------------------------
    case Phase::Returning: {
        returnTimer_ += dt;
        float t = (returnDuration_ > 0.0f)
            ? std::clamp(returnTimer_ / returnDuration_, 0.0f, 1.0f)
            : 1.0f;

        // イーズアウト（減速しながら戻る）
        float ease = Easing::EaseOutQuad(t);

        currentPosOffset_ = Lerp(returnFromPos_, {}, ease);
        currentRotOffset_ = Lerp(returnFromRot_, {}, ease);
        currentFovDelta_ =  Lerp(returnFromFov_, 0.0f, ease);
        currentTimeScale_ = Lerp(returnFromTs_, 1.0f, ease);

        if (t >= 1.0f) {
            phase_ = Phase::Idle;
            ResetValues();
        }
        break;
    }
    }
}

// ============================================================
// 再生開始
// ============================================================
void AttackCameraComponent::Play(const std::string& workName) {
    const AttackCameraWork* work = FindWork(workName);
    if (!work) return;

    currentWorkName_ = workName;
    shakeTriggered_.assign(work->keyframes.size(), false);
    ResetValues();

    // 開始時補間を使用するかで分岐
    if (work->useStartInterpolation) {
        EnterStartInterpolation(*work);
    }
    else {
        phase_ = Phase::Playing;
        playTimer_ = 0.0f;
    }
}

// ============================================================
// 即時停止
// ============================================================
void AttackCameraComponent::Stop(FollowCamera* /*camera*/) {
    phase_ = Phase::Idle;
    ResetValues();
}

// ============================================================
// 再生進捗比率 (0.0 ~ 1.0)
// ============================================================
float AttackCameraComponent::GetPlayRatio() const {
    if (phase_ == Phase::Returning) return 1.0f;
    if (phase_ == Phase::Playing) {
        const AttackCameraWork* work = FindWork(currentWorkName_);
        if (!work || work->totalDuration <= 0.0f) return 0.0f;
        return std::clamp(playTimer_ / work->totalDuration, 0.0f, 1.0f);
    }
    return 0.0f;
}

// ============================================================
// 現在ワークのフレーム保持設定
// ============================================================
bool AttackCameraComponent::ShouldKeepPlayerInFrame() const {
    const AttackCameraWork* work = FindWork(currentWorkName_);
    // 該当ワークが見つからない場合は安全側（保持する）に倒す
    return work ? work->keepPlayerInFrame : true;
}

// ============================================================
// データ管理
// ============================================================
void AttackCameraComponent::AddWork(const AttackCameraWork& work) {
    auto it = std::find_if(works_.begin(), works_.end(),
        [&](const AttackCameraWork& w) { return w.name == work.name; });
    if (it != works_.end()) *it = work;
    else                    works_.push_back(work);
}

void AttackCameraComponent::RemoveWork(const std::string& name) {
    works_.erase(
        std::remove_if(works_.begin(), works_.end(),
            [&](const AttackCameraWork& w) { return w.name == name; }),
        works_.end());
}

AttackCameraWork* AttackCameraComponent::FindWork(const std::string& name) {
    for (auto& w : works_) if (w.name == name) return &w;
    return nullptr;
}

const AttackCameraWork* AttackCameraComponent::FindWork(const std::string& name) const {
    for (const auto& w : works_) if (w.name == name) return &w;
    return nullptr;
}

// ============================================================
// ファイル保存
// ============================================================
void AttackCameraComponent::SaveToFile(const std::string& filePath) const {
    try {
        nlohmann::json j;
        j["works"] = nlohmann::json::array();
        for (const auto& w : works_) {
            nlohmann::json wj;
            w.Save(wj);
            j["works"].push_back(wj);
        }
        std::ofstream ofs(filePath);
        ofs << j.dump(4);
        std::string msg = "[AttackCameraComponent] Saved: " + filePath + "\n";
		Logger(msg);
    }
    catch (const std::exception& e) {
        Logger("[AttackCameraComponent] SaveToFile error: " + std::string(e.what()) + "\n");
    }
}

// ============================================================
// ファイル読み込み
// ============================================================
void AttackCameraComponent::LoadFromFile(const std::string& filePath) {
    try {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) return;

        nlohmann::json j;
        ifs >> j;
        works_.clear();
        if (j.contains("works")) {
            for (const auto& wj : j["works"]) {
                AttackCameraWork w;
                w.Load(wj);
                works_.push_back(std::move(w));
            }
        }
        std::cout << "[AttackCameraComponent] Loaded: " << filePath
            << " (" << works_.size() << " works)\n";
    }
    catch (const std::exception& e) {
        std::cout << "[AttackCameraComponent] LoadFromFile error: " << e.what() << "\n";
    }
}

// ============================================================
// 開始時補間フェーズへ移行
// ============================================================
void AttackCameraComponent::EnterStartInterpolation(const AttackCameraWork& work) {
    startInterpolationTimer_ = 0.0f;
    startInterpolationDuration_ = work.startInterpolationDuration;
    phase_ = Phase::StartInterpolating;
}

// ============================================================
// 終了時補間フェーズへ移行
// ============================================================
void AttackCameraComponent::EnterReturning(const AttackCameraWork& work) {
    if (!work.useReturnInterpolation || work.returnDuration <= 0.0f) {
        // 瞬間切り替え or 補間なし
        phase_ = Phase::Idle;
        ResetValues();
        return;
    }

    returnFromPos_ = currentPosOffset_;
    returnFromRot_ = currentRotOffset_;
    returnFromFov_ = currentFovDelta_;
    returnFromTs_ = currentTimeScale_;
    returnTimer_ = 0.0f;
    returnDuration_ = work.returnDuration;
    phase_ = Phase::Returning;
}

// ============================================================
// 現在値をデフォルト（オフセット 0）にリセット
// ============================================================
void AttackCameraComponent::ResetValues() {
    currentPosOffset_ = {};
    currentRotOffset_ = {};
    currentFovDelta_ = 0.0f;
    currentTimeScale_ = 1.0f;
}

// ============================================================
// ワーク名一覧取得
// ============================================================
std::vector<std::string> AttackCameraComponent::GetWorkNames() const {
    std::vector<std::string> names;
    names.reserve(works_.size());
    for (const auto& work : works_) {
        names.push_back(work.name);
    }
    return names;
}
// ============================================================
// キーフレーム線形補間
// ============================================================
void AttackCameraComponent::SampleKeyframes(const AttackCameraWork& work, float t) {
    const auto& kfs = work.keyframes;
    if (kfs.empty()) { ResetValues(); return; }

    if (t <= kfs.front().time) {
        currentPosOffset_ = kfs.front().posOffset;
        currentRotOffset_ = kfs.front().rotOffset;
        currentFovDelta_ = kfs.front().fovDelta;
        currentTimeScale_ = kfs.front().timeScale;
        return;
    }
    if (t >= kfs.back().time) {
        currentPosOffset_ = kfs.back().posOffset;
        currentRotOffset_ = kfs.back().rotOffset;
        currentFovDelta_ = kfs.back().fovDelta;
        currentTimeScale_ = kfs.back().timeScale;
        return;
    }

    for (size_t i = 0; i + 1 < kfs.size(); ++i) {
        if (t >= kfs[i].time && t < kfs[i + 1].time) {
            float span = kfs[i + 1].time - kfs[i].time;
            float alpha = (span > 0.0f) ? (t - kfs[i].time) / span : 0.0f;

            currentPosOffset_ = Lerp(kfs[i].posOffset, kfs[i + 1].posOffset, alpha);
            currentRotOffset_ = Lerp(kfs[i].rotOffset, kfs[i + 1].rotOffset, alpha);
            currentFovDelta_ = kfs[i].fovDelta + (kfs[i + 1].fovDelta - kfs[i].fovDelta) * alpha;
            currentTimeScale_ = kfs[i].timeScale + (kfs[i + 1].timeScale - kfs[i].timeScale) * alpha;
            return;
        }
    }
}