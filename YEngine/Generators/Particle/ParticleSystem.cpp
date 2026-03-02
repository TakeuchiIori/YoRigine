#include "ParticleSystem.h"

// Engine
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Loaders/Texture/TextureManager.h"
#include "Systems/Camera/Camera.h"
#include "Mesh/Mesh.h"

//C++
#include <numbers>
#include <cassert>
#include <algorithm>

ParticleSystem::ParticleSystem(const std::string& name)
    : name_(name), emissionTimer_(0.0f), systemTime_(0.0f), isActive_(true), hasStarted_(false),
    systemPosition_(Vector3{ 0, 0, 0 }), systemRotation_(Vector3{ 0, 0, 0 }), systemVelocity_(Vector3{ 0, 0, 0 }),
    previousSystemPosition_(Vector3{ 0, 0, 0 }), burstTimer_(0.0f), burstCount_(0),
    textureIndexSRV_(0), srvIndex_(0), instancingDataForGPU_(nullptr),
    randomEngine_(randomDevice_()) {
    particles_.reserve(1000);
    instancingData_.reserve(kMaxInstances_);
}

void ParticleSystem::InitializeResources(SrvManager* srvManager) {
    auto dxCommon = YoRigine::DirectXCommon::GetInstance();

    // インスタンシング用リソース作成
    instancingResource_ = dxCommon->CreateBufferResource(sizeof(ParticleForGPU) * kMaxInstances_);

    // SRVインデックス取得
    srvIndex_ = srvManager->Allocate();

    // GPUメモリにマップ
    instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingDataForGPU_));

    // SRV作成
    srvManager->CreateSRVforStructuredBuffer(
        srvIndex_,
        instancingResource_.Get(),
        kMaxInstances_,
        sizeof(ParticleForGPU)
    );

    // インスタンシングデータ初期化
    instancingData_.resize(kMaxInstances_);

    // 初期化フラグ
    hasStarted_ = false;
    systemTime_ = 0.0f;
    previousSystemPosition_ = systemPosition_;
}

void ParticleSystem::Finalize() {
    if (instancingResource_) {
        instancingResource_->Unmap(0, nullptr);
        instancingResource_.Reset();
    }
    instancingDataForGPU_ = nullptr;
}

void ParticleSystem::SetTexture(const std::string& textureFilePath) {
    textureFilePath_ = textureFilePath;

    if (!textureFilePath.empty()) {
        // テクスチャ読み込み
        TextureManager::GetInstance()->LoadTexture(textureFilePath);
        textureIndexSRV_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(textureFilePath);
    }
}

void ParticleSystem::Update(float deltaTime) {
    if (!isActive_) return;

    // システム時間更新
    systemTime_ += deltaTime;

    // 開始遅延チェック
    if (!hasStarted_) {
        if (systemTime_ >= settings_.GetStartDelay()) {
            hasStarted_ = true;
            systemTime_ = 0.0f; // リセットして実際の動作開始
        } else {
            return; // まだ開始時刻ではない
        }
    }

    // 持続時間チェック
    if (!settings_.GetLooping() && systemTime_ >= settings_.GetDuration()) {
        isActive_ = false;
        return;
    }

    // システム速度計算（速度継承用）
    systemVelocity_ = (systemPosition_ - previousSystemPosition_) / deltaTime;
    previousSystemPosition_ = systemPosition_;

    RemoveDeadParticles();
    UpdateParticles(deltaTime);

    UpdateMaterialInfo();
}

void ParticleSystem::UpdateEmission(float deltaTime) {
    // バースト処理
    if (settings_.GetBurstEnabled()) {
        burstTimer_ += deltaTime;
        if (burstTimer_ >= settings_.GetBurstInterval()) {
            EmitBurst(systemPosition_, settings_.GetBurstCount());
            burstTimer_ = 0.0f;
        }
    }

    // 通常エミッション
    emissionTimer_ += deltaTime;
    float emissionInterval = 1.0f / settings_.GetEmissionRate();

    while (emissionTimer_ >= emissionInterval && particles_.size() < settings_.GetMaxParticles()) {
        Emit(systemPosition_);
        emissionTimer_ -= emissionInterval;
    }
}

void ParticleSystem::UpdateParticles(float deltaTime) {
    for (auto& particle : particles_) {
        // 年齢更新
        particle.currentTime += deltaTime;
        particle.age = particle.currentTime / particle.lifeTime;

        // 各種更新処理
        UpdatePhysics(particle, deltaTime);
        UpdateRotation(particle, deltaTime);
        UpdateVelocity(particle, deltaTime);
        UpdateForces(particle, deltaTime);
        UpdateColor(particle);
        UpdateSize(particle);
        UpdateAlpha(particle);
        UpdateUV(particle, deltaTime);
        UpdateTextureSheet(particle, deltaTime);
        UpdateTrail(particle, deltaTime);

        // 位置更新（最後に実行）
        particle.position += particle.velocity * deltaTime;
    }
}

void ParticleSystem::UpdatePhysics(ParticleData& particle, float deltaTime) {
    if (!settings_.GetIsPhysicsEnabled()) return;

    // 重力適用
    ApplyGravity(particle, deltaTime);

    // 空気抵抗
    ApplyDrag(particle, deltaTime);

    // 乱流
    if (settings_.GetTurbulenceEnabled()) {
        ApplyTurbulence(particle, deltaTime);
    }
}

void ParticleSystem::UpdateRotation(ParticleData& particle, float deltaTime) {
    if (!settings_.GetRandomRotationEnabled()) {
        // 従来の角速度による回転
        particle.rotation += particle.angularVelocity * deltaTime;
        return;
    }

    // ランダム回転システム
    UpdateRotationVelocity(particle, deltaTime);

    // 回転更新
    particle.rotation.x += particle.rotationVelocity.x * deltaTime;
    particle.rotation.y += particle.rotationVelocity.y * deltaTime;
    particle.rotation.z += particle.rotationVelocity.z * deltaTime;

    // 角度正規化（-π〜π）
    auto normalizeAngle = [](float angle) {
        while (angle > std::numbers::pi_v<float>) angle -= 2.0f * std::numbers::pi_v<float>;
        while (angle < -std::numbers::pi_v<float>) angle += 2.0f * std::numbers::pi_v<float>;
        return angle;
        };

    particle.rotation.x = normalizeAngle(particle.rotation.x);
    particle.rotation.y = normalizeAngle(particle.rotation.y);
    particle.rotation.z = normalizeAngle(particle.rotation.z);
}

void ParticleSystem::UpdateVelocity(ParticleData& particle, float deltaTime) {
    // 時間経過による速度変化
    if (settings_.GetVelocityOverTime()) {
        Vector3 multiplier = settings_.GetVelocityOverTimeMultiplier();

        // 年齢に基づく補間
        float t = particle.age;
        particle.velocity.x = particle.initVelocity.x * (1.0f + (multiplier.x - 1.0f) * t);
        particle.velocity.y = particle.initVelocity.y * (1.0f + (multiplier.y - 1.0f) * t);
        particle.velocity.z = particle.initVelocity.z * (1.0f + (multiplier.z - 1.0f) * t);
    }

    // 速度バリエーション（ランダム要素追加）
    if (settings_.GetSpeedVariation() > 0.0f) {
        float variation = settings_.GetSpeedVariation() * SmoothStep(particle.age);
        Vector3 randomOffset = {
            GetRandomFloat(-variation, variation),
            GetRandomFloat(-variation, variation),
            GetRandomFloat(-variation, variation)
        };
        particle.velocity += randomOffset * deltaTime;
    }
}

void ParticleSystem::UpdateForces(ParticleData& particle, float deltaTime) {
    // 時間経過による力
    if (settings_.GetForceOverTime()) {
        Vector3 force = settings_.GetForceVector();
        particle.velocity += force * deltaTime / particle.mass;
    }

    // 渦力
    if (settings_.GetVortexEnabled()) {
        ApplyVortex(particle, deltaTime);
    }

}

void ParticleSystem::UpdateColor(ParticleData& particle) {
    switch (settings_.GetColorType()) {
    case ParticleManagerEnums::ColorChangeType::None:
        particle.color = settings_.GetStartColor();
        break;
    case ParticleManagerEnums::ColorChangeType::Fade:
        UpdateFadeColor(particle);
        break;
    case ParticleManagerEnums::ColorChangeType::Fire:
        UpdateFireColor(particle);
        break;
    case ParticleManagerEnums::ColorChangeType::Rainbow:
        UpdateRainbowColor(particle);
        break;
    case ParticleManagerEnums::ColorChangeType::Flash:
        UpdateFlashColor(particle);
        break;
    case ParticleManagerEnums::ColorChangeType::Gradient:
        UpdateGradientColor(particle);
        break;
    case ParticleManagerEnums::ColorChangeType::Electric:
        UpdateElectricColor(particle);
        break;
    default:
        particle.color = settings_.GetStartColor();
        break;
    }
}

void ParticleSystem::UpdateSize(ParticleData& particle) {
    if (settings_.GetSizeOverTime()) {
        float startMultiplier = settings_.GetSizeMultiplierStart();
        float endMultiplier = settings_.GetSizeMultiplierEnd();

        float sizeMultiplier = startMultiplier + (endMultiplier - startMultiplier) * EaseInOut(particle.age);
        particle.scale = particle.initScale * sizeMultiplier;
    }
}

void ParticleSystem::UpdateAlpha(ParticleData& particle) {
    float alpha = 1.0f;

    // フェードイン
    float fadeInTime = settings_.GetAlphaFadeInTime();
    if (fadeInTime > 0.0f && particle.currentTime < fadeInTime) {
        alpha *= particle.currentTime / fadeInTime;
    }

    // フェードアウト
    float fadeOutTime = settings_.GetAlphaFadeOutTime();
    if (fadeOutTime > 0.0f) {
        float fadeOutStart = particle.lifeTime - fadeOutTime;
        if (particle.currentTime > fadeOutStart) {
            float fadeProgress = (particle.currentTime - fadeOutStart) / fadeOutTime;
            alpha *= (1.0f - fadeProgress);
        }
    }

    particle.color.w = particle.initColor.w * alpha;
}

void ParticleSystem::UpdateUV(ParticleData& particle, float deltaTime) {
    if (settings_.GetUVAnimationEnabled()) {
        Vector2 animSpeed = settings_.GetUVAnimationSpeed();
        particle.uvOffset.x += animSpeed.x * deltaTime;
        particle.uvOffset.y += animSpeed.y * deltaTime;

        // UV座標をラップ
        particle.uvOffset.x = std::fmod(particle.uvOffset.x, 1.0f);
        particle.uvOffset.y = std::fmod(particle.uvOffset.y, 1.0f);
    }
}

void ParticleSystem::UpdateTextureSheet(ParticleData& particle, [[maybe_unused]] float deltaTime) {
    if (settings_.GetTextureSheetEnabled()) {
        Vector2 tiles = settings_.GetTextureSheetTiles();
        float frameRate = settings_.GetTextureSheetFrameRate();

        int totalFrames = static_cast<int>(tiles.x * tiles.y);
        float frameTime = 1.0f / frameRate;

        int currentFrame = static_cast<int>(particle.currentTime / frameTime) % totalFrames;

        particle.textureSheetIndex.x = static_cast<float>(currentFrame % static_cast<int>(tiles.x));
        particle.textureSheetIndex.y = static_cast<float>(currentFrame / static_cast<int>(tiles.x));
    }
}

void ParticleSystem::UpdateTrail(ParticleData& particle, float deltaTime) {
    if (!settings_.GetTrailEnabled()) return;

    particle.trailTimer += deltaTime;

    // トレイル初期化
    if (!particle.trailInitialized) {
        particle.lastTrailPosition = particle.position;
        particle.trailInitialized = true;
        return;
    }

    // 移動距離チェック
    float distanceMoved = Length(particle.position - particle.lastTrailPosition);
    if (distanceMoved >= settings_.GetTrailSegmentDistance()) {

        // 新しいセグメントを追加
        TrailSegment segment;
        segment.position = particle.position;
        segment.age = 0.0f;
        segment.width = settings_.GetTrailWidth();
        segment.color = settings_.GetTrailColor();

        particle.trailSegments.push_back(segment);
        particle.lastTrailPosition = particle.position;

        // 最大長を超えた場合は古いセグメントを削除
        if (particle.trailSegments.size() > static_cast<size_t>(settings_.GetTrailLength())) {
            particle.trailSegments.erase(particle.trailSegments.begin());
        }
    }

    // 既存セグメントの更新
    for (auto it = particle.trailSegments.begin(); it != particle.trailSegments.end();) {
        it->age += deltaTime;

        // フェード処理
        float fadeProgress = it->age * settings_.GetTrailFadeSpeed();
        if (fadeProgress >= 1.0f) {
            it = particle.trailSegments.erase(it);
        } else {
            // アルファフェード
            it->color.w = settings_.GetTrailColor().w * (1.0f - fadeProgress);
            // 幅の縮小
            it->width = settings_.GetTrailWidth() * (1.0f - fadeProgress * 0.5f);
            ++it;
        }
    }
}

// 色更新関数の実装
void ParticleSystem::UpdateFadeColor(ParticleData& particle) {
    particle.color = LerpColor(settings_.GetStartColor(), settings_.GetEndColor(), particle.age);
}

void ParticleSystem::UpdateFireColor(ParticleData& particle) {
    float t = particle.age;
    if (t < 0.33f) {
        particle.color = LerpColor(Vector4{ 1, 0.2f, 0, 1 }, Vector4{ 1, 0.6f, 0, 1 }, t * 3.0f);
    } else if (t < 0.66f) {
        particle.color = LerpColor(Vector4{ 1, 0.6f, 0, 1 }, Vector4{ 1, 1, 0.2f, 1 }, (t - 0.33f) * 3.0f);
    } else {
        particle.color = LerpColor(Vector4{ 1, 1, 0.2f, 1 }, Vector4{ 0.5f, 0.5f, 0.5f, 0 }, (t - 0.66f) * 3.0f);
    }
}

void ParticleSystem::UpdateRainbowColor(ParticleData& particle) {
    float hue = std::fmod(particle.currentTime * 0.5f + particle.age, 1.0f);
    particle.color = HSVtoRGB(hue, 1.0f, 1.0f, particle.initColor.w * (1.0f - particle.age * 0.5f));
}

void ParticleSystem::UpdateFlashColor(ParticleData& particle) {
    float flash = std::sin(particle.currentTime * 10.0f) * 0.5f + 0.5f;
    Vector4 flashColor = LerpColor(settings_.GetStartColor(), settings_.GetEndColor(), flash);
    particle.color = LerpColor(particle.initColor, flashColor, particle.age);
}

void ParticleSystem::UpdateGradientColor(ParticleData& particle) {
    // 複数色のグラデーション
    Vector4 colors[] = {
        settings_.GetStartColor(),
        Vector4{1.0f, 0.5f, 0.0f, 1.0f}, // オレンジ
        Vector4{1.0f, 1.0f, 0.0f, 1.0f}, // 黄色
        settings_.GetEndColor()
    };

    float t = particle.age * 3.0f; // 4色なので3セグメント
    int index = static_cast<int>(t);
    float localT = t - index;

    if (index >= 3) {
        particle.color = colors[3];
    } else {
        particle.color = LerpColor(colors[index], colors[index + 1], localT);
    }
}

void ParticleSystem::UpdateElectricColor(ParticleData& particle) {
    // 電気のような激しい色変化
    float noise = PerlinNoise(particle.position.x * 0.1f, particle.position.y * 0.1f, particle.position.z * 0.1f, systemTime_ * 5.0f);
    float electric = std::abs(noise) * 2.0f;

    Vector4 baseColor = LerpColor(Vector4{ 0.0f, 0.5f, 1.0f, 1.0f }, Vector4{ 1.0f, 1.0f, 1.0f, 1.0f }, electric);
    particle.color = LerpColor(baseColor, settings_.GetEndColor(), particle.age);
}

Vector4 ParticleSystem::HSVtoRGB(float h, float s, float v, float a) {
    float c = v * s;
    float x = static_cast<float>(c * (1 - std::abs(std::fmod(h * 6, 2) - 1)));
    float m = v - c;

    float r, g, b;
    if (h < 1.0f / 6.0f) { r = c; g = x; b = 0; } else if (h < 2.0f / 6.0f) { r = x; g = c; b = 0; } else if (h < 3.0f / 6.0f) { r = 0; g = c; b = x; } else if (h < 4.0f / 6.0f) { r = 0; g = x; b = c; } else if (h < 5.0f / 6.0f) { r = x; g = 0; b = c; } else { r = c; g = 0; b = x; }

    return Vector4{ r + m, g + m, b + m, a };
}

// 物理計算の実装
void ParticleSystem::ApplyGravity(ParticleData& particle, float deltaTime) {
    particle.velocity += settings_.GetGravity() * deltaTime;
}

void ParticleSystem::ApplyDrag(ParticleData& particle, float deltaTime) {
    float drag = settings_.GetDrag();
    particle.velocity *= (1.0f - drag * deltaTime);
}

void ParticleSystem::ApplyTurbulence(ParticleData& particle, float deltaTime) {
    Vector3 noiseScale = settings_.GetNoiseScale();
    float noiseSpeed = settings_.GetNoiseSpeed();
    float turbulenceStrength = settings_.GetTurbulenceStrength();

    float noiseX = PerlinNoise(particle.position.x * noiseScale.x, particle.position.y * noiseScale.y, particle.position.z * noiseScale.z, systemTime_ * noiseSpeed);
    float noiseY = PerlinNoise(particle.position.y * noiseScale.y, particle.position.z * noiseScale.z, particle.position.x * noiseScale.x, systemTime_ * noiseSpeed + 100.0f);
    float noiseZ = PerlinNoise(particle.position.z * noiseScale.z, particle.position.x * noiseScale.x, particle.position.y * noiseScale.y, systemTime_ * noiseSpeed + 200.0f);

    Vector3 turbulence = { noiseX, noiseY, noiseZ };
    turbulence = turbulence * turbulenceStrength;

    particle.velocity += turbulence * deltaTime;
}

void ParticleSystem::ApplyVortex(ParticleData& particle, float deltaTime) {
    Vector3 vortexCenter = settings_.GetVortexCenter();
    float vortexStrength = settings_.GetVortexStrength();
    float vortexRadius = settings_.GetVortexRadius();

    Vector3 toCenter = vortexCenter - particle.position;
    float distance = Length(toCenter);

    if (distance < vortexRadius && distance > 0.001f) {
        Vector3 direction = Normalize(toCenter);
        Vector3 tangent = Cross(direction, Vector3{ 0, 1, 0 });
        if (Length(tangent) < 0.001f) {
            tangent = Cross(direction, Vector3{ 1, 0, 0 });
        }
        tangent = Normalize(tangent);

        float strength = vortexStrength * (1.0f - distance / vortexRadius);
        particle.velocity += tangent * strength * deltaTime;
        particle.velocity += direction * strength * 0.1f * deltaTime; // 吸引力
    }
}


// 回転関連の実装
void ParticleSystem::InitializeRotation(ParticleData& particle) {
    if (!settings_.GetRandomRotationEnabled()) return;

    Vector3 rotationRange = settings_.GetRandomRotationRange();
    Vector3 rotationSpeed = settings_.GetRandomRotationSpeed();

    if (settings_.GetRandomRotationPerAxis()) {
        // 各軸独立でランダム設定
        particle.rotation.x = GetRandomFloat(-rotationRange.x, rotationRange.x);
        particle.rotation.y = GetRandomFloat(-rotationRange.y, rotationRange.y);
        particle.rotation.z = GetRandomFloat(-rotationRange.z, rotationRange.z);

        particle.rotationVelocity.x = GetRandomFloat(-rotationSpeed.x, rotationSpeed.x);
        particle.rotationVelocity.y = GetRandomFloat(-rotationSpeed.y, rotationSpeed.y);
        particle.rotationVelocity.z = GetRandomFloat(-rotationSpeed.z, rotationSpeed.z);
    } else {
        // 統一ランダム設定
        float randomRotation = GetRandomFloat(-rotationRange.x, rotationRange.x);
        float randomSpeed = GetRandomFloat(-rotationSpeed.x, rotationSpeed.x);

        particle.rotation = Vector3{ randomRotation, randomRotation, randomRotation };
        particle.rotationVelocity = Vector3{ randomSpeed, randomSpeed, randomSpeed };
    }

    // 初期回転継承
    if (settings_.GetInheritInitialRotation()) {
        particle.rotation += systemRotation_;
    }
}

void ParticleSystem::UpdateRotationVelocity(ParticleData& particle, float deltaTime) {
    if (!settings_.GetRotationOverTime()) return;

    Vector3 acceleration = settings_.GetRotationAcceleration();
    float damping = settings_.GetRotationDamping();

    // 加速度適用
    particle.rotationVelocity += acceleration * deltaTime;

    // 減衰適用
    particle.rotationVelocity *= (1.0f - damping * deltaTime);
}


void ParticleSystem::RemoveDeadParticles() {
    particles_.erase(
        std::remove_if(particles_.begin(), particles_.end(),
            [](const ParticleData& p) { return p.age >= 1.0f; }),
        particles_.end()
    );
}

void ParticleSystem::Emit(const Vector3& position, int count) {
    for (int i = 0; i < count && particles_.size() < settings_.GetMaxParticles(); ++i) {
        particles_.push_back(CreateParticle(position));
    }
}

void ParticleSystem::EmitBurst(const Vector3& position, int count) {
    Emit(position, count);
}

ParticleData ParticleSystem::CreateParticle(const Vector3& position) {
    ParticleData particle;

    // 位置設定
    particle.position = position + SampleEmissionShape() + settings_.GetOffset();
    particle.initPos = particle.position;

    // 速度設定
    particle.velocity = GenerateRandomVelocity();
    particle.initVelocity = particle.velocity;

    // 変換速度継承
    if (settings_.GetInheritTransformVelocity()) {
        float multiplier = settings_.GetInheritVelocityMultiplier();
        particle.velocity += systemVelocity_ * multiplier;
    }

    // 色・スケール・回転設定
    particle.color = GenerateRandomColor();
    particle.initColor = particle.color;
    particle.scale = GenerateRandomScale();
    particle.initScale = particle.scale;
    particle.rotation = GenerateRandomRotation();

    // 回転速度設定
    particle.rotationVelocity = GenerateRandomRotationVelocity();
    particle.angularVelocity = Vector3{ 0, 0, 0 }; // 従来の角速度（後方互換）

    // ランダム回転初期化
    InitializeRotation(particle);

    // 寿命設定
    particle.lifeTime = GenerateRandomLifeTime();
    particle.currentTime = 0.0f;
    particle.age = 0.0f;

    // 物理設定
    particle.mass = GenerateRandomMass();
    particle.hasCollided = false;

    // UV・テクスチャシート設定
    particle.uvOffset = Vector2{ 0.0f, 0.0f };
    particle.textureSheetIndex = Vector2{ 0.0f, 0.0f };

    // トレイル設定
    particle.trailPositions.clear();
    particle.trailPositions.reserve(settings_.GetTrailLength());
    particle.trailTimer = 0.0f;
    particle.lastTrailPosition = particle.position;
    particle.trailInitialized = false;

    return particle;
}

Vector3 ParticleSystem::SampleEmissionShape() {
    switch (settings_.GetEmissionType()) {
    case ParticleManagerEnums::EmissionType::Point:
        return Vector3{ 0, 0, 0 };

    case ParticleManagerEnums::EmissionType::Sphere: {
        float theta = GetRandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
        float phi = std::acos(1.0f - 2.0f * GetRandomFloat(0.0f, 1.0f));
        float r = settings_.GetEmissionRadius() * std::cbrt(GetRandomFloat(0.0f, 1.0f)); // 体積均等分布

        return Vector3{
            r * std::sin(phi) * std::cos(theta),
            r * std::cos(phi),
            r * std::sin(phi) * std::sin(theta)
        };
    }

    case ParticleManagerEnums::EmissionType::Hemisphere: {
        float theta = GetRandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
        float phi = std::acos(GetRandomFloat(0.0f, 1.0f)); // 半球
        float r = settings_.GetEmissionRadius() * std::cbrt(GetRandomFloat(0.0f, 1.0f));

        return Vector3{
            r * std::sin(phi) * std::cos(theta),
            r * std::cos(phi),
            r * std::sin(phi) * std::sin(theta)
        };
    }

    case ParticleManagerEnums::EmissionType::Box: {
        Vector3 size = settings_.GetEmissionSize();
        return Vector3{
            GetRandomFloat(-size.x * 0.5f, size.x * 0.5f),
            GetRandomFloat(-size.y * 0.5f, size.y * 0.5f),
            GetRandomFloat(-size.z * 0.5f, size.z * 0.5f)
        };
    }

    case ParticleManagerEnums::EmissionType::Circle: {
        float angle = GetRandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
        float r = settings_.GetEmissionRadius() * std::sqrt(GetRandomFloat(0.0f, 1.0f));
        return Vector3{
            r * std::cos(angle),
            0.0f,
            r * std::sin(angle)
        };
    }

    case ParticleManagerEnums::EmissionType::Ring: {
        float angle = GetRandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
        float innerRadius = settings_.GetEmissionRadius() * 0.5f;
        float outerRadius = settings_.GetEmissionRadius();
        float r = GetRandomFloat(innerRadius, outerRadius);
        return Vector3{
            r * std::cos(angle),
            0.0f,
            r * std::sin(angle)
        };
    }

    case ParticleManagerEnums::EmissionType::Cone: {
        float angle = GetRandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
        float coneAngle = settings_.GetConeAngle();
        float height = GetRandomFloat(0.0f, settings_.GetEmissionSize().y);
        float radius = height * std::tan(coneAngle) * GetRandomFloat(0.0f, 1.0f);

        return Vector3{
            radius * std::cos(angle),
            height,
            radius * std::sin(angle)
        };
    }

    case ParticleManagerEnums::EmissionType::Line: {
        Vector3 size = settings_.GetEmissionSize();
        float t = GetRandomFloat(0.0f, 1.0f);
        return Vector3{
            size.x * (t - 0.5f),
            0.0f,
            0.0f
        };
    }

    default:
        return Vector3{ 0, 0, 0 };
    }
}

Vector3 ParticleSystem::GenerateRandomVelocity() {
    if (settings_.GetRandomDirection()) {
        // ランダム方向
        Vector3 randomDir = GetRandomDirection();
        return randomDir * settings_.GetSpeed();
    } else {
        // 基本速度 + バリエーション
        Vector3 variation = settings_.GetVelocityVariation();
        return settings_.GetBaseVelocity() + Vector3{
            GetRandomFloat(-variation.x, variation.x),
            GetRandomFloat(-variation.y, variation.y),
            GetRandomFloat(-variation.z, variation.z)
        };
    }
}

Vector3 ParticleSystem::GenerateRandomScale() {
    return GetRandomVector3(settings_.GetScaleMin(), settings_.GetScaleMax());
}

Vector3 ParticleSystem::GenerateRandomRotation() {
    return GetRandomVector3(settings_.GetRotateMin(), settings_.GetRotateMax());
}

Vector3 ParticleSystem::GenerateRandomRotationVelocity() {
    if (!settings_.GetRandomRotationEnabled()) {
        return Vector3{ 0, 0, 0 };
    }

    Vector3 rotationSpeed = settings_.GetRandomRotationSpeed();

    if (settings_.GetRandomRotationPerAxis()) {
        return Vector3{
            GetRandomFloat(-rotationSpeed.x, rotationSpeed.x),
            GetRandomFloat(-rotationSpeed.y, rotationSpeed.y),
            GetRandomFloat(-rotationSpeed.z, rotationSpeed.z)
        };
    } else {
        float speed = GetRandomFloat(-rotationSpeed.x, rotationSpeed.x);
        return Vector3{ speed, speed, speed };
    }
}

float ParticleSystem::GenerateRandomLifeTime() {
    Vector2 lifeRange = settings_.GetLifeTimeRange();
    return GetRandomFloat(lifeRange.x, lifeRange.y);
}

float ParticleSystem::GenerateRandomMass() {
    Vector2 massRange = settings_.GetMassRange();
    return GetRandomFloat(massRange.x, massRange.y);
}

Vector4 ParticleSystem::GenerateRandomColor() {
    if (settings_.GetRandomStartColor()) {
        return Vector4{
            GetRandomFloat(0.0f, 1.0f),
            GetRandomFloat(0.0f, 1.0f),
            GetRandomFloat(0.0f, 1.0f),
            settings_.GetStartColor().w
        };
    }
    return settings_.GetStartColor();
}

void ParticleSystem::PrepareInstancingData(Camera* camera) {
    if (!camera) return;

    // カメラ行列計算
    Matrix4x4 view = camera->viewMatrix_;
    Matrix4x4 proj = camera->projectionMatrix_;
    Matrix4x4 vp = Multiply(view, proj);

    // ビルボード行列
    Matrix4x4 billboardMatrix = view;
    billboardMatrix.m[3][0] = 0.0f;
    billboardMatrix.m[3][1] = 0.0f;
    billboardMatrix.m[3][2] = 0.0f;
    billboardMatrix.m[3][3] = 1.0f;
    Matrix4x4 billboardBase = Inverse(billboardMatrix);

    // カリング設定
    Vector3 cameraPos = camera->transform_.translate;
    float cullingDistance = settings_.GetCullingDistance();
    bool cullingEnabled = settings_.GetCullingEnabled();

    // LOD設定
    float lodDistance1 = settings_.GetLODDistance1();
    float lodDistance2 = settings_.GetLODDistance2();
    bool lodEnabled = settings_.GetLODEnabled();

    // インスタンシングデータを準備
    uint32_t instanceCount = 0;
    for (auto& particle : particles_) {
        if (instanceCount >= kMaxInstances_) break;

        // カリング判定
        if (cullingEnabled) {
            float distance = Length(particle.position - cameraPos);
            if (distance > cullingDistance) continue;
        }

        // LOD判定
        if (lodEnabled) {
            float distance = Length(particle.position - cameraPos);
            if (distance > lodDistance2) continue; // 最遠距離でスキップ

            // LOD段階に応じた処理（例：パーティクル数削減）
            if (distance > lodDistance1) {
                // 中距離：一部のパーティクルをスキップ
                if (instanceCount % 2 == 0) continue;
            }
        }

        // 行列計算
        Matrix4x4 S = MakeScaleMatrix(particle.scale);
        //Matrix4x4 R = MakeRotateMatrixXYZ(particle.rotation);
        Matrix4x4 T = MakeTranslateMatrix(particle.position);

        Matrix4x4 world;
        if (settings_.GetEnableBillboard()) {
            // ビルボード
            Matrix4x4 Rz = MakeRotateMatrixZ(particle.rotation.z); // Z軸回転のみ使用
            world = Multiply(S, Multiply(Rz, Multiply(billboardBase, T)));
        } else {
            // 通常回転
            Matrix4x4 R = MakeRotateMatrixXYZ(particle.rotation);
            world = Multiply(S, Multiply(R, T));
        }

        Matrix4x4 wvp = Multiply(world, vp);

        // インスタンシングデータに設定
        instancingData_[instanceCount].WVP = wvp;
        instancingData_[instanceCount].World = world;
        instancingData_[instanceCount].color = particle.color;

        instanceCount++;
    }

    instanceCount_ = instanceCount;

    // GPUに転送
    if (instancingDataForGPU_ && instanceCount > 0) {
        std::memcpy(instancingDataForGPU_, instancingData_.data(), sizeof(ParticleForGPU) * instanceCount);
    }
}

void ParticleSystem::InitializeTrailResources(SrvManager* srvManager)
{
    if (!settings_.GetTrailEnabled()) return;

    auto dxCommon = YoRigine::DirectXCommon::GetInstance();

    // 頂点バッファの作成（動的に変更されるため大きめに確保）
    const size_t maxVertices = kMaxTrailInstances_ * 4; // 1セグメントあたり4頂点
    trailVertexBuffer_ = dxCommon->CreateBufferResource(sizeof(TrailVertex) * maxVertices);

    // インデックスバッファの作成
    const size_t maxIndices = kMaxTrailInstances_ * 6; // 1セグメントあたり6インデックス（2三角形）
    trailIndexBuffer_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * maxIndices);

    // インスタンシング用リソース作成
    trailInstancingResource_ = dxCommon->CreateBufferResource(sizeof(TrailForGPU) * kMaxTrailInstances_);

    // SRVインデックス取得
    trailSrvIndex_ = srvManager->Allocate();

    // GPUメモリにマップ
    trailInstancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&trailInstancingDataForGPU_));

    // SRV作成
    srvManager->CreateSRVforStructuredBuffer(
        trailSrvIndex_,
        trailInstancingResource_.Get(),
        kMaxTrailInstances_,
        sizeof(TrailForGPU)
    );

    // データ配列の初期化
    trailVertices_.reserve(maxVertices);
    trailIndices_.reserve(maxIndices);
    trailInstancingData_.resize(kMaxTrailInstances_);
}

void ParticleSystem::FinalizeTrailResources() {
    if (trailInstancingResource_) {
        trailInstancingResource_->Unmap(0, nullptr);
        trailInstancingResource_.Reset();
    }
    if (trailVertexBuffer_) {
        trailVertexBuffer_.Reset();
    }
    if (trailIndexBuffer_) {
        trailIndexBuffer_.Reset();
    }
    trailInstancingDataForGPU_ = nullptr;
}

// トレイル描画データ準備
void ParticleSystem::PrepareTrailData(Camera* camera) {
    if (!settings_.GetTrailEnabled() || !camera) return;

    trailVertices_.clear();
    trailIndices_.clear();
    trailInstanceCount_ = 0;

    Matrix4x4 view = camera->viewMatrix_;
    Matrix4x4 proj = camera->projectionMatrix_;
    Matrix4x4 vp = Multiply(view, proj);

    uint32_t vertexOffset = 0;

    for (const auto& particle : particles_) {
        if (particle.trailSegments.size() < 2) continue;
        if (trailInstanceCount_ >= kMaxTrailInstances_) break;

        // トレイルセグメントから頂点とインデックスを生成
        for (size_t i = 0; i < particle.trailSegments.size() - 1; ++i) {
            const TrailSegment& current = particle.trailSegments[i];
            const TrailSegment& next = particle.trailSegments[i + 1];

            // セグメント方向を計算
            Vector3 direction = Normalize(next.position - current.position);
            Vector3 cameraDirection = Normalize(camera->transform_.translate - current.position);
            Vector3 right = Normalize(Cross(direction, cameraDirection));

            // 4つの頂点を生成（クワッド）
            float halfWidth1 = current.width * 0.5f;
            float halfWidth2 = next.width * 0.5f;

            TrailVertex vertices[4];

            // 現在セグメントの両端
            vertices[0] = { current.position - right * halfWidth1, {0.0f, 0.0f}, current.color };
            vertices[1] = { current.position + right * halfWidth1, {1.0f, 0.0f}, current.color };

            // 次セグメントの両端
            vertices[2] = { next.position + right * halfWidth2, {1.0f, 1.0f}, next.color };
            vertices[3] = { next.position - right * halfWidth2, {0.0f, 1.0f}, next.color };

            // 頂点を追加
            for (int j = 0; j < 4; ++j) {
                trailVertices_.push_back(vertices[j]);
            }

            // インデックスを追加（2つの三角形）
            trailIndices_.push_back(vertexOffset + 0);
            trailIndices_.push_back(vertexOffset + 1);
            trailIndices_.push_back(vertexOffset + 2);

            trailIndices_.push_back(vertexOffset + 0);
            trailIndices_.push_back(vertexOffset + 2);
            trailIndices_.push_back(vertexOffset + 3);

            vertexOffset += 4;
            trailInstanceCount_++;
        }
    }

    // 頂点バッファにデータをコピー
    if (!trailVertices_.empty()) {
        TrailVertex* vertexData = nullptr;
        trailVertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
        std::memcpy(vertexData, trailVertices_.data(), sizeof(TrailVertex) * trailVertices_.size());
        trailVertexBuffer_->Unmap(0, nullptr);
    }

    // インデックスバッファにデータをコピー
    if (!trailIndices_.empty()) {
        uint32_t* indexData = nullptr;
        trailIndexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&indexData));
        std::memcpy(indexData, trailIndices_.data(), sizeof(uint32_t) * trailIndices_.size());
        trailIndexBuffer_->Unmap(0, nullptr);
    }
}

// ユーティリティ関数の実装
float ParticleSystem::GetRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(randomEngine_);
}

Vector3 ParticleSystem::GetRandomVector3(const Vector3& min, const Vector3& max) {
    return Vector3{
        GetRandomFloat(min.x, max.x),
        GetRandomFloat(min.y, max.y),
        GetRandomFloat(min.z, max.z)
    };
}

Vector3 ParticleSystem::GetRandomDirection() {
    // 単位球面上の均等分布
    float theta = GetRandomFloat(0.0f, 2.0f * std::numbers::pi_v<float>);
    float phi = std::acos(1.0f - 2.0f * GetRandomFloat(0.0f, 1.0f));

    return Vector3{
        std::sin(phi) * std::cos(theta),
        std::cos(phi),
        std::sin(phi) * std::sin(theta)
    };
}

Vector4 ParticleSystem::LerpColor(const Vector4& start, const Vector4& end, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Vector4{
        start.x + (end.x - start.x) * t,
        start.y + (end.y - start.y) * t,
        start.z + (end.z - start.z) * t,
        start.w + (end.w - start.w) * t
    };
}

float ParticleSystem::EaseInOut(float t) {
    return t * t * (3.0f - 2.0f * t);
}

float ParticleSystem::SmoothStep(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

Vector3 ParticleSystem::ApplyRotationAcceleration(const Vector3& velocity, const Vector3& acceleration, float deltaTime) {
    return velocity + acceleration * deltaTime;
}

Vector3 ParticleSystem::ApplyRotationDamping(const Vector3& velocity, float damping, float deltaTime) {
    // 減衰係数を適用（0.0 = 減衰なし、1.0 = 即座に停止）
    float dampingFactor = 1.0f - std::clamp(damping * deltaTime, 0.0f, 1.0f);
    return velocity * dampingFactor;
}

void ParticleSystem::UpdateMaterialInfo()
{
    materialInfo_.color = settings_.GetSystemColor();
    materialInfo_.enableLighting = false;
    UpdateSystemUVTransform();
}

void ParticleSystem::UpdateSystemUVTransform()
{    // UV回転アニメーション
    if (settings_.GetUVRotationEnabled()) {
        uvRotation_ += settings_.GetUVRotationSpeed() * lastDeltaTime_;
        uvRotation_ = std::fmod(uvRotation_, 2.0f * std::numbers::pi_v<float>);
    }

    // UVスケールアニメーション
    if (settings_.GetUVScaleAnimationEnabled()) {
        float scaleTime = systemTime_ * settings_.GetUVScaleAnimationSpeed();
        float scalePulse = std::sin(scaleTime) * settings_.GetUVScaleAnimationAmount() + 1.0f;
        currentUVScale_ = settings_.GetUVBaseScale() * scalePulse;
    } else {
        currentUVScale_ = settings_.GetUVBaseScale();
    }

    // システム全体のUVスクロール
    if (settings_.GetSystemUVScrollEnabled()) {
        Vector2 scrollSpeed = settings_.GetSystemUVScrollSpeed();
        uvOffset_.x += scrollSpeed.x * lastDeltaTime_;
        uvOffset_.y += scrollSpeed.y * lastDeltaTime_;

        // ラップ処理
        uvOffset_.x = std::fmod(uvOffset_.x, 1.0f);
        uvOffset_.y = std::fmod(uvOffset_.y, 1.0f);
    }

    // 最終的なUV変換行列を計算（システム全体のマテリアル用）
    Vector2 finalTranslate = settings_.GetUVBaseTranslate() + uvOffset_;

    Matrix4x4 uvS = MakeScaleMatrix({ currentUVScale_.x, currentUVScale_.y, 1.0f });
    Matrix4x4 uvR = MakeRotateMatrixZ(uvRotation_ + settings_.GetUVBaseRotation());
    Matrix4x4 uvT = MakeTranslateMatrix({ finalTranslate.x, finalTranslate.y, 0.0f });

    materialInfo_.uvTransform = Multiply(uvS, Multiply(uvR, uvT));
}

float ParticleSystem::PerlinNoise(float x, float y, float z, float time) {
    // 簡易パーリンノイズ実装
    // ハッシュ関数
    auto hash = [](float x, float y, float z, float w) -> float {
        // シード値を使った簡易ハッシュ
        float result = std::sin(x * 12.9898f + y * 78.233f + z * 37.719f + w * 17.159f) * 43758.5453f;
        return result - std::floor(result); // 0-1の範囲に正規化
        };

    // 補間関数（スムーズステップ）
    auto smoothstep = [](float t) -> float {
        return t * t * (3.0f - 2.0f * t);
        };

    // 格子点の整数部分
    float ix = std::floor(x);
    float iy = std::floor(y);
    float iz = std::floor(z);

    // 格子内での小数部分
    float fx = x - ix;
    float fy = y - iy;
    float fz = z - iz;

    // スムーズステップを適用
    float u = smoothstep(fx);
    float v = smoothstep(fy);
    float w = smoothstep(fz);

    // 8つの格子点でのノイズ値を計算
    float n000 = static_cast<float>(hash(ix, iy, iz, time));
    float n001 = static_cast<float>(hash(ix, iy, iz + 1, time));
    float n010 = static_cast<float>(hash(ix, iy + 1, iz, time));
    float n011 = static_cast<float>(hash(ix, iy + 1, iz + 1, time));
    float n100 = static_cast<float>(hash(ix + 1, iy, iz, time));
    float n101 = static_cast<float>(hash(ix + 1, iy, iz + 1, time));
    float n110 = static_cast<float>(hash(ix + 1, iy + 1, iz, time));
    float n111 = static_cast<float>(hash(ix + 1, iy + 1, iz + 1, time));

    // トリリニア補間
    float nx00 = n000 * (1 - u) + n100 * u;
    float nx01 = n001 * (1 - u) + n101 * u;
    float nx10 = n010 * (1 - u) + n110 * u;
    float nx11 = n011 * (1 - u) + n111 * u;

    float nxy0 = nx00 * (1 - v) + nx10 * v;
    float nxy1 = nx01 * (1 - v) + nx11 * v;

    float noise = nxy0 * (1 - w) + nxy1 * w;

    // -1から1の範囲に変換
    return (noise * 2.0f) - 1.0f;
}