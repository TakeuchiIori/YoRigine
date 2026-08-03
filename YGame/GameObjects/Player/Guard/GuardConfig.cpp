#include "GuardConfig.h"

#include <Debugger/Logger.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <json.hpp>

using json = nlohmann::json;

// ============================================================
// タイムラインの整合性を取る
// エディタでドラッグされた後に呼び、区間が反転したり
// active の外へパリィがはみ出したりしないようにする。
// ============================================================
void GuardTimeline::Sanitize() {
  fps = std::max(1, fps);
  startupFrames = std::max(0, startupFrames);
  activeFrames = std::max(1, activeFrames);
  recoveryFrames = std::max(0, recoveryFrames);

  // パリィは active の内側に収める
  parryStartFrame = std::clamp(parryStartFrame, 0, activeFrames - 1);
  parryEndFrame = std::clamp(parryEndFrame, parryStartFrame, activeFrames - 1);
}

// ============================================================
// 既定値
// 通常ガードは「受け止めて押される」、パリィは「弾いて相手を突き放す」。
// ここで押し合いの向きを反転させているのが両者の性格の違いになる。
// ============================================================
GuardConfig::GuardConfig() {
  // ── 通常ガード ──
  // ダメージは通すが大幅に軽減。CCを払い、自分が押し込まれる。
  guard.damageRate = 0.3f;
  guard.ccCost = 1;
  guard.ccRecover = 0;
  guard.hitStop = 0.05f;
  guard.hitStopEase = 0.0f;
  guard.shakeIntensity = 0.15f;
  guard.shakeDuration = 0.08f;
  guard.selfPushDistance = 0.45f;
  guard.selfPushDuration = 0.12f;
  guard.enemyPushPower = 0.0f;
  guard.enemyPushDuration = 0.0f;
  // 面方向へ広がりながら厚みが潰れる。受け止めた形。
  guard.shieldSquash = 0.45f;
  guard.shieldSquashTime = 0.22f;
  guard.shieldSquashAxis = {0.6f, 0.6f, -1.0f};
  guard.shieldSquashBounce = 1.6f;

  // ── パリィ ──
  // ダメージ完全無効。CCが回復し、自分は動かず相手を突き放す。
  parry.damageRate = 0.0f;
  parry.ccCost = 0;
  parry.ccRecover = 1;
  parry.hitStop = 0.12f;
  parry.hitStopEase = 0.05f;
  parry.shakeIntensity = 0.30f;
  parry.shakeDuration = 0.15f;
  parry.selfPushDistance = 0.0f;
  parry.selfPushDuration = 0.0f;
  parry.enemyPushPower = 6.0f;
  parry.enemyPushDuration = 0.25f;
  // パリィは弾き返した側なので、潰れるより外へ張り出す形にする。
  parry.shieldSquash = 0.35f;
  parry.shieldSquashTime = 0.18f;
  parry.shieldSquashAxis = {1.0f, 1.0f, 0.4f};
  parry.shieldSquashBounce = 2.0f;
}

namespace {

// GuardOutcome 1つ分の入出力。ガードとパリィで同じ形なので使い回す。
json OutcomeToJson(const GuardOutcome &o) {
  return json{
      {"damageRate", o.damageRate},
      {"ccCost", o.ccCost},
      {"ccRecover", o.ccRecover},
      {"hitStop", o.hitStop},
      {"hitStopEase", o.hitStopEase},
      {"shakeIntensity", o.shakeIntensity},
      {"shakeDuration", o.shakeDuration},
      {"selfPushDistance", o.selfPushDistance},
      {"selfPushDuration", o.selfPushDuration},
      {"enemyPushPower", o.enemyPushPower},
      {"enemyPushDuration", o.enemyPushDuration},
      {"shieldSquash", o.shieldSquash},
      {"shieldSquashTime", o.shieldSquashTime},
      {"shieldSquashAxisX", o.shieldSquashAxis.x},
      {"shieldSquashAxisY", o.shieldSquashAxis.y},
      {"shieldSquashAxisZ", o.shieldSquashAxis.z},
      {"shieldSquashBounce", o.shieldSquashBounce},
      {"vfxName", o.vfxName},
  };
}

// 未記載のキーは引数で渡した既定値（＝GuardConfig の初期値）を維持する
void OutcomeFromJson(const json &j, GuardOutcome &o) {
  o.damageRate = j.value("damageRate", o.damageRate);
  o.ccCost = j.value("ccCost", o.ccCost);
  o.ccRecover = j.value("ccRecover", o.ccRecover);
  o.hitStop = j.value("hitStop", o.hitStop);
  o.hitStopEase = j.value("hitStopEase", o.hitStopEase);
  o.shakeIntensity = j.value("shakeIntensity", o.shakeIntensity);
  o.shakeDuration = j.value("shakeDuration", o.shakeDuration);
  o.selfPushDistance = j.value("selfPushDistance", o.selfPushDistance);
  o.selfPushDuration = j.value("selfPushDuration", o.selfPushDuration);
  o.enemyPushPower = j.value("enemyPushPower", o.enemyPushPower);
  o.enemyPushDuration = j.value("enemyPushDuration", o.enemyPushDuration);
  o.shieldSquash = j.value("shieldSquash", o.shieldSquash);
  o.shieldSquashTime = j.value("shieldSquashTime", o.shieldSquashTime);
  o.shieldSquashAxis.x = j.value("shieldSquashAxisX", o.shieldSquashAxis.x);
  o.shieldSquashAxis.y = j.value("shieldSquashAxisY", o.shieldSquashAxis.y);
  o.shieldSquashAxis.z = j.value("shieldSquashAxisZ", o.shieldSquashAxis.z);
  o.shieldSquashBounce = j.value("shieldSquashBounce", o.shieldSquashBounce);
  o.vfxName = j.value("vfxName", o.vfxName);
}

} // namespace

// ============================================================
// 読み込み
// ============================================================
bool GuardConfigIO::Load(const std::string &path, GuardConfig &out) {
  std::ifstream ifs(path);
  if (!ifs.is_open()) {
    Logger(
        ("[GuardConfig] 設定ファイルが無いので既定値を使います: " + path + "\n")
            .c_str());
    return false;
  }

  try {
    json j = json::parse(ifs);

    if (j.contains("timeline")) {
      const auto &t = j["timeline"];
      auto &dst = out.timeline;
      dst.fps = t.value("fps", dst.fps);
      dst.startupFrames = t.value("startupFrames", dst.startupFrames);
      dst.activeFrames = t.value("activeFrames", dst.activeFrames);
      dst.parryStartFrame = t.value("parryStartFrame", dst.parryStartFrame);
      dst.parryEndFrame = t.value("parryEndFrame", dst.parryEndFrame);
      dst.recoveryFrames = t.value("recoveryFrames", dst.recoveryFrames);
    }

    if (j.contains("guard"))
      OutcomeFromJson(j["guard"], out.guard);
    if (j.contains("parry"))
      OutcomeFromJson(j["parry"], out.parry);

    out.frontHalfAngleDeg = j.value("frontHalfAngleDeg", out.frontHalfAngleDeg);
    out.timeline.Sanitize();

    Logger(("[GuardConfig] 読み込み完了: " + path + "\n").c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[GuardConfig] 読み込みに失敗しました: " + std::string(e.what()) +
            "\n")
               .c_str());
    return false;
  }
}

// ============================================================
// 書き出し
// ============================================================
bool GuardConfigIO::Save(const std::string &path, const GuardConfig &config) {
  json j;
  j["timeline"] = {
      {"fps", config.timeline.fps},
      {"startupFrames", config.timeline.startupFrames},
      {"activeFrames", config.timeline.activeFrames},
      {"parryStartFrame", config.timeline.parryStartFrame},
      {"parryEndFrame", config.timeline.parryEndFrame},
      {"recoveryFrames", config.timeline.recoveryFrames},
  };
  j["guard"] = OutcomeToJson(config.guard);
  j["parry"] = OutcomeToJson(config.parry);
  j["frontHalfAngleDeg"] = config.frontHalfAngleDeg;

  try {
    // 保存先フォルダが無いこともあるので作ってから書く
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
      std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      Logger(("[GuardConfig] 保存用ファイルを開けませんでした: " + path + "\n")
                 .c_str());
      return false;
    }
    ofs << std::setw(4) << j;
    Logger(("[GuardConfig] 保存完了: " + path + "\n").c_str());
    return true;
  } catch (const std::exception &e) {
    Logger(("[GuardConfig] 保存に失敗しました: " + std::string(e.what()) + "\n")
               .c_str());
    return false;
  }
}
