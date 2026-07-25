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

    /// <summary>
    /// この生成モジュールが粒に設定しうる「最大生存時間(秒)」のヒント。
    /// エフェクトの再生尺（＝最後の粒が消えるまで）の見積りに使う。
    /// 寿命に関与しないモジュールは 0（既定）を返す。
    /// </summary>
    virtual float GetMaxLifetimeHint() const { return 0.0f; }
};

// --- 更新タイミング専用 ---
class IUpdateModule : public IParticleModule {
public:
    virtual void OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt) = 0;

    /// <summary>
    /// 粒が寿命で死ぬ瞬間に1回だけ呼ばれる（デフォルトは何もしない）。
    /// サブエミッタ（死亡時に子エフェクトを出す等）で使用する。
    /// </summary>
    virtual void OnDeath([[maybe_unused]] ParticleAttribute* attrs,
                         [[maybe_unused]] uint32_t index) {}
};