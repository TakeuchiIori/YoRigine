#include "EffectHandle.h"

// ============================================================================
// ファクトリメソッド
// ============================================================================

EffectHandle EffectHandle::Play(const std::string& systemName,
                                const Vector3&     position,
                                bool               loop,
                                int                emitCount)
{
    auto* mgr = &YParticleManager::GetInstance();
    YParticleSystem* sys = mgr->GetSystem(systemName);
    if (!sys) return {};

    EffectHandle h;
    h.systemName_ = systemName;
    h.emitter_    = std::make_shared<YParticleEmitter>(systemName, position);
    h.emitter_->SetAutoEmit(loop);
    if (emitCount > 0) h.emitter_->SetEmitCount(emitCount);

    if (loop) {
        // ループ: マネージャに登録して毎フレーム tick させる。
        // これで継続発生・SetPosition() による追従・Stop() による停止が機能する。
        mgr->RegisterEmitter(h.emitter_);
    } else {
        // 非ループ: 1 フレームだけ放出して終了（tick 不要の撃ちっぱなし）
        int n = (emitCount > 0) ? emitCount : h.emitter_->GetEmitCount();
        sys->Emit(position, n);
    }

    return h;
}

EffectHandle EffectHandle::PlayOneShot(const std::string& systemName,
                                       const Vector3&     position,
                                       int                emitCount)
{
    return Play(systemName, position, /*loop=*/false, emitCount);
}

void EffectHandle::Burst(const std::string& systemName,
                         const Vector3&     position,
                         int                count)
{
    auto* mgr = &YParticleManager::GetInstance();
    mgr->EmitBurst(systemName, position, count);
}

void EffectHandle::EmitAll(std::initializer_list<const char*> systemNames,
                           const Vector3& position,
                           int            countEach)
{
    auto* mgr = &YParticleManager::GetInstance();
    for (const char* name : systemNames) {
        mgr->Emit(name, position, countEach);
    }
}

// ============================================================================
// 操作
// ============================================================================

void EffectHandle::SetPosition(const Vector3& pos)
{
    if (emitter_) emitter_->SetPosition(pos);
}

void EffectHandle::Stop()
{
    if (emitter_) {
        emitter_->SetAutoEmit(false);
        emitter_->SetActive(false);
    }
}

// ============================================================================
// クエリ
// ============================================================================

bool EffectHandle::IsActive() const
{
    return emitter_ && emitter_->IsActive();
}
