#pragma once
#include <cstdint>
#include <string>
#include "../ParticleMath.h"
#include "ParticleAttribute.h"
#include "Loaders/Json/ISerializable.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif // USE_IMGUI

// 共通の基底
class IParticleModule : public ISerializable {
public:
    virtual ~IParticleModule() = default;
    virtual void Initialize([[maybe_unused]] uint32_t maxParticles) {}
    virtual void DrawEditor() = 0;
    virtual std::string GetName() const = 0;
};

// --- 生成タイミング専用 ---
class ISpawnModule : public IParticleModule {
public:
    virtual void OnSpawn(ParticleAttribute* attrs, uint32_t index) = 0;
};

// --- 更新タイミング専用 ---
class IUpdateModule : public IParticleModule {
public:
    virtual void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) = 0;
};