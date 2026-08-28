#pragma once

// Engine
#include "Core/SceneViewSettings.h"
#include <Loaders/Json/Use/AutoJson.h>
#include <Material/MaterialOverrideSet.h>
#include <Object3D/ObjectManager.h>

namespace YoRigine::SceneJsonBinding {

///=============================================================================
/// PlacedObject / マテリアル上書き / シーン設定 を AutoJson へ束ねる関数群。
///
/// 保存と読み込みで別々にフィールドを列挙すると必ずどこかで食い違うので、
/// 「登録は 1 箇所」という AutoJson の性質をそのまま使う。
/// 返された AutoJson は対象オブジェクトのメンバを直接指しているので、
/// 対象より長生きさせないこと (その場で Save/Load して捨てる想定)。
///=============================================================================

// 配置オブジェクト 1 個ぶんのフィールド束
AutoJson BindPlacedObject(ObjectManager::PlacedObject &obj);

// マテリアルスロット 1 枚ぶんの上書き設定
AutoJson BindMaterialOverride(MeshMaterialOverride &slot);

// シーン全体の表示・カリング設定
AutoJson BindSceneSettings(SceneViewSettings &settings,
                           bool &collisionFrustumCulling);

} // namespace YoRigine::SceneJsonBinding
