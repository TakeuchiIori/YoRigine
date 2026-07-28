#pragma once

// Math
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

// C++
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace YoRigine {

class Camera;

///************************* 対象の指定 *************************///

// 穴を開ける対象の種類。
enum class TutorialSpotlightTargetKind {
  Ui,    // UIManager に登録された UI の矩形
  Rect,  // 画面座標で直接指定した矩形
  World, // 登録済みのワールド座標をカメラで投影した矩形
};

struct TutorialSpotlightTarget {
  TutorialSpotlightTargetKind kind = TutorialSpotlightTargetKind::Ui;
  std::string id;                 // Ui: UIのID / World: 登録名
  Vector2 center{800.0f, 450.0f}; // Rect のときの中心（画面座標）
  Vector2 size{200.0f, 100.0f};   // Rect のときの大きさ
  float radius = 1.0f;            // World のときの半径（ワールド単位）
};

struct TutorialSpotlightConfig {
  bool enabled = false;
  std::vector<TutorialSpotlightTarget> targets;
  Vector4 dimColor{0.0f, 0.0f, 0.0f, 0.65f};
  float padding = 12.0f;     // 穴を対象より少し広げる余白（ピクセル）
  float fadeSeconds = 0.25f; // 暗転のフェードイン時間
};

///************************* スポットライト *************************///

// 注目させたい場所以外を暗幕で覆う仕組み。
//
// 暗幕を1枚のくり抜き画像で作るのではなく、「穴を避けた矩形の集合」へ
// 分解して敷き詰める。そのため穴の部分には何も描かれず、
// 下にあるUIも3Dの画もそのまま明るく残る。シェーダは不要。
//
// 穴を矩形として抽象化しているので、UI・画面座標・ワールド座標のどれでも
// 同じ経路で扱える。
class TutorialSpotlight {
public:
  ///************************* 基本関数 *************************///

  static TutorialSpotlight *GetInstance();

  // 設定を適用する。layer は暗幕を敷く描画レイヤー。
  // 穴の下にあるものを見せたいので、説明パネルより1つ下を指定する。
  void Apply(const TutorialSpotlightConfig &config, int layer);

  // 暗幕を即座に消す。フェードアウトはしない。
  void Clear();

  // 毎フレーム呼ぶ。フェードと、動く対象への矩形追従を行う。
  void Update(float deltaTime);

  bool IsActive() const { return active_; }

  ///************************* ワールド対象 *************************///

  // ワールド座標の対象を投影するためのカメラ。
  // 未設定の場合、World 種別の対象は無視される（暗幕は他の穴だけで作られる）。
  void SetCamera(Camera *camera) { camera_ = camera; }
  Camera *GetCamera() const { return camera_; }

  // ワールド座標の対象を名前で登録する。
  // 位置を関数で受け取るため、動き回る敵にもそのまま追従できる。
  void RegisterWorldTarget(const std::string &name,
                           std::function<Vector3()> provider);
  void UnregisterWorldTarget(const std::string &name);
  void ClearWorldTargets();

  // 登録済みの名前一覧。エディタの候補表示に使う。
  std::vector<std::string> GetWorldTargetNames() const;

private:
  ///************************* 内部構造 *************************///

  TutorialSpotlight() = default;
  ~TutorialSpotlight() = default;
  TutorialSpotlight(const TutorialSpotlight &) = delete;
  TutorialSpotlight &operator=(const TutorialSpotlight &) = delete;

  struct ScreenRect {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
  };

  // 対象1つを画面矩形へ変換する。変換できない場合は false。
  bool BuildTargetRect(const TutorialSpotlightTarget &target,
                       ScreenRect &out) const;

  // 穴の一覧から暗幕の矩形群を組み立てて UI へ反映する。
  void RebuildPanels();

  // index 以降の暗幕スプライトを隠す。
  void HidePanelsFrom(std::size_t index);

  ///************************* メンバ変数 *************************///

  TutorialSpotlightConfig config_;
  Camera *camera_ = nullptr;
  std::unordered_map<std::string, std::function<Vector3()>> worldTargets_;

  std::size_t activePanelCount_ = 0;
  int layer_ = 0;
  float opacity_ = 0.0f;
  bool active_ = false;
};

} // namespace YoRigine
