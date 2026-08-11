#include "SceneEditor.h"

// Engine
#include <Debugger/Logger.h>

#ifdef USE_IMGUI
#include <Core/Editor/Widgets/YEditorWidget.h>
#include <Editor/Editor.h>
#include <imgui.h>
#endif

// C++
#include <vector>

namespace YoRigine {

SceneEditor *SceneEditor::GetInstance() {
  static SceneEditor instance;
  return &instance;
}

//=============================================================================
// コンストラクタ
//
// サブシステムはすべて context_ への参照を握るので、必ずここで束縛する。
// context_ の中身 (実体へのポインタ) は Initialize / SetCamera で埋まる。
//=============================================================================
SceneEditor::SceneEditor()
    : renderer_(context_), debugDrawer_(context_), clipboard_(context_),
      placement_(context_), loadController_(context_)
#ifdef USE_IMGUI
      ,
      gizmoLayer_(context_)
#endif
{
}

//=============================================================================
// 共有コンテキストの構築
//=============================================================================
void SceneEditor::BuildContext() {
  context_.camera = camera_;
  context_.objectManager = objectManager_;
  context_.selector = &selector_;
  context_.serializer = &serializer_;
  context_.prefabManager = &prefabManager_;
  context_.motionEditor = &motionEditor_;
  context_.viewSettings = &viewSettings_;
}

//=============================================================================
// 初期化
//=============================================================================
void SceneEditor::Initialize() {
  if (isInitialized_) {
    return; // 重複初期化を防止
  }

  objectManager_ = ObjectManager::GetInstance();
  BuildContext();

  serializer_.SetObjectManager(objectManager_);
  serializer_.SetModelFolderPath(placement_.GetModelFolderPath());
  serializer_.SetViewSettings(&viewSettings_);

  prefabManager_.SetObjectManager(objectManager_);
  prefabManager_.SetSerializer(&serializer_);
  prefabManager_.ScanPrefabFolder();

  selector_.SetObjectManager(objectManager_);
  motionEditor_.Initialize(camera_);
  debugDrawer_.Initialize();

#ifdef USE_IMGUI
  pickBuffer_ = PickBuffer::GetInstance();
  pickBuffer_->Initialize(); // PickBuffer もここで一度だけ初期化
  selector_.SetPickBuffer(pickBuffer_);

  stampMode_.SetObjectManager(objectManager_);
  gizmoLayer_.Initialize();

  SetupUI();
  SetupShortcuts();
#endif

  isInitialized_ = true;
}

#ifdef USE_IMGUI
//=============================================================================
// UI のセットアップ
//=============================================================================
void SceneEditor::SetupUI() {
  modelBrowser_.SetModelFolderPath(placement_.GetModelFolderPath());
  modelBrowser_.SetPlaceCallback(
      [this](const std::string &path) { PlaceObject(path); });
  modelBrowser_.ScanModelFolder();

  // マテリアルのテクスチャ差し替えは MaterialPanel が持つ FileBrowser が
  // 走査するので、ここで候補を先読みする必要はない。

  ScenePanelContext panelContext;
  panelContext.scene = &context_;
  panelContext.placement = &placement_;
  panelContext.loader = &loadController_;
  panelContext.gizmoLayer = &gizmoLayer_;
  panelContext.startStamp = [this] {
    stampMode_.Enter(selector_.GetPrimaryId());
  };
  panelContext.exitStamp = [this] { stampMode_.Exit(); };
  panelContext.isStamping = [this] { return stampMode_.IsActive(); };

  editorUI_.SetContext(panelContext);

  // Editor へのメニュー登録もここで一度だけ行う
  Editor::GetInstance()->RegisterMenuBar([this] { editorUI_.DrawMenuBar(); });
}

//=============================================================================
// ショートカットのセットアップ
//
// 「どのキーで何が起きるか」は SceneEditorShortcuts
// 側にまとまっているので、 ここでは各操作の実体を渡すだけ。
//=============================================================================
void SceneEditor::SetupShortcuts() {
  SceneEditorShortcuts::Actions actions;
  actions.save = [this] { loadController_.Save(); };
  actions.copy = [this] { clipboard_.Copy(); };
  actions.paste = [this] { clipboard_.Paste(); };
  actions.duplicate = [this] { DuplicateSelection(); };
  actions.deleteSelection = [this] { DeleteSelection(); };
  actions.snapToSurface = [this] { placement_.SnapSelectionToSurface(); };
  actions.focusSelection = [this] { FocusSelection(); };
  actions.startStamp = [this] { stampMode_.Enter(selector_.GetPrimaryId()); };
  actions.exitStamp = [this] { stampMode_.Exit(); };
  shortcuts_.SetActions(std::move(actions));
}

//=============================================================================
// 選択操作のヘルパー
//=============================================================================
void SceneEditor::DeleteSelection() {
  // 反復中に選択集合が変化しないよう ID を控えてから削除する
  const std::vector<int> ids(selector_.GetSelectedIds().begin(),
                             selector_.GetSelectedIds().end());
  for (const int id : ids) {
    objectManager_->DeleteObject(id);
  }
  selector_.ClearSelection();
}

void SceneEditor::DuplicateSelection() {
  const std::vector<int> ids(selector_.GetSelectedIds().begin(),
                             selector_.GetSelectedIds().end());
  selector_.ClearSelection();
  for (const int id : ids) {
    if (auto *duplicate =
            objectManager_->DuplicateObject(id, {1.0f, 0.0f, 0.0f})) {
      selector_.AddToSelection(duplicate->id);
    }
  }
}

void SceneEditor::FocusSelection() {
  if (!camera_ || !selector_.HasSelection()) {
    return;
  }
  // 選択中の重心へカメラの注視点だけを寄せる。
  // カメラの操作方式 (デバッグカメラ / ゲームカメラ) に踏み込まないよう、
  // ここでは平行移動だけに留める。
  Vector3 center{};
  int count = 0;
  for (const int id : selector_.GetSelectedIds()) {
    if (auto *obj = objectManager_->GetObjectById(id)) {
      center += obj->position;
      ++count;
    }
  }
  if (count == 0) {
    return;
  }
  center /= static_cast<float>(count);

  // 現在の視線方向を保ったまま、一定距離だけ手前に下がった位置へ移動する。
  // 視線はカメラのワールド行列の Z 軸 (3 行目) から取る。
  constexpr float kFocusDistance = 12.0f;
  const Matrix4x4 &world = camera_->worldMatrix_;
  Vector3 forward{world.m[2][0], world.m[2][1], world.m[2][2]};
  forward = Normalize(forward);
  camera_->SetTranslate(center - forward * kFocusDistance);
}

//=============================================================================
// シーンエディタが有効か
//
// 宣言 (ヘッダ) と呼び出し元はすべて USE_IMGUI ガード内に閉じているため、
// Release ビルド (USE_IMGUI 未定義) ではこの関数は存在しない。
//=============================================================================
bool SceneEditor::IsSceneEditorActive() const {
  // 「モデル操作」ウィンドウ (MyGame で RegisterGameUI 登録された名前) が
  // 開かれているときのみ、選択・ギズモを有効化する。
  return Editor::GetInstance()->GetShowEditor() &&
         Editor::GetInstance()->IsGameUIVisible("モデル操作");
}
#endif // USE_IMGUI

//=============================================================================
// カメラの設定
//=============================================================================
void SceneEditor::SetCamera(Camera *camera) {
  camera_ = camera;
  context_.camera = camera;

  selector_.SetCamera(camera);
  motionEditor_.SetCamera(camera);
  debugDrawer_.SetCamera(camera);
  if (objectManager_) {
    objectManager_->SetCamera(camera);
  }
#ifdef USE_IMGUI
  stampMode_.SetCamera(camera);
#endif
}

//=============================================================================
// Update
//
// ■ Pick Pass の順番
//   1. BeginPickPass()     ← RT クリア・パイプラインセット
//   2. DrawForPick()       ← 全オブジェクトを PickPSO で描画 (ID を焼き込む)
//   3. EndPickPass()       ← クリック座標を 1px コピー + Signal
//   4. selector_.Update()  ← クリック検出時は RequestPick、
//                            前フレームの結果があれば ReadPickResult
//
//   クリック座標は RequestPick で登録され EndPickPass でコピーされる。
//   次フレームの Update の先頭で ReadPickResult が読み取る。
//=============================================================================
void SceneEditor::Update() {
  if (!isInitialized_) {
    return;
  }

#ifdef USE_IMGUI
  const bool editorActive = IsSceneEditorActive();
  const bool stamping = stampMode_.IsActive();

  shortcuts_.Update(editorActive, selector_.HasSelection(), stamping);

  // モーションエディタは「使う」と明示したときだけ選択に追従させる
  if (motionEditor_.IsEnabled()) {
    motionEditor_.SetTargetObjectId(selector_.GetPrimaryId());
    motionEditor_.Update();
  }

  const ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
  const ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();

  // スタンプモード中はクリックを StampMode が消費するので選択処理は抑制する
  selector_.SetCamera(camera_);
  selector_.Update(editorActive && !stamping, viewPos, viewSize);

  stampMode_.Update(viewPos, viewSize);
#endif

  // ObjectManager::Update() は Framework::Update() から毎フレーム
  // 1 回だけ駆動される。ここでも呼ぶとアニメーション時間が二重に進む。
}

//=============================================================================
// 描画パス (すべてサブシステムへの取り次ぎ)
//=============================================================================
void SceneEditor::Draw() {
  if (!isInitialized_) {
    return;
  }
  renderer_.Draw();
  motionEditor_.Draw();

#ifdef USE_IMGUI
  // スタンプモード中はカーソル下にゴーストを描画
  stampMode_.DrawGhost();
#endif
}

void SceneEditor::DrawLine() {
  if (!isInitialized_ || !camera_) {
    return;
  }
  motionEditor_.DrawBone();

#ifdef USE_IMGUI
  // デバッグ線はエディタ専用。Release では描画コストごと消す。
  debugDrawer_.Draw();
#endif
}

void SceneEditor::DrawShadow() {
  if (!isInitialized_) {
    return;
  }
  renderer_.DrawShadow();
}

void SceneEditor::DrawForPick() { renderer_.DrawForPick(); }

void SceneEditor::DrawPickPass() {
#ifdef USE_IMGUI
  if (!isInitialized_ || !pickBuffer_) {
    return;
  }
  // クリックが無いなら描画しない (無駄な GPU 負荷を避ける)
  if (!pickBuffer_->IsPickPending()) {
    return;
  }
  pickBuffer_->BeginPickPass();
  renderer_.DrawForPick();
  pickBuffer_->EndPickPass();
#endif
}

void SceneEditor::DrawImGui() {
#ifdef USE_IMGUI
  if (!isInitialized_) {
    return;
  }
  if (editorUI_.GetShowModelBrowserPtr() &&
      *editorUI_.GetShowModelBrowserPtr()) {
    modelBrowser_.Draw();
    ImGui::Separator();
  }
  editorUI_.DrawPanels();
  motionEditor_.ShowEditor();
#endif
}

void SceneEditor::DrawGizmo() {
#ifdef USE_IMGUI
  if (!isInitialized_ || !IsSceneEditorActive()) {
    return;
  }
  gizmoLayer_.Draw(Editor::GetInstance()->GetGameViewPos(),
                   Editor::GetInstance()->GetGameViewSize());
  motionEditor_.DrawGizmo();
#endif
}

//=============================================================================
// シーン操作
//=============================================================================
void SceneEditor::LoadScene(const std::string &sceneName) {
  // システムが初期化されていなければ初期化する (安全策)
  if (!isInitialized_) {
    Initialize();
  }
#ifdef USE_IMGUI
  if (pickBuffer_) {
    pickBuffer_->Reset();
  }
#endif
  loadController_.Load(sceneName);
}

void SceneEditor::PlaceObject(const std::string &modelPath) {
  placement_.PlaceModel(modelPath);
}

//=============================================================================
// 終了処理
//=============================================================================
void SceneEditor::Finalize() {
#ifdef USE_IMGUI
  // 状態リセットのみ (GPU 同期は不要)
  if (pickBuffer_) {
    pickBuffer_->Reset();
  }
#endif
  if (objectManager_) {
    objectManager_->Finalize();
  }
}

} // namespace YoRigine
