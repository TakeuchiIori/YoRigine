#pragma once
#include "Vector3.h"
#include <string>
#include <vector>
#include <json.hpp>

// ============================================================
// 参照フレーム：posOffset をどの空間の軸で解釈するか
//   CameraLocal    … カメラの向き基準（従来の挙動・既定）
//   PlayerLocal    … プレイヤーの向き基準（背中側に引く 等）
//   World          … ワールド絶対軸（+X 右 / +Y 上 / +Z 奥）
//   TargetRelative … プレイヤー→注視対象 の方向を奥(+Z)とした基準
//                     （攻撃方向／対象方向に回り込む。対象が無ければ PlayerLocal に退避）
// ============================================================
enum class CameraSpace { CameraLocal, PlayerLocal, World, TargetRelative };

// ============================================================
// 注視対象：lookAtWeight>0 のとき、カメラ回転をこの対象へ向けて混ぜる
//   None        … 注視しない（rotOffset 加算のみ・従来の挙動）
//   LockedEnemy … ロックオン対象（無ければ Anchor→無効）
//   Player      … プレイヤーピボット
//   Midpoint    … プレイヤーと対象の中点
//   Anchor      … Play 時に渡した任意座標（ヒット点など）
// ============================================================
enum class LookAtTarget { None, LockedEnemy, Player, Midpoint, Anchor };

// ============================================================
// 1 キーフレーム
// ============================================================
struct AttackCameraKeyframe {
    float   time = 0.0f;  // 発火時刻（秒）

    // ---- 位置 / 方向 ----
    Vector3 posOffset = {};    // 参照フレーム空間での位置追加オフセット
    Vector3 rotOffset = {};    // 回転追加量（pitch, yaw, roll）ラジアン。注視適用後に加算
    float   fovDelta = 0.0f;  // 基準 FOV からの差分（ラジアン）

    // ---- 注視ブレンド ----
    float   lookAtWeight = 0.0f;  // 注視対象へ向ける強さ（0=向けない / 1=完全に対象へ）

    // ---- シェイク ----
    float   shakeIntensity = 0.0f;  // シェイク強さ（0 = なし）
    float   shakeDuration = 0.0f;  // シェイク長さ（秒）

    // ---- タイムスケール ----
    float   timeScale = 1.0f;  // ゲーム速度倍率（1.0 = 通常、0.3 = スロー）

    void Save(nlohmann::json& j) const {
        j["time"] = time;
        j["posOffset"] = { posOffset.x, posOffset.y, posOffset.z };
        j["rotOffset"] = { rotOffset.x, rotOffset.y, rotOffset.z };
        j["fovDelta"] = fovDelta;
        j["lookAtWeight"] = lookAtWeight;
        j["shakeIntensity"] = shakeIntensity;
        j["shakeDuration"] = shakeDuration;
        j["timeScale"] = timeScale;
    }
    void Load(const nlohmann::json& j) {
        time = j.value("time", 0.0f);
        fovDelta = j.value("fovDelta", 0.0f);
        lookAtWeight = j.value("lookAtWeight", 0.0f);
        shakeIntensity = j.value("shakeIntensity", 0.0f);
        shakeDuration = j.value("shakeDuration", 0.0f);
        timeScale = j.value("timeScale", 1.0f);
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
    float       totalDuration = 0.5f;
    float       returnDuration = 0.2f;     // 演出終了後に元の状態へ補間で戻る時間（0 = 瞬間切り替え）
    bool        resetOnFinish = true;     // 終了後にオフセットをリセットするか
    bool        keepPlayerInFrame = true;     // 再生中、プレイヤーが画角外へ出ないよう最終ガードで引き戻すか（false = カットシーン的に画角外を許可）

    // ============================================================
    // 参照フレーム / 注視（このワーク全体に適用）
    // ============================================================
    CameraSpace  posSpace = CameraSpace::CameraLocal;   // posOffset の解釈空間
    LookAtTarget lookAt   = LookAtTarget::None;         // キーフレームの lookAtWeight が向く対象

    // ============================================================
    // 補間設定
    // ============================================================
    bool        useStartInterpolation = true;   // 開始時の補間を使用するか
    float       startInterpolationDuration = 0.2f;  // 開始時補間の時間
    bool        useReturnInterpolation = true;    // 終了時の補間を使用するか

    std::vector<AttackCameraKeyframe> keyframes;

    void Save(nlohmann::json& j) const {
        j["name"] = name;
        j["totalDuration"] = totalDuration;
        j["returnDuration"] = returnDuration;
        j["resetOnFinish"] = resetOnFinish;
        j["keepPlayerInFrame"] = keepPlayerInFrame;
        j["posSpace"] = static_cast<int>(posSpace);
        j["lookAt"] = static_cast<int>(lookAt);
        j["useStartInterpolation"] = useStartInterpolation;
        j["startInterpolationDuration"] = startInterpolationDuration;
        j["useReturnInterpolation"] = useReturnInterpolation;
        j["keyframes"] = nlohmann::json::array();
        for (const auto& kf : keyframes) {
            nlohmann::json kfj;
            kf.Save(kfj);
            j["keyframes"].push_back(kfj);
        }
    }
    void Load(const nlohmann::json& j) {
        name = j.value("name", "");
        totalDuration = j.value("totalDuration", 0.5f);
        returnDuration = j.value("returnDuration", 0.2f);
        resetOnFinish = j.value("resetOnFinish", true);
        keepPlayerInFrame = j.value("keepPlayerInFrame", true);
        posSpace = static_cast<CameraSpace>(j.value("posSpace", static_cast<int>(CameraSpace::CameraLocal)));
        lookAt   = static_cast<LookAtTarget>(j.value("lookAt", static_cast<int>(LookAtTarget::None)));
        useStartInterpolation = j.value("useStartInterpolation", false);
        startInterpolationDuration = j.value("startInterpolationDuration", 0.2f);
        useReturnInterpolation = j.value("useReturnInterpolation", true);
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