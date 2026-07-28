#include "TutorialSpotlight.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Systems/Camera/Camera.h"
#include "Systems/UI/UIBase.h"
#include "Systems/UI/UIManager.h"
#include "WinApp./WinApp.h"

namespace {

// 矩形の潰れ判定に使う許容値（ピクセル）。
constexpr float kEpsilon = 0.5f;

constexpr const char *kPanelPrefix = "__TutorialSpotlight_";
constexpr const char *kPanelTexture = "./Resources/images/white.png";

std::string PanelId(std::size_t index) {
  return std::string(kPanelPrefix) + std::to_string(index);
}

// ワールド座標を画面座標へ投影する。カメラの後ろにある場合は false。
bool ProjectToScreen(const Vector3 &world, const Matrix4x4 &viewProjection,
                     float screenWidth, float screenHeight, Vector2 &out) {
  const float x = world.x * viewProjection.m[0][0] +
                  world.y * viewProjection.m[1][0] +
                  world.z * viewProjection.m[2][0] + viewProjection.m[3][0];
  const float y = world.x * viewProjection.m[0][1] +
                  world.y * viewProjection.m[1][1] +
                  world.z * viewProjection.m[2][1] + viewProjection.m[3][1];
  const float w = world.x * viewProjection.m[0][3] +
                  world.y * viewProjection.m[1][3] +
                  world.z * viewProjection.m[2][3] + viewProjection.m[3][3];

  // w が 0 以下 = カメラの後ろ。割ると座標が反転して暴れるので弾く。
  if (w <= 1e-4f)
    return false;

  out.x = (x / w * 0.5f + 0.5f) * screenWidth;
  out.y = (0.5f - y / w * 0.5f) * screenHeight;
  return true;
}

} // namespace

namespace YoRigine {

///************************* 基本関数 *************************///

TutorialSpotlight *TutorialSpotlight::GetInstance() {
  static TutorialSpotlight instance;
  return &instance;
}

void TutorialSpotlight::Apply(const TutorialSpotlightConfig &config,
                              int layer) {
  if (!config.enabled || config.targets.empty()) {
    Clear();
    return;
  }

  config_ = config;
  layer_ = layer;
  active_ = true;
  opacity_ = (config_.fadeSeconds > 0.0f) ? 0.0f : 1.0f;
  RebuildPanels();
}

void TutorialSpotlight::Clear() {
  HidePanelsFrom(0);
  activePanelCount_ = 0;
  active_ = false;
  opacity_ = 0.0f;
  config_ = TutorialSpotlightConfig{};
}

void TutorialSpotlight::Update(float deltaTime) {
  if (!active_)
    return;

  if (opacity_ < 1.0f) {
    const float duration = (std::max)(0.0f, config_.fadeSeconds);
    opacity_ = (duration > 0.0f)
                   ? std::clamp(opacity_ + deltaTime / duration, 0.0f, 1.0f)
                   : 1.0f;
  }

  // 動くワールド対象へ追従させるため、矩形は毎フレーム組み直す。
  RebuildPanels();
}

///************************* ワールド対象 *************************///

void TutorialSpotlight::RegisterWorldTarget(const std::string &name,
                                            std::function<Vector3()> provider) {
  if (name.empty() || !provider)
    return;
  worldTargets_[name] = std::move(provider);
}

void TutorialSpotlight::UnregisterWorldTarget(const std::string &name) {
  worldTargets_.erase(name);
}

void TutorialSpotlight::ClearWorldTargets() { worldTargets_.clear(); }

std::vector<std::string> TutorialSpotlight::GetWorldTargetNames() const {
  std::vector<std::string> names;
  names.reserve(worldTargets_.size());
  for (const auto &entry : worldTargets_)
    names.push_back(entry.first);
  std::sort(names.begin(), names.end());
  return names;
}

///************************* 内部処理 *************************///

bool TutorialSpotlight::BuildTargetRect(const TutorialSpotlightTarget &target,
                                        ScreenRect &out) const {
  const float screenWidth = static_cast<float>(WinApp::kClientWidth);
  const float screenHeight = static_cast<float>(WinApp::kClientHeight);

  switch (target.kind) {
  case TutorialSpotlightTargetKind::Ui: {
    if (target.id.empty())
      return false;
    UIBase *ui = UIManager::GetInstance()->GetUI(target.id);
    if (!ui)
      return false;

    // UI は「アンカー付きの中心座標＋サイズ」で置かれている。
    // アンカーは 0..1 の正規化ピボットなので、左上を逆算する。
    const Vector3 position = ui->GetPosition();
    const Vector2 size = ui->GetSize();
    const Vector2 scale = ui->GetScale();
    const Vector2 anchor = ui->GetAnchorPoint();
    const float width = size.x * scale.x;
    const float height = size.y * scale.y;

    out.minX = position.x - width * anchor.x;
    out.minY = position.y - height * anchor.y;
    out.maxX = out.minX + width;
    out.maxY = out.minY + height;
    return true;
  }

  case TutorialSpotlightTargetKind::Rect:
    out.minX = target.center.x - target.size.x * 0.5f;
    out.minY = target.center.y - target.size.y * 0.5f;
    out.maxX = target.center.x + target.size.x * 0.5f;
    out.maxY = target.center.y + target.size.y * 0.5f;
    return true;

  case TutorialSpotlightTargetKind::World: {
    if (!camera_ || target.id.empty())
      return false;
    const auto it = worldTargets_.find(target.id);
    if (it == worldTargets_.end() || !it->second)
      return false;

    const Vector3 center = it->second();
    const Matrix4x4 &viewProjection = camera_->GetViewProjectionMatrix();

    Vector2 screenCenter;
    if (!ProjectToScreen(center, viewProjection, screenWidth, screenHeight,
                         screenCenter)) {
      return false;
    }

    // 半径をピクセルへ換算する。カメラの向きによっては特定の軸が潰れるため、
    // XYZ 3方向へずらした点を投影し、最も大きな見かけの距離を採用する。
    const float radius = (std::max)(0.01f, target.radius);
    const Vector3 offsets[3] = {
        {center.x + radius, center.y, center.z},
        {center.x, center.y + radius, center.z},
        {center.x, center.y, center.z + radius},
    };
    float screenRadius = 0.0f;
    for (const Vector3 &offset : offsets) {
      Vector2 projected;
      if (!ProjectToScreen(offset, viewProjection, screenWidth, screenHeight,
                           projected)) {
        continue;
      }
      const float dx = projected.x - screenCenter.x;
      const float dy = projected.y - screenCenter.y;
      screenRadius = (std::max)(screenRadius, std::sqrt(dx * dx + dy * dy));
    }
    if (screenRadius <= 0.0f)
      return false;

    out.minX = screenCenter.x - screenRadius;
    out.minY = screenCenter.y - screenRadius;
    out.maxX = screenCenter.x + screenRadius;
    out.maxY = screenCenter.y + screenRadius;
    return true;
  }

  default:
    return false;
  }
}

void TutorialSpotlight::RebuildPanels() {
  const float screenWidth = static_cast<float>(WinApp::kClientWidth);
  const float screenHeight = static_cast<float>(WinApp::kClientHeight);

  // ------------------------------------------------------------
  // 穴を集める。画面外へはみ出した分は切り詰める。
  // ------------------------------------------------------------
  std::vector<ScreenRect> holes;
  holes.reserve(config_.targets.size());
  for (const TutorialSpotlightTarget &target : config_.targets) {
    ScreenRect rect;
    if (!BuildTargetRect(target, rect))
      continue;

    rect.minX = std::clamp(rect.minX - config_.padding, 0.0f, screenWidth);
    rect.minY = std::clamp(rect.minY - config_.padding, 0.0f, screenHeight);
    rect.maxX = std::clamp(rect.maxX + config_.padding, 0.0f, screenWidth);
    rect.maxY = std::clamp(rect.maxY + config_.padding, 0.0f, screenHeight);

    if (rect.maxX - rect.minX <= kEpsilon)
      continue;
    if (rect.maxY - rect.minY <= kEpsilon)
      continue;
    holes.push_back(rect);
  }

  // ------------------------------------------------------------
  // 暗幕を「穴を避けた矩形の集合」へ分解する。
  // 画面を穴の上下辺で横帯に切り、帯ごとに穴の x 区間を除いた隙間を並べる。
  // 穴が1つなら上下左右の4枚に、複数あっても破綻せず敷き詰められる。
  // ------------------------------------------------------------
  std::vector<float> bandEdges;
  bandEdges.reserve(holes.size() * 2 + 2);
  bandEdges.push_back(0.0f);
  bandEdges.push_back(screenHeight);
  for (const ScreenRect &hole : holes) {
    bandEdges.push_back(hole.minY);
    bandEdges.push_back(hole.maxY);
  }
  std::sort(bandEdges.begin(), bandEdges.end());
  bandEdges.erase(std::unique(bandEdges.begin(), bandEdges.end()),
                  bandEdges.end());

  std::vector<ScreenRect> panels;
  std::vector<std::pair<float, float>> spans;
  for (std::size_t i = 0; i + 1 < bandEdges.size(); ++i) {
    const float top = bandEdges[i];
    const float bottom = bandEdges[i + 1];
    if (bottom - top <= kEpsilon)
      continue;

    // この帯を丸ごと跨ぐ穴だけを集める。
    spans.clear();
    for (const ScreenRect &hole : holes) {
      if (hole.minY <= top + kEpsilon && hole.maxY >= bottom - kEpsilon) {
        spans.emplace_back(hole.minX, hole.maxX);
      }
    }
    std::sort(spans.begin(), spans.end());

    // 左から走査し、穴の隙間を暗幕として切り出す。
    // 重なった穴は running な right で自然に結合される。
    float right = 0.0f;
    for (const auto &span : spans) {
      if (span.first > right + kEpsilon) {
        panels.push_back(ScreenRect{right, top, span.first, bottom});
      }
      right = (std::max)(right, span.second);
    }
    if (screenWidth - right > kEpsilon) {
      panels.push_back(ScreenRect{right, top, screenWidth, bottom});
    }
  }

  // ------------------------------------------------------------
  // スプライトへ反映する。枚数はフレームごとに変わるため使い回す。
  // ------------------------------------------------------------
  UIManager *uiManager = UIManager::GetInstance();
  Vector4 color = config_.dimColor;
  color.w *= std::clamp(opacity_, 0.0f, 1.0f);

  for (std::size_t i = 0; i < panels.size(); ++i) {
    const ScreenRect &panel = panels[i];
    const std::string id = PanelId(i);

    UIBase *ui = uiManager->GetUI(id);
    if (!ui) {
      auto created = std::make_unique<UIBase>(id);
      created->Initialize("");
      created->SetTransient(true);
      ui = created.get();
      uiManager->AddUI(id, std::move(created));
    }
    ui->SetTexture(kPanelTexture);
    ui->SetAnchorPoint({0.5f, 0.5f});
    ui->SetPosition({(panel.minX + panel.maxX) * 0.5f,
                     (panel.minY + panel.maxY) * 0.5f, 0.0f});
    ui->SetSize({panel.maxX - panel.minX, panel.maxY - panel.minY});
    ui->SetColor(color);
    ui->SetLayer(layer_);
    ui->SetVisible(true);
    uiManager->BringToFront(id);
  }

  HidePanelsFrom(panels.size());
  activePanelCount_ = panels.size();
  uiManager->SortByLayer();

  // TutorialSpotlight は UIManager::UpdateAll より後に動くため、
  // 生成・移動したスプライトの頂点をこのフレーム分だけ即時更新する。
  for (std::size_t i = 0; i < panels.size(); ++i) {
    if (UIBase *ui = uiManager->GetUI(PanelId(i)))
      ui->Update();
  }
}

void TutorialSpotlight::HidePanelsFrom(std::size_t index) {
  UIManager *uiManager = UIManager::GetInstance();
  for (std::size_t i = index; i < activePanelCount_; ++i) {
    if (UIBase *ui = uiManager->GetUI(PanelId(i)))
      ui->SetVisible(false);
  }
}

} // namespace YoRigine
