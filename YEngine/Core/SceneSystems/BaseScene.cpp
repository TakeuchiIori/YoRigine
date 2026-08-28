#include "BaseScene.h"
#include <Object3D/BaseObjectManager.h>
#include <Object3D/Object3dCommon.h>

void BaseScene::InitializeCommon() {
  YoRigine::GameTime::Initialize();
  YoRigine::CollisionManager::GetInstance()->Initialize();

  // モデルマニピュレータ
  YoRigine::SceneEditor::GetInstance()->LoadScene(sceneName_);
  YoRigine::SceneEditor::GetInstance()->SetCamera(sceneCamera_.get());
}

void BaseScene::UpdateCommon() {
  YoRigine::GameTime::Update();
  YoRigine::SceneEditor::GetInstance()->Update();
  YoRigine::CollisionManager::GetInstance()->Update();
  YoRigine::LightManager::GetInstance()->UpdateShadowMatrix(sceneCamera_.get());
}

void BaseScene::DrawCommonObject() {
  Object3dCommon::GetInstance()->DrawPreference();
}

void BaseScene::DrawCommonShadow() {
  Object3dCommon::GetInstance()->ShadowDrawPreference();

  //------------------------------------------------------------
  // 登録オブジェクトの影描画もここで一括処理する。
  // 全シーンが本関数を呼んでいるため、シーン側に影描画コードは不要になる。
  //------------------------------------------------------------
  BaseObjectManager::GetInstance()->DrawShadowAll();
  YoRigine::SceneEditor::GetInstance()->DrawShadow();
}

void BaseScene::DrawCommonParticles() {}

// ============================================================
// 登録オブジェクトの一括更新
// ============================================================
void BaseScene::UpdateObjects() {
  BaseObjectManager::GetInstance()->UpdateAll();
}

// ============================================================
// 登録オブジェクトの一括描画
// ============================================================
void BaseScene::DrawObjects() {
  BaseObjectManager::GetInstance()->DrawAll();
  BaseObjectManager::GetInstance()->DrawAnimationAll();
}
