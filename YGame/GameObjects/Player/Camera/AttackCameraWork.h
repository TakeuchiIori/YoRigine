#pragma once
#include "Vector3.h"
#include <string>
#include <vector>
#include <json.hpp>

// ============================================================
// 1 キーフレーム
// ============================================================
struct AttackCameraKeyframe {
    float   time            = 0.0f;  // 発火時刻（秒）

    // ---- 位置 / 方向 ----
    Vector3 posOffset       = {};    // カメラローカル空間での位置追加オフセット
    Vector3 rotOffset       = {};    // 回転追加量（pitch, yaw, roll）ラジアン
    float   fovDelta        = 0.0f;  // 基準 FOV からの差分（ラジアン）

    // ---- シェイク ----
    float   shakeIntensity  = 0.0f;  // シェイク強さ（0 = なし）
    float   shakeDuration   = 0.0f;  // シェイク長さ（秒）

    // ---- タイムスケール ----
    float   timeScale       = 1.0f;  // ゲーム速度倍率（1.0 = 通常、0.3 = スロー）

    void Save(nlohmann::json& j) const {
        j["time"]           = time;
        j["posOffset"]      = { posOffset.x, posOffset.y, posOffset.z };
        j["rotOffset"]      = { rotOffset.x, rotOffset.y, rotOffset.z };
        j["fovDelta"]       = fovDelta;
        j["shakeIntensity"] = shakeIntensity;
        j["shakeDuration"]  = shakeDuration;
        j["timeScale"]      = timeScale;
    }
    void Load(const nlohmann::json& j) {
        time           = j.value("time",           0.0f);
        fovDelta       = j.value("fovDelta",       0.0f);
        shakeIntensity = j.value("shakeIntensity", 0.0f);
        shakeDuration  = j.value("shakeDuration",  0.0f);
        timeScale      = j.value("timeScale",      1.0f);
        if (j.contains("posOffset"))
            posOffset = { j["posOffset"][0], j["posOffset"][1], j["posOffset"][2] };
        if (j.contains("rotOffset"))
            rotOffset = { j["rotOffset"][0], j["rotOffset"][1], j["rotOffset"][2] };
    }
};

// ============================================================
// 1 攻撃に対応するカメラワーク全体
// ============================================================
struct AttackCameraWork {
    std::string name;
    float       totalDuration  = 0.5f;
    bool        useStartInterpolation      = false; // 再生開始時に Idle -> Playing を補間するか
    float       startInterpolationDuration = 0.1f;  // 開始時補間時間（0 = 瞬間切り替え）
    bool        useReturnInterpolation     = true;  // 再生終了時に補間で戻るか
    float       returnDuration = 0.2f;   // 演出終了後に元の状態へ補間で戻る時間（0 = 瞬間切り替え）
    bool        resetOnFinish  = true;   // 終了後にオフセットをリセットするか
    std::vector<AttackCameraKeyframe> keyframes;

    void Save(nlohmann::json& j) const {
        j["name"]           = name;
        j["totalDuration"]  = totalDuration;
        j["useStartInterpolation"]      = useStartInterpolation;
        j["startInterpolationDuration"] = startInterpolationDuration;
        j["useReturnInterpolation"]     = useReturnInterpolation;
        j["returnDuration"] = returnDuration;
        j["resetOnFinish"]  = resetOnFinish;
        j["keyframes"]     = nlohmann::json::array();
        for (const auto& kf : keyframes) {
            nlohmann::json kfj;
            kf.Save(kfj);
            j["keyframes"].push_back(kfj);
        }
    }
    void Load(const nlohmann::json& j) {
        name           = j.value("name",           "");
        totalDuration  = j.value("totalDuration",  0.5f);
        useStartInterpolation      = j.value("useStartInterpolation",      false);
        startInterpolationDuration = j.value("startInterpolationDuration", 0.1f);
        useReturnInterpolation     = j.value("useReturnInterpolation",     true);
        returnDuration = j.value("returnDuration", 0.2f);
        resetOnFinish  = j.value("resetOnFinish",  true);
        keyframes.clear();
        if (j.contains("keyframes")) {
            for (const auto& kfj : j["keyframes"]) {
                AttackCameraKeyframe kf;
                kf.Load(kfj);
                keyframes.push_back(kf);
            }
        }
    }
};
