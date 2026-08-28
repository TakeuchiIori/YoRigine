#pragma once

// C++
#include <d3d12.h>
#include <string>
#include <vector>
#include <wrl.h>

// Engine
#include "Material/MaterialColor.h"
#include "Material/MaterialDissolve.h"
#include "Material/MaterialLighting.h"
#include "Material/MaterialOverrideSet.h"
#include "Material/MaterialUV.h"
#include "Model.h"
#include "Motion/Core/MotionSystem.h"
#include "Systems/Camera/Camera.h"
#include <Loaders/Json/JsonManager.h>

// Math
#include "Matrix4x4.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

class Object3dCommon;

namespace YoRigine {

class Line;
class WorldTransform;
class Model;

/// <summary>
/// オブジェクト生成クラス
/// </summary>
class Object3d {
public:
  ///************************* GPU用構造体 *************************///

  // シャドウマップ
  struct ObjectTransform {
    Matrix4x4 world;
  };

  ///************************* Static関数 *************************///

  static std::unique_ptr<Object3d> Create(const std::string &filePath,
                                          const std::string &animationName = "",
                                          bool isAnimation = false);

  // 指定モデルのマテリアルから「白以外」の代表色を取り出す（無ければ false）。
  // モデルが未ロードなら読み込む。攻撃モデルの色を体へ反映する等に使う。
  static bool TryGetModelThemeColor(const std::string &filePath, Vector4 &out);

  // 指定モデルの「実テクスチャ（アクセント画像）」のパスを取り出す（無ければ
  // false）。 モデルが未ロードなら読み込む（＝そのテクスチャも TextureManager
  // に登録される）。
  static bool TryGetModelAccentTexture(const std::string &filePath,
                                       std::string &out);

  ///************************* 基本的な関数 *************************///

  Object3d();

  // 初期化
  void Initialize();
  // アニメーションの更新
  void UpdateAnimation();
  // 描画
  void Draw(YoRigine::Camera *camera, YoRigine::WorldTransform &worldTransform);
  // スケルトン描画
  void DrawBone(YoRigine::Line &line, const Matrix4x4 &worldMatrix);
  // 影描画
  void DrawShadow(YoRigine::WorldTransform &worldTransform);
  // モデルのセット
  void SetModel(const std::string &filePath, bool isAnimation = false,
                const std::string &animationName = "");
  // デバッグ表示
  void DebugInfo();

  ///************************* モーション関連 *************************///
  // アニメーションを切り替える
  void SetChangeMotion(const std::string &filePath, MotionPlayMode playMode,
                       const std::string &animationName = "");

  // 上半身のアニメーションを再生する
  void PlayUpperMotion(const std::string &filePath, MotionPlayMode playMode,
                       const std::string &animationName = "");

  // 今のアニメーション速度を切り替え
  void SetMotionSpeed(float speed);
  // 上半身アニメーション速度を独立して切り替え
  void SetUpperMotionSpeed(float speed);
  // モーションの再生方法
  void PlayOnce() { model_->PlayOnce(); }
  void PlayLoop() { model_->PlayLoop(); }
  void Stop() { model_->Stop(); }
  void Resume() { model_->Resume(); }

public:
  // UVのSRT
  Vector2 uvScale = {1.0f, 1.0f};
  Vector2 uvTranslate = {0.0f, 0.0f};
  float uvRotate = 0.0f;

private:
  ///************************* 内部処理*************************///

  // シャドウマップ用リソース作成
  void CreateShadowResources();

  // インバートハル輪郭線の描画（OutlineSettings 有効時に Draw 内から呼ぶ）。
  // 専用 PSO/RS へ切り替え、頂点ジオメトリを押し出しシェルとして描く。
  void DrawOutlinePass(YoRigine::Camera *camera,
                       YoRigine::WorldTransform &worldTransform);

  // UVの更新
  void UpdateUV();

public:
  ///************************* アクセッサ *************************///

  YoRigine::Model *GetModel() { return model_; }
  MaterialLighting *GetMaterialLighting() const {
    return materialLighting_.get();
  }

  // このオブジェクトだけモデルのテクスチャを別テクスチャ（パス）で上書きする。空で解除（本来のテクスチャ）。
  // モデルは共有されるが、上書きは Object3d
  // 単位（描画時に適用）なので他インスタンスに影響しない。
  void SetOverrideTexturePath(const std::string &path) {
    overrideTexturePath_ = path;
  }
  const std::string &GetOverrideTexturePath() const {
    return overrideTexturePath_;
  }

  // インバートハル輪郭線をこのオブジェクトに掛けるか（既定 true）。
  // 地面など輪郭を出したくないオブジェクトは false にする。
  void SetOutlineEnabled(bool enable) { outlineEnabled_ = enable; }
  bool IsOutlineEnabled() const { return outlineEnabled_; }

  ///************************* マテリアル上書き *************************///
  // モデルは共有されるため、色・粗さ・メタリック・テクスチャの個別変更は
  // ここに持つ上書きセットを介して行う。マルチメッシュのモデルでも
  // マテリアルスロット単位で別々に設定できる。

  // 上書きセットを取得（未生成なら nullptr）。
  MaterialOverrideSet *GetMaterialOverrides() const {
    return materialOverrides_.get();
  }

  // 上書きセットを取得（無ければモデルのマテリアル数ぶん生成する）。
  MaterialOverrideSet *EnsureMaterialOverrides();

  // 上書きをすべて解除する（セット自体は保持する）。
  void ClearMaterialOverrides();

  // 描画時に渡すべき上書きセット。実際に上書きが無ければ nullptr を返すので、
  // インスタンシングのバッチが不要に分裂しない。
  MaterialOverrideSet *GetActiveMaterialOverrides() const;

  Vector4 &GetColor() { return materialColor_->GetColor(); }
  void SetMaterialColor(const Vector4 &color) {
    materialColor_->SetColor(color);
  }
  void SetAlpha(float alpha) { materialColor_->SetAlpha(alpha); }
  void SetUvTransform(const Matrix4x4 &uvTransform) {
    materialUV_->SetUVTransform(uvTransform);
  }
  const Matrix4x4 &GetUvTransform() const {
    return materialUV_->GetUVTransform();
  }
  // 0 = 通常タイル / 1 = タイルごとのハッシュ回転＆オフセットを最大適用
  void SetStochasticStrength(float s) { materialUV_->SetStochasticStrength(s); }
  float GetStochasticStrength() const {
    return materialUV_->GetStochasticStrength();
  }

  ///************************* ディゾルブ *************************///
  MaterialDissolve *GetMaterialDissolve() const {
    return materialDissolve_.get();
  }
  void SetDissolveEnabled(bool enable) {
    materialDissolve_->SetEnabled(enable);
  }
  void SetDissolveThreshold(float t) { materialDissolve_->SetThreshold(t); }
  void SetDissolveEdgeWidth(float w) { materialDissolve_->SetEdgeWidth(w); }
  void SetDissolveEdgeColor(const Vector3 &c) {
    materialDissolve_->SetEdgeColor(c);
  }
  void SetDissolveNoiseScale(float s) { materialDissolve_->SetNoiseScale(s); }
  bool IsDissolveEnabled() const { return materialDissolve_->IsEnabled(); }
  float GetDissolveThreshold() const {
    return materialDissolve_->GetThreshold();
  }

  void SetEnableLighting(bool enable) {
    materialLighting_->SetEnableLighting(enable);
  }
  void SetEnableSpecular(bool enable) {
    materialLighting_->SetEnableSpecular(enable);
  }
  void SetEnableEnvironment(bool enable) {
    materialLighting_->SetEnableEnvironment(enable);
  }
  void SetIsHalfVector(bool isHalf) {
    materialLighting_->SetIsHalfVector(isHalf);
  }
  void SetShininess(float shininess) {
    materialLighting_->SetShininess(shininess);
  }
  void SetEnvironmentCoefficient(float coeff) {
    materialLighting_->SetEnvironmentCoefficient(coeff);
  }

  bool IsLightingEnabled() const {
    return materialLighting_->IsLightingEnabled();
  }
  bool IsSpecularEnabled() const {
    return materialLighting_->IsSpecularEnabled();
  }
  bool IsEnvironmentEnabled() const {
    return materialLighting_->IsEnvironmentEnabled();
  }
  bool IsHalfVector() const { return materialLighting_->IsHalfVector(); }
  float GetShininess() const { return materialLighting_->GetShininess(); }
  float GetEnvironmentCoefficient() const {
    return materialLighting_->GetEnvironmentCoefficient();
  }

private:
  ///************************* メンバ変数 *************************///

  Object3dCommon *object3dCommon_ = nullptr;
  YoRigine::Model *model_ = nullptr;

  // インバートハル輪郭線をこのオブジェクトに掛けるか（地面等は false）
  bool outlineEnabled_ = true;

  // このオブジェクト固有のテクスチャ上書きパス（空なら上書きしない）
  std::string overrideTexturePath_;

  // マテリアル関連
  std::unique_ptr<MaterialColor> materialColor_;
  std::unique_ptr<MaterialLighting> materialLighting_;
  std::unique_ptr<MaterialUV> materialUV_;
  std::unique_ptr<MaterialDissolve> materialDissolve_;

  // メッシュ (マテリアルスロット) 単位の上書き。使うまで生成されない。
  std::unique_ptr<MaterialOverrideSet> materialOverrides_;

  // デフォルトのモデルパス
  static const std::string defaultModelPath_;
  // シャドウマップ用リソース
  Microsoft::WRL::ComPtr<ID3D12Resource> objectCB_;
  ObjectTransform *objectData_;
};

} // namespace YoRigine
