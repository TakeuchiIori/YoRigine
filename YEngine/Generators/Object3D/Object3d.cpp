#include "Object3d.h"
#include "Object3dCommon.h"

// C++
#include <algorithm>
#include <assert.h>
#include <fstream>
#include <sstream>

// Engine
#include "Debugger/Logger.h"
#include "Loaders./Texture./TextureManager.h"
#include "Material/OutlineSettings.h"
#include "Model.h"
#include "ModelManager.h"
#include "PipelineManager/YPipelineManager.h"
#include "WorldTransform./WorldTransform.h"
#include <LightManager/LightManager.h>

namespace YoRigine {

const std::string Object3d::defaultModelPath_ = "Resources/Models/";

/// <summary>
/// コンストラクタ
/// </summary>
Object3d::Object3d() {}

/// <summary>
/// Object3d の初期化
/// </summary>
void Object3d::Initialize() {
  object3dCommon_ = Object3dCommon::GetInstance();

  CreateShadowResources();
  materialColor_ = std::make_unique<MaterialColor>();
  materialColor_->Initialize();
  materialLighting_ = std::make_unique<MaterialLighting>();
  materialLighting_->Initialize();
  materialUV_ = std::make_unique<MaterialUV>();
  materialUV_->Initialize();
  materialDissolve_ = std::make_unique<MaterialDissolve>();
  materialDissolve_->Initialize();
}

/// <summary>
/// モデルのアニメーションを更新
/// </summary>
void Object3d::UpdateAnimation() {
  if (model_) {
    model_->UpdateAnimation();
  }
}

/// <summary>
/// Object3d の描画
/// </summary>
void Object3d::Draw(YoRigine::Camera *camera,
                    YoRigine::WorldTransform &worldTransform) {
  object3dCommon_->DrawPreference();

  auto pm = YPipelineManager::GetInstance();
  const auto &indices = pm->GetParameterIndices("Object");

  UpdateUV();

  Matrix4x4 worldViewProjectionMatrix{};
  Matrix4x4 worldMatrix{};

  if (model_) {

    if (camera) {
      const Matrix4x4 &vp = camera->GetViewProjectionMatrix();

      // ボーンの有無でルート行列の扱いが変わる
      if (!model_->GetHasBones()) {
        worldViewProjectionMatrix = worldTransform.GetMatWorld() *
                                    model_->GetRootNode().GetLocalMatrix() * vp;
        worldMatrix = worldTransform.GetMatWorld() *
                      model_->GetRootNode().GetLocalMatrix();
      } else {
        worldViewProjectionMatrix = worldTransform.GetMatWorld() * vp;
        worldMatrix = worldTransform.GetMatWorld();
      }
    } else {
      // カメラ無し（デバッグ）
      worldViewProjectionMatrix = worldTransform.GetMatWorld();
      worldMatrix = worldTransform.GetMatWorld();
    }
  }

  worldTransform.SetMapWVP(worldViewProjectionMatrix);
  worldTransform.SetMapWorld(worldMatrix);

  auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

  // === インバートハル輪郭線（本体より先に描き、内側は本体で上書きする） ===
  // 専用 PSO/RS
  // へ切り替えるため、ここで描いてからオブジェクト本体パイプラインへ復帰する。
  if (camera && model_ && outlineEnabled_ &&
      OutlineSettings::GetInstance()->IsEnabled()) {
    DrawOutlinePass(camera, worldTransform);

    // 輪郭用 RS へ切り替えた時点で Object RS の root 引数は無効化される。
    // 本体パイプラインへ戻し、DrawPreference で張っていたライトを張り直す。
    object3dCommon_->SetRootSignature();
    object3dCommon_->SetGraphicsCommand();
    object3dCommon_->SetPrimitiveTopology();
    YoRigine::LightManager::GetInstance()->SetCommandList(
        indices.at("gDirectionalLight"), indices.at("gPointLights"),
        indices.at("gSpotLights"));
  }

  // ワールド
  commandList->SetGraphicsRootConstantBufferView(
      indices.at("gTransformationMatrix"),
      worldTransform.GetConstBuffer()->GetGPUVirtualAddress());

  // カメラ
  if (camera) {
    commandList->SetGraphicsRootConstantBufferView(
        indices.at("gCamera"),
        camera->GetCameraResource()->GetGPUVirtualAddress());
  }
  // カラーパス：PS のカスケード選択用にライト VP 配列を渡す
  commandList->SetGraphicsRootConstantBufferView(
      indices.at("gCascadeShadow"), YoRigine::LightManager::GetInstance()
                                        ->GetCascadeResource()
                                        ->GetGPUVirtualAddress());
  // マテリアル
  materialUV_->RecordDrawCommands(commandList.Get(), indices.at("gMaterial"));
  materialColor_->RecordDrawCommands(commandList.Get(),
                                     indices.at("gMaterialColor"));
  materialLighting_->RecordDrawCommands(commandList.Get(),
                                        indices.at("gMaterialLight"));
  // ディゾルブ（旧シェーダとの互換のため find チェック）
  if (auto it = indices.find("gMaterialDissolve"); it != indices.end()) {
    materialDissolve_->RecordDrawCommands(commandList.Get(), it->second);
  }

  // モデル描画（テクスチャ上書きが設定されていればそれを使う）
  if (model_) {
    MaterialOverrideSet *overrides = GetActiveMaterialOverrides();
    if (overrides) {
      overrides->Apply(*model_);
    }
    model_->Draw(overrideTexturePath_, overrides);
  }
}

/// <summary>
/// マテリアル上書きセットを取得する（無ければモデルのマテリアル数ぶん生成）
/// </summary>
MaterialOverrideSet *Object3d::EnsureMaterialOverrides() {
  if (!materialOverrides_) {
    materialOverrides_ = std::make_unique<MaterialOverrideSet>();
  }
  if (model_) {
    materialOverrides_->EnsureSlotCount(model_->GetMaterialCount());
  }
  return materialOverrides_.get();
}

void Object3d::ClearMaterialOverrides() {
  if (materialOverrides_) {
    materialOverrides_->ClearAll();
  }
}

MaterialOverrideSet *Object3d::GetActiveMaterialOverrides() const {
  if (!materialOverrides_ || !materialOverrides_->HasAnyOverride()) {
    return nullptr;
  }
  return materialOverrides_.get();
}

/// <summary>
/// モデルボーンのデバッグ描画
/// </summary>
void Object3d::DrawBone(YoRigine::Line &line, const Matrix4x4 &worldMatrix) {
  if (model_) {
    model_->DrawBone(line, worldMatrix);
  }
}

void Object3d::DrawShadow(YoRigine::WorldTransform &worldTransform) {
  if (!model_)
    return;

  // PSO・RS・topology・gLight は Object3dCommon::ShadowDrawPreference()
  // で設定済み。 ここではオブジェクト固有のワールド行列のみ更新してセットする。
  auto pm = YPipelineManager::GetInstance();
  const auto &indices = pm->GetParameterIndices("ShadowMap");
  auto commandList = object3dCommon_->GetDxCommon()->GetCommandList().Get();

  if (!model_->GetHasBones()) {
    objectData_->world =
        worldTransform.GetMatWorld() * model_->GetRootNode().GetLocalMatrix();
  } else {
    objectData_->world = worldTransform.GetMatWorld();
  }
  commandList->SetGraphicsRootConstantBufferView(
      indices.at("gObject"), objectCB_->GetGPUVirtualAddress());

  model_->DrawShadow();
}

/// <summary>
/// インバートハル輪郭線の描画。
/// 専用 PSO/RS（前面カリング）に切り替え、頂点を法線方向へ押し出した
/// シェルを描く。ジオメトリ供給は頂点/インデックスのみで足りるため
/// Model::DrawShadow() を流用する（PSO/RS/CB は本関数で設定済み）。
/// </summary>
void Object3d::DrawOutlinePass(YoRigine::Camera *camera,
                               YoRigine::WorldTransform &worldTransform) {
  if (!model_ || !camera)
    return;

  auto pm = YPipelineManager::GetInstance();
  const auto &oidx = pm->GetParameterIndices("ObjectOutline");
  auto commandList = object3dCommon_->GetDxCommon()->GetCommandList().Get();

  // 輪郭用 PSO / RS / トポロジ
  commandList->SetPipelineState(pm->GetPipeLineStateObject("ObjectOutline"));
  commandList->SetGraphicsRootSignature(pm->GetRootSignature("ObjectOutline"));
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  // 最新のパラメータ（太さ・色）を CB へ反映
  OutlineSettings::GetInstance()->UpdateCB();

  commandList->SetGraphicsRootConstantBufferView(
      oidx.at("gTransformationMatrix"),
      worldTransform.GetConstBuffer()->GetGPUVirtualAddress());
  commandList->SetGraphicsRootConstantBufferView(
      oidx.at("gCamera"), camera->GetCameraResource()->GetGPUVirtualAddress());
  commandList->SetGraphicsRootConstantBufferView(
      oidx.at("gOutline"),
      OutlineSettings::GetInstance()->GetResource()->GetGPUVirtualAddress());

  // 頂点/インデックスをバインドして描画するだけ（Model::DrawShadow を流用）
  model_->DrawShadow();
}

void Object3d::CreateShadowResources() {
  objectCB_ = object3dCommon_->GetDxCommon()->CreateBufferResource(
      sizeof(ObjectTransform));
  objectCB_->Map(0, nullptr, reinterpret_cast<void **>(&objectData_));
  objectData_->world = MakeIdentity4x4();
}

/// <summary>
/// UVアニメーションの更新
/// </summary>
void Object3d::UpdateUV() {
  Vector3 s = {uvScale.x, uvScale.y, 1.0f};
  Vector3 r = {0.0f, 0.0f, uvRotate};
  Vector3 t = {uvTranslate.x, uvTranslate.y, 0.0f};

  Matrix4x4 affine = MakeAffineMatrix(s, r, t);

  SetUvTransform(affine);
}

/// <summary>
/// モデルを読み込みセットする
/// </summary>
void Object3d::SetModel(const std::string &filePath, bool isAnimation,
                        const std::string &animationName) {
  // モデルのパスを解析して正しい形式に変換
  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(filePath);

  ModelManager::GetInstance()->LoadModel(defaultModelPath_ + basePath, fileName,
                                         animationName, isAnimation);

  model_ = ModelManager::GetInstance()->FindModel(fileName, animationName,
                                                  isAnimation);
}

bool Object3d::TryGetModelThemeColor(const std::string &filePath,
                                     Vector4 &out) {
  // モデルを（未ロードなら）読み込み、そのマテリアルの「白以外」の色を取り出す。
  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(filePath);
  ModelManager::GetInstance()->LoadModel(defaultModelPath_ + basePath, fileName,
                                         "", false);
  YoRigine::Model *model =
      ModelManager::GetInstance()->FindModel(fileName, "", false);
  if (!model) {
    return false;
  }
  return model->TryGetThemeColor(out);
}

bool Object3d::TryGetModelAccentTexture(const std::string &filePath,
                                        std::string &out) {
  // モデルを（未ロードなら）読み込む。この時点でモデルのテクスチャも
  // TextureManager に登録される。
  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(filePath);
  ModelManager::GetInstance()->LoadModel(defaultModelPath_ + basePath, fileName,
                                         "", false);
  YoRigine::Model *model =
      ModelManager::GetInstance()->FindModel(fileName, "", false);
  if (!model) {
    return false;
  }
  return model->TryGetAccentTexturePath(out);
}

/// <summary>
/// モデルのデバッグ情報を表示
/// </summary>
void Object3d::DebugInfo() {
  if (model_) {
    model_->DebugInfo();
  }
}

/// <summary>
/// 別モーションへ切り替える
/// </summary>
void Object3d::SetChangeMotion(const std::string &filePath,
                               MotionPlayMode playMode,
                               const std::string &animationName) {
  if (!model_) {
    return;
  }

  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(filePath);

  model_->SetChangeMotion(defaultModelPath_ +
                              basePath, // "Resources/Models/Enemy/Slime"
                          fileName,     // "Slime.gltf"
                          playMode, animationName);
}

void Object3d::PlayUpperMotion(const std::string &filePath,
                               MotionPlayMode playMode,
                               const std::string &animationName) {
  if (!model_) {
    return;
  }

  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(filePath);

  model_->PlayUpperMotion(defaultModelPath_ + basePath, fileName, playMode,
                          animationName);
}

/// <summary>
// モーション速度の切り替え
/// </summary>
/// <param name="speed"></param>
void Object3d::SetMotionSpeed(float speed) {
  if (model_) {
    if (model_->GetMotionSystem()) {
      model_->GetMotionSystem()->SetCurrentAnimationSpeed(speed);
    }
  }
}

/// <summary>
/// 上半身モーション速度の独立した切り替え
/// </summary>
void Object3d::SetUpperMotionSpeed(float speed) {
  if (model_) {
    if (model_->GetMotionSystem()) {
      model_->GetMotionSystem()->SetUpperMotionSpeed(speed);
    }
  }
}
/// <summary>
/// ファイル名から Object3d を生成
/// </summary>
std::unique_ptr<Object3d> Object3d::Create(const std::string &filePath,
                                           const std::string &animationName,
                                           bool isAnimation) {
  // モデルのパスを解析して正しい形式に変換
  auto [basePath, fileName] =
      ModelManager::GetInstance()->ParseModelPath(filePath);

  ModelManager::GetInstance()->LoadModel(defaultModelPath_ + basePath, fileName,
                                         animationName, isAnimation);

  YoRigine::Model *model = ModelManager::GetInstance()->FindModel(
      fileName, animationName, isAnimation);
  if (!model) {
    return nullptr;
  }
  auto newObj = std::make_unique<Object3d>();
  newObj->Initialize();
  newObj->model_ = model;
  return newObj;
}

} // namespace YoRigine
