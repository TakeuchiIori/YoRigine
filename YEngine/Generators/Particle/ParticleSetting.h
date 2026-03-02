#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <random>
#include <algorithm>

#include "Vector3.h"
#include "Vector4.h"
#include "Vector2.h"
#include "Matrix4x4.h"
#include "Mathfunc.h"
#include "Loaders/Json/EnumUtils.h"

// トレイル用構造体
struct TrailSegment {
	Vector3 position;
	float age;
	float width;
	Vector4 color;
};

// CPUパーティクルのパラメーラー構造体
struct ParticleData {
	// 基本的な位置・変形・動作
	Vector3 position;
	Vector3 scale;
	Vector3 rotation;
	Vector3 velocity;
	Vector4 color;

	// 時間管理
	float age;
	float lifeTime;
	float currentTime;

	// 初期値保存（リセット・補間用）
	Vector3 initPos;
	Vector3 initScale;
	Vector3 initRot;
	Vector3 initVelocity;
	Vector4 initColor;

	// 基本物理
	float mass = 1.0f;
	Vector3 angularVelocity = { 0.0f, 0.0f, 0.0f };
	Vector3 acceleration = { 0.0f, 0.0f, 0.0f };
	Vector3 force = { 0.0f, 0.0f, 0.0f };
	bool hasCollided = false;
	uint32_t instance;

	// 既存のアニメーション・エフェクト
	float sizeOverTime = 1.0f;
	float rotationSpeed = 0.0f;
	float alphaFadeRate = 1.0f;

	// トレイル
	std::vector<Vector3> trailPositions;
	int trailLength = 0;

	// ノイズ
	Vector3 noiseOffset = { 0.0f, 0.0f, 0.0f };
	float noiseTime = 0.0f;

	// ランダム回転システム
	Vector3 rotationVelocity = { 0.0f, 0.0f, 0.0f };                // 各軸の回転速度
	Vector3 initialRotationVelocity = { 0.0f, 0.0f, 0.0f };         // 初期回転速度（リセット用）
	Vector3 rotationAcceleration = { 0.0f, 0.0f, 0.0f };            // 回転加速度
	bool rotationOverTimeEnabled = false;                           // 回転時間変化フラグ

	// 拡張物理
	Vector3 turbulenceForce = { 0.0f, 0.0f, 0.0f };                 // 乱流による力
	Vector3 vortexForce = { 0.0f, 0.0f, 0.0f };                     // 渦による力
	Vector3 customForces = { 0.0f, 0.0f, 0.0f };                    // カスタム力
	float dragCoefficient = 0.0f;                                   // 個別空気抵抗係数

	// 色・見た目の拡張
	Vector4 targetColor = { 1.0f, 1.0f, 1.0f, 1.0f };               // 目標色（補間用）
	float colorTransitionSpeed = 1.0f;                              // 色変化速度
	float brightnessMultiplier = 1.0f;                              // 明度倍率
	float saturationMultiplier = 1.0f;                              // 彩度倍率

	// サイズ・スケールの拡張
	Vector3 targetScale = { 1.0f, 1.0f, 1.0f };                     // 目標スケール
	Vector3 scaleVelocity = { 0.0f, 0.0f, 0.0f };                   // スケール変化速度
	float uniformScaleMultiplier = 1.0f;                            // 統一スケール倍率
	bool maintainAspectRatio = true;                                // アスペクト比維持

	// UV・テクスチャ関連
	Vector2 uvOffset = { 0.0f, 0.0f };                              // UV座標オフセット
	Vector2 uvVelocity = { 0.0f, 0.0f };                            // UV移動速度
	Vector2 textureSheetIndex = { 0.0f, 0.0f };                     // テクスチャシートインデックス
	float textureSheetFrame = 0.0f;                                 // 現在のフレーム（小数点可）
	float textureSheetSpeed = 1.0f;                                 // アニメーション速度

	// エミッション・生成関連
	float emissionTime = 0.0f;                                      // エミッション時刻
	uint32_t generationID = 0;                                      // 生成世代ID
	uint32_t parentSystemID = 0;                                    // 親システムID
	bool isSubEmitter = false;                                      // サブエミッター由来か

	// 衝突・相互作用
	Vector3 lastCollisionNormal = { 0.0f, 1.0f, 0.0f };             // 最後の衝突法線
	float collisionTime = 0.0f;                                     // 衝突時刻
	int collisionCount = 0;                                         // 衝突回数
	float bounciness = 0.5f;                                        // 反発係数（個別）
	float frictionCoefficient = 0.1f;                               // 摩擦係数（個別）

	// 環境相互作用
	float windResistance = 1.0f;                                    // 風への抵抗
	float temperatureEffect = 0.0f;                                 // 温度効果
	float gravityMultiplier = 1.0f;                                 // 重力倍率（個別）
	Vector3 magneticField = { 0.0f, 0.0f, 0.0f };                   // 磁場の影響

	// LOD・カリング関連
	float distanceToCamera = 0.0f;                                  // カメラまでの距離
	uint32_t lodLevel = 0;                                          // LODレベル（0=最高品質）
	bool isVisible = true;                                          // 描画フラグ
	float screenSize = 1.0f;                                        // スクリーンサイズ（ピクセル）

	// 状態フラグ
	bool isDying = false;                                           // 死亡処理中
	bool isPaused = false;                                          // 一時停止中
	bool isActive = true;                                           // アクティブ状態
	bool needsUpdate = true;                                        // 更新必要フラグ

	// デバッグ・エディタ用
	std::string debugName = "";                                     // デバッグ名
	uint32_t debugID = 0;                                           // デバッグID
	Vector4 debugColor = { 1.0f, 1.0f, 1.0f, 1.0f };                // デバッグ表示色
	bool isSelected = false;                                        // エディタで選択中

	// パフォーマンス最適化
	uint32_t updateFrame = 0;                                       // 最終更新フレーム
	float lastUpdateTime = 0.0f;                                    // 最終更新時刻
	bool skipPhysics = false;                                       // 物理計算スキップ
	bool skipRendering = false;                                     // 描画スキップ

	// 詳細なアニメーション
	float animationPhase = 0.0f;                                    // アニメーション位相
	float pulseFrequency = 0.0f;                                    // パルス周波数
	float waveAmplitude = 0.0f;                                     // 波の振幅
	Vector3 orbitCenter = { 0.0f, 0.0f, 0.0f };                     // 軌道中心
	float orbitRadius = 0.0f;                                       // 軌道半径
	float orbitSpeed = 0.0f;                                        // 軌道速度

	// サブパーティクル・チェイン
	std::vector<uint32_t> childParticles;                           // 子パーティクルID
	uint32_t parentParticleID = 0;                                  // 親パーティクルID
	bool hasChildren = false;                                       // 子を持つか
	float childSpawnTimer = 0.0f;                                   // 子生成タイマー

	// トレイル関連
	std::vector<TrailSegment> trailSegments;                        // トレイルセグメント
	float trailTimer = 0.0f;                                        // トレイル更新タイマー
	Vector3 lastTrailPosition;                                      // 前回のトレイル記録位置
	bool trailInitialized = false;                                  // トレイル初期化フラグ

	// カスタムデータ（拡張用）
	std::unordered_map<std::string, float> customFloats;            // カスタム浮動小数点
	std::unordered_map<std::string, Vector3> customVectors;         // カスタムベクトル
	std::unordered_map<std::string, bool> customBools;              // カスタムブール値
	void* customUserData = nullptr;                                 // ユーザーデータポインタ
};

// Forward declarations
class DirectXCommon;
class SrvManager;
class Camera;
class Mesh;
class TextureManager;
class PipelineManager;

class ParticleSetting {
public:
	//************************* 基本設定 *************************//
	int GetMaxParticles() const { return kMaxParticles_; }
	void SetMaxParticles(int value) { kMaxParticles_ = value; }

	float GetEmissionRate() const { return emissionRate_; }
	void SetEmissionRate(float value) { emissionRate_ = value; }

	const Vector2& GetLifeTimeRange() const { return lifeTimeRange_; }
	void SetLifeTimeRange(const Vector2& value) { lifeTimeRange_ = value; }

	bool GetLooping() const { return looping_; }
	void SetLooping(bool value) { looping_ = value; }

	float GetDuration() const { return duration_; }
	void SetDuration(float value) { duration_ = value; }

	float GetStartDelay() const { return startDelay_; }
	void SetStartDelay(float value) { startDelay_ = value; }


	//************************* ランダム回転 *************************//
	bool GetRandomRotationEnabled() const { return randomRotationEnabled_; }
	void SetRandomRotationEnabled(bool enabled) { randomRotationEnabled_ = enabled; }

	const Vector3& GetRandomRotationRange() const { return randomRotationRange_; }
	void SetRandomRotationRange(const Vector3& range) { randomRotationRange_ = range; }

	const Vector3& GetRandomRotationSpeed() const { return randomRotationSpeed_; }
	void SetRandomRotationSpeed(const Vector3& speed) { randomRotationSpeed_ = speed; }

	bool GetInheritInitialRotation() const { return inheritInitialRotation_; }
	void SetInheritInitialRotation(bool inherit) { inheritInitialRotation_ = inherit; }

	bool GetRandomRotationPerAxis() const { return randomRotationPerAxis_; }
	void SetRandomRotationPerAxis(bool perAxis) { randomRotationPerAxis_ = perAxis; }

	bool GetRotationOverTime() const { return rotationOverTime_; }
	void SetRotationOverTime(bool enabled) { rotationOverTime_ = enabled; }

	const Vector3& GetRotationAcceleration() const { return rotationAcceleration_; }
	void SetRotationAcceleration(const Vector3& acceleration) { rotationAcceleration_ = acceleration; }

	float GetRotationDamping() const { return rotationDamping_; }
	void SetRotationDamping(float damping) { rotationDamping_ = damping; }

	//************************* 物理 *************************//
	const Vector3& GetGravity() const { return gravity_; }
	void SetGravity(const Vector3& value) { gravity_ = value; }

	float GetDrag() const { return drag_; }
	void SetDrag(float value) { drag_ = value; }

	float GetMass() const { return mass_; }
	void SetMass(float value) { mass_ = value; }

	float GetBounciness() const { return bounciness_; }
	void SetBounciness(float value) { bounciness_ = value; }

	float GetFriction() const { return friction_; }
	void SetFriction(float value) { friction_ = value; }

	bool GetCollisionEnabled() const { return collisionEnabled_; }
	void SetCollisionEnabled(bool value) { collisionEnabled_ = value; }

	bool GetIsPhysicsEnabled() const { return isPhysicsEnabled_; }
	void SetIsPhysicsEnabled(bool value) { isPhysicsEnabled_ = value; }

	float GetCollisionRadius() const { return collisionRadius_; }
	void SetCollisionRadius(float value) { collisionRadius_ = value; }

	//************************* ノイズなど *************************//
	bool GetTurbulenceEnabled() const { return turbulenceEnabled_; }
	void SetTurbulenceEnabled(bool value) { turbulenceEnabled_ = value; }

	float GetTurbulenceStrength() const { return turbulenceStrength_; }
	void SetTurbulenceStrength(float value) { turbulenceStrength_ = value; }

	float GetTurbulenceFrequency() const { return turbulenceFrequency_; }
	void SetTurbulenceFrequency(float value) { turbulenceFrequency_ = value; }

	Vector3 GetNoiseScale() const { return noiseScale_; }
	void SetNoiseScale(const Vector3& value) { noiseScale_ = value; }

	float GetNoiseSpeed() const { return noiseSpeed_; }
	void SetNoiseSpeed(float value) { noiseSpeed_ = value; }

	//************************* 色 *************************//

	Vector4 GetSystemColor() const { return systemColor_; }
	void SetSystemColor(const Vector4& color) { systemColor_ = color; }

	const Vector4& GetStartColor() const { return startColor_; }
	void SetStartColor(const Vector4& value) { startColor_ = value; }

	const Vector4& GetEndColor() const { return endColor_; }
	void SetEndColor(const Vector4& value) { endColor_ = value; }

	ParticleManagerEnums::ColorChangeType GetColorType() const { return colorType_; }
	void SetColorType(ParticleManagerEnums::ColorChangeType value) { colorType_ = value; }

	// Gradient colors
	const std::vector<Vector4>& GetGradientColors() const { return gradientColors_; }
	void SetGradientColors(const std::vector<Vector4>& value) { gradientColors_ = value; }

	const std::vector<float>& GetGradientTimes() const { return gradientTimes_; }
	void SetGradientTimes(const std::vector<float>& value) { gradientTimes_ = value; }

	bool GetRandomStartColor() const { return randomStartColor_; }
	void SetRandomStartColor(bool random) { randomStartColor_ = random; }

	//************************* アルファ *************************//
	float GetAlphaFadeInTime() const { return alphaFadeInTime_; }
	void SetAlphaFadeInTime(float value) { alphaFadeInTime_ = value; }

	float GetAlphaFadeOutTime() const { return alphaFadeOutTime_; }
	void SetAlphaFadeOutTime(float value) { alphaFadeOutTime_ = value; }

	//************************* 加速度 *************************//
	const Vector3& GetBaseVelocity() const { return baseVelocity_; }
	void SetBaseVelocity(const Vector3& value) { baseVelocity_ = value; }

	const Vector3& GetVelocityVariation() const { return velocityVariation_; }
	void SetVelocityVariation(const Vector3& value) { velocityVariation_ = value; }

	bool GetRandomDirection() const { return randomDirection_; }
	void SetRandomDirection(bool value) { randomDirection_ = value; }

	float GetSpeed() const { return speed_; }
	void SetSpeed(float value) { speed_ = value; }

	float GetSpeedVariation() const { return speedVariation_; }
	void SetSpeedVariation(float value) { speedVariation_ = value; }

	//************************* 時間経過に伴う速度 *************************//
	bool GetVelocityOverTime() const { return velocityOverTime_; }
	void SetVelocityOverTime(bool value) { velocityOverTime_ = value; }

	const Vector3& GetVelocityOverTimeMultiplier() const { return velocityOverTimeMultiplier_; }
	void SetVelocityOverTimeMultiplier(const Vector3& value) { velocityOverTimeMultiplier_ = value; }

	//************************* SRT *************************//
	const Vector3& GetScaleMin() const { return scaleMin_; }
	void SetScaleMin(const Vector3& value) { scaleMin_ = value; }

	const Vector3& GetScaleMax() const { return scaleMax_; }
	void SetScaleMax(const Vector3& value) { scaleMax_ = value; }

	bool GetSizeOverTime() const { return sizeOverTime_; }
	void SetSizeOverTime(bool value) { sizeOverTime_ = value; }

	float GetSizeMultiplierStart() const { return sizeMultiplierStart_; }
	void SetSizeMultiplierStart(float value) { sizeMultiplierStart_ = value; }

	float GetSizeMultiplierEnd() const { return sizeMultiplierEnd_; }
	void SetSizeMultiplierEnd(float value) { sizeMultiplierEnd_ = value; }

	const Vector3& GetRotateMin() const { return rotateMin_; }
	void SetRotateMin(const Vector3& value) { rotateMin_ = value; }

	const Vector3& GetRotateMax() const { return rotateMax_; }
	void SetRotateMax(const Vector3& value) { rotateMax_ = value; }

	float GetAngularVelocityMin() const { return angularVelocityMin_; }
	void SetAngularVelocityMin(float value) { angularVelocityMin_ = value; }

	float GetAngularVelocityMax() const { return angularVelocityMax_; }
	void SetAngularVelocityMax(float value) { angularVelocityMax_ = value; }

	//************************* エミッション *************************//
	ParticleManagerEnums::EmissionType GetEmissionType() const { return emissionType_; }
	void SetEmissionType(ParticleManagerEnums::EmissionType value) { emissionType_ = value; }

	float GetEmissionRadius() const { return emissionRadius_; }
	void SetEmissionRadius(float value) { emissionRadius_ = value; }

	const Vector3& GetEmissionSize() const { return emissionSize_; }
	void SetEmissionSize(const Vector3& value) { emissionSize_ = value; }

	float GetEmissionAngle() const { return emissionAngle_; }
	void SetEmissionAngle(float value) { emissionAngle_ = value; }

	float GetEmissionHeight() const { return emissionHeight_; }
	void SetEmissionHeight(float value) { emissionHeight_ = value; }

	//************************* バースト *************************//
	bool GetBurstEnabled() const { return burstEnabled_; }
	void SetBurstEnabled(bool value) { burstEnabled_ = value; }

	int GetBurstCount() const { return burstCount_; }
	void SetBurstCount(int value) { burstCount_ = value; }

	float GetBurstInterval() const { return burstInterval_; }
	void SetBurstInterval(float value) { burstInterval_ = value; }

	//************************* 描画 *************************//
	BlendMode GetBlendMode() const { return blendMode_; }
	void SetBlendMode(BlendMode value) { blendMode_ = value; }

	bool GetEnableBillboard() const { return enableBillboard_; }
	void SetEnableBillboard(bool value) { enableBillboard_ = value; }

	const Vector3& GetOffset() const { return offset_; }
	void SetOffset(const Vector3& value) { offset_ = value; }

	//************************* UV *************************//
	const Vector2& GetUVScale() const { return uvScale_; }
	void SetUVScale(const Vector2& value) { uvScale_ = value; }

	const Vector2& GetUVTranslate() const { return uvTranslate_; }
	void SetUVTranslate(const Vector2& value) { uvTranslate_ = value; }

	float GetUVRotate() const { return uvRotate_; }
	void SetUVRotate(float value) { uvRotate_ = value; }

	bool GetUVAnimationEnabled() const { return uvAnimationEnabled_; }
	void SetUVAnimationEnabled(bool value) { uvAnimationEnabled_ = value; }

	Vector2 GetUVAnimationSpeed() const { return uvAnimationSpeed_; }
	void SetUVAnimationSpeed(const Vector2& value) { uvAnimationSpeed_ = value; }









	Vector2 GetUVBaseScale() const { return uvBaseScale_; }
	void SetUVBaseScale(const Vector2& scale) { uvBaseScale_ = scale; }

	Vector2 GetUVBaseTranslate() const { return uvBaseTranslate_; }
	void SetUVBaseTranslate(const Vector2& translate) { uvBaseTranslate_ = translate; }

	float GetUVBaseRotation() const { return uvBaseRotation_; }
	void SetUVBaseRotation(float rotation) { uvBaseRotation_ = rotation; }

	// システム全体のUVスクロール
	bool GetSystemUVScrollEnabled() const { return systemUVScrollEnabled_; }
	void SetSystemUVScrollEnabled(bool enabled) { systemUVScrollEnabled_ = enabled; }

	Vector2 GetSystemUVScrollSpeed() const { return systemUVScrollSpeed_; }
	void SetSystemUVScrollSpeed(const Vector2& speed) { systemUVScrollSpeed_ = speed; }

	// システム全体のUV回転アニメーション
	bool GetUVRotationEnabled() const { return uvRotationEnabled_; }
	void SetUVRotationEnabled(bool enabled) { uvRotationEnabled_ = enabled; }

	float GetUVRotationSpeed() const { return uvRotationSpeed_; }
	void SetUVRotationSpeed(float speed) { uvRotationSpeed_ = speed; }

	// システム全体のUVスケールアニメーション
	bool GetUVScaleAnimationEnabled() const { return uvScaleAnimationEnabled_; }
	void SetUVScaleAnimationEnabled(bool enabled) { uvScaleAnimationEnabled_ = enabled; }

	float GetUVScaleAnimationSpeed() const { return uvScaleAnimationSpeed_; }
	void SetUVScaleAnimationSpeed(float speed) { uvScaleAnimationSpeed_ = speed; }

	float GetUVScaleAnimationAmount() const { return uvScaleAnimationAmount_; }
	void SetUVScaleAnimationAmount(float amount) { uvScaleAnimationAmount_ = amount; }


	//************************* テクスチャ *************************//
	bool GetTextureSheetEnabled() const { return textureSheetEnabled_; }
	void SetTextureSheetEnabled(bool value) { textureSheetEnabled_ = value; }

	Vector2 GetTextureSheetTiles() const { return textureSheetTiles_; }
	void SetTextureSheetTiles(const Vector2& value) { textureSheetTiles_ = value; }

	float GetTextureSheetFrameRate() const { return textureSheetFrameRate_; }
	void SetTextureSheetFrameRate(float value) { textureSheetFrameRate_ = value; }

	//************************* トレイル（まだ使えない） *************************//

	bool GetTrailEnabled() const { return trailEnabled_; }
	void SetTrailEnabled(bool value) { trailEnabled_ = value; }

	int GetTrailLength() const { return trailLength_; }
	void SetTrailLength(int value) { trailLength_ = value; }

	float GetTrailWidth() const { return trailWidth_; }
	void SetTrailWidth(float value) { trailWidth_ = value; }

	Vector4 GetTrailColor() const { return trailColor_; }
	void SetTrailColor(const Vector4& value) { trailColor_ = value; }

	float GetTrailFadeSpeed() const { return trailFadeSpeed_; }
	void SetTrailFadeSpeed(float value) { trailFadeSpeed_ = value; }

	bool GetTrailWorldSpace() const { return trailWorldSpace_; }
	void SetTrailWorldSpace(bool value) { trailWorldSpace_ = value; }

	float GetTrailSegmentDistance() const { return trailSegmentDistance_; }
	void SetTrailSegmentDistance(float value) { trailSegmentDistance_ = value; }

	//************************* 力 *************************//
	bool GetForceOverTime() const { return forceOverTime_; }
	void SetForceOverTime(bool value) { forceOverTime_ = value; }

	Vector3 GetForceVector() const { return forceVector_; }
	void SetForceVector(const Vector3& value) { forceVector_ = value; }


	bool GetVortexEnabled() const { return vortexEnabled_; }
	void SetVortexEnabled(bool value) { vortexEnabled_ = value; }

	Vector3 GetVortexCenter() const { return vortexCenter_; }
	void SetVortexCenter(const Vector3& value) { vortexCenter_ = value; }

	float GetVortexStrength() const { return vortexStrength_; }
	void SetVortexStrength(float value) { vortexStrength_ = value; }

	float GetVortexRadius() const { return vortexRadius_; }
	void SetVortexRadius(float value) { vortexRadius_ = value; }

	//************************* 詳細な設定 *************************//
	bool GetInheritTransformVelocity() const { return inheritTransformVelocity_; }
	void SetInheritTransformVelocity(bool value) { inheritTransformVelocity_ = value; }

	float GetInheritVelocityMultiplier() const { return inheritVelocityMultiplier_; }
	void SetInheritVelocityMultiplier(float value) { inheritVelocityMultiplier_ = value; }

	//************************* カリング（まだ使えない） *************************//
	bool GetCullingEnabled() const { return cullingEnabled_; }
	void SetCullingEnabled(bool value) { cullingEnabled_ = value; }

	float GetCullingDistance() const { return cullingDistance_; }
	void SetCullingDistance(float value) { cullingDistance_ = value; }

	//************************* LOD *************************//
	bool GetLODEnabled() const { return lodEnabled_; }
	void SetLODEnabled(bool value) { lodEnabled_ = value; }
	float GetLODDistance1() const { return lodDistance1_; }
	void SetLODDistance1(float value) { lodDistance1_ = value; }
	float GetLODDistance2() const { return lodDistance2_; }
	void SetLODDistance2(float value) { lodDistance2_ = value; }

	//************************* 当たり判定（調整中） *************************//
	float GetCollisionRestitution() const { return collisionRestitution_; }
	void SetCollisionRestitution(float restitution) { collisionRestitution_ = restitution; }

	float GetCollisionFriction() const { return collisionFriction_; }
	void SetCollisionFriction(float friction) { collisionFriction_ = friction; }

	// コーン角度
	float GetConeAngle() const { return coneAngle_; }
	void SetConeAngle(float angle) { coneAngle_ = angle; }

	// 質量範囲
	const Vector2& GetMassRange() const { return massRange_; }
	void SetMassRange(const Vector2& range) { massRange_ = range; }

	// ランダム開始色

	//************************* ライティング設定 *************************//
	bool GetEnableLighting() const { return enableLighting_; }
	void SetEnableLighting(bool enable) { enableLighting_ = enable; }

private:
	// ===== 基本設定 =====
	int kMaxParticles_ = 1000;
	float emissionRate_ = 10.0f;
	Vector2 lifeTimeRange_ = { 1.0f, 5.0f };
	bool looping_ = true;
	float duration_ = 5.0f;
	float startDelay_ = 0.0f;

	// ===== 物理設定 =====
	Vector3 gravity_ = { 0.0f, -9.8f, 0.0f };
	float drag_ = 0.1f;
	float mass_ = 1.0f;
	float bounciness_ = 0.0f;
	float friction_ = 0.0f;
	bool collisionEnabled_ = false;
	bool isPhysicsEnabled_ = false;
	float collisionRadius_ = 0.5f;
	float collisionRestitution_;                      // 反発係数
	float collisionFriction_;                         // 摩擦係数
	Vector2 massRange_;

	// ===== ノイズなど =====
	bool turbulenceEnabled_ = false;
	float turbulenceStrength_ = 1.0f;
	float turbulenceFrequency_ = 1.0f;
	Vector3 noiseScale_ = { 1.0f, 1.0f, 1.0f };
	float noiseSpeed_ = 1.0f;

	// ===== 色 =====
	Vector4 systemColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 startColor_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 endColor_ = { 1.0f, 1.0f, 1.0f, 0.0f };
	ParticleManagerEnums::ColorChangeType colorType_ = ParticleManagerEnums::ColorChangeType::Fade;
	std::vector<Vector4> gradientColors_;
	std::vector<float> gradientTimes_;
	float alphaFadeInTime_ = 0.0f;
	float alphaFadeOutTime_ = 1.0f;
	bool randomStartColor_; // ランダム開始色


	// ===== 速度 =====
	Vector3 baseVelocity_ = { 0, 1, 0 };
	Vector3 velocityVariation_ = { 1, 1, 1 };
	bool randomDirection_ = false;
	float speed_ = 1.0f;
	float speedVariation_ = 0.0f;
	bool velocityOverTime_ = false;
	Vector3 velocityOverTimeMultiplier_ = { 1.0f, 1.0f, 1.0f };

	// ===== スケールなど =====
	Vector3 scaleMin_ = { 1, 1, 1 };
	Vector3 scaleMax_ = { 1, 1, 1 };
	bool sizeOverTime_ = false;
	float sizeMultiplierStart_ = 1.0f;
	float sizeMultiplierEnd_ = 1.0f;
	Vector3 rotateMin_ = { 0, 0, 0 };
	Vector3 rotateMax_ = { 0, 0, 0 };
	float angularVelocityMin_ = 0.0f;
	float angularVelocityMax_ = 0.0f;

	// ランダム回転設定
	bool randomRotationEnabled_;              // ランダム回転有効フラグ
	Vector3 randomRotationRange_;             // 各軸のランダム回転範囲（度数）
	Vector3 randomRotationSpeed_;             // 各軸のランダム回転速度範囲
	bool inheritInitialRotation_;             // 初期回転を継承するか
	bool randomRotationPerAxis_;              // 軸ごとに独立してランダム化

	// 時間経過による回転変化
	bool rotationOverTime_;                   // 時間経過回転有効
	Vector3 rotationAcceleration_;            // 回転加速度
	float rotationDamping_;                   // 回転減衰率

	// ===== エミッション =====
	ParticleManagerEnums::EmissionType emissionType_ = ParticleManagerEnums::EmissionType::Point;
	float emissionRadius_ = 1.0f;
	Vector3 emissionSize_ = { 1, 1, 1 };
	float emissionAngle_ = 25.0f;
	float emissionHeight_ = 1.0f;
	bool burstEnabled_ = false;
	int burstCount_ = 30;
	float burstInterval_ = 2.0f;
	float coneAngle_;


	// ===== 描画設定 =====
	BlendMode blendMode_ = BlendMode::kBlendModeAdd;
	bool enableBillboard_ = true;


	Vector3 offset_ = { 0, 0, 0 };
	Vector2 uvScale_ = { 1, 1 };
	Vector2 uvTranslate_ = { 0, 0 };
	float uvRotate_ = 0.0f;
	bool uvAnimationEnabled_ = false;
	Vector2 uvAnimationSpeed_ = { 0.0f, 0.0f };

	Vector2 uvBaseScale_ = { 1.0f, 1.0f };      // 基本UVスケール
	Vector2 uvBaseTranslate_ = { 0.0f, 0.0f };  // 基本UVオフセット
	float uvBaseRotation_ = 0.0f;               // 基本UV回転角

	// システム全体のUVスクロール
	bool systemUVScrollEnabled_ = false;        // システム全体のUVスクロール有効
	Vector2 systemUVScrollSpeed_ = { 0.0f, 0.0f }; // システム全体のUVスクロール速度

	// システム全体のUV回転アニメーション
	bool uvRotationEnabled_ = false;            // システム全体のUV回転アニメーション有効
	float uvRotationSpeed_ = 0.0f;              // システム全体のUV回転速度（rad/sec）

	// システム全体のUVスケールアニメーション
	bool uvScaleAnimationEnabled_ = false;      // システム全体のUVスケールアニメーション有効
	float uvScaleAnimationSpeed_ = 1.0f;        // システム全体のUVスケールアニメーション速度
	float uvScaleAnimationAmount_ = 0.1f;       // システム全体のUVスケールアニメーション振幅



	bool textureSheetEnabled_ = false;
	Vector2 textureSheetTiles_ = { 1, 1 };
	float textureSheetFrameRate_ = 30.0f;

	// トレイル設定
	bool trailEnabled_ = false;
	int trailLength_ = 10;
	float trailWidth_ = 0.1f;
	Vector4 trailColor_ = { 1, 1, 1, 0.5f };
	float trailFadeSpeed_ = 1.0f;           // フェード速度
	bool trailWorldSpace_ = true;           // ワールド空間でのトレイル
	float trailSegmentDistance_ = 0.1f;     // セグメント間の最小距離

	// ===== 力 =====
	bool forceOverTime_ = false;
	Vector3 forceVector_ = { 0, 0, 0 };
	bool vortexEnabled_ = false;
	Vector3 vortexCenter_ = { 0, 0, 0 };
	float vortexStrength_ = 1.0f;
	float vortexRadius_ = 5.0f;

	// ===== 高度な設定 =====
	bool inheritTransformVelocity_ = false;
	float inheritVelocityMultiplier_ = 1.0f;
	bool cullingEnabled_ = true;
	float cullingDistance_ = 100.0f;
	bool lodEnabled_ = false;
	float lodDistance1_ = 25.0f;
	float lodDistance2_ = 50.0f;

	// ===== ライティング =====
	bool enableLighting_ = false;
};