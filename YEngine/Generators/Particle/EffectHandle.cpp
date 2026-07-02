#include "EffectHandle.h"
#include "Debugger/Logger.h"
#include "Composite/CompositeEffectManager.h"   // 複合エフェクト解決（入口統一の先頭）

// ============================================================================
// ファクトリメソッド
// ============================================================================

EffectHandle EffectHandle::Play(const std::string& systemName,
                                const Vector3&     position,
                                bool               loop,
                                int                emitCount)
{
    auto* mgr      = &YParticleManager::GetInstance();
    auto& groupMgr = YEmitterGroupManager::GetInstance();

    // ── 入口統一(最優先): 複合エフェクト（Particle+VfxMesh+GPU+Sound）として解決 ──
    if (auto* comp = CompositeEffectManager::GetInstance(); comp->Has(systemName)) {
        if (loop) {
            return comp->Play(systemName, position);
        }
        comp->PlayOneShot(systemName, position);
        return {}; // ワンショットは撃ちっぱなし＝保持不要
    }

    // ── 入口統一: まず Group（束ねた完成エフェクト）として解決 ──────────────
    if (YEmitterGroup* group = groupMgr.GetGroup(systemName)) {
        // 同名の System も存在する場合は構成ミスの可能性が高いので警告
        if (mgr->GetSystem(systemName)) {
            Logger("EffectHandle: Group と System が同名 -> Group を優先: " + systemName);
        }

        EffectHandle h;
        h.systemName_ = systemName;
        h.group_      = group;

        group->SetPosition(position);
        group->SetActive(true);
        if (loop) {
            // ループ: 自動発生ON。GroupManager::Update（YParticleManager::Update
            // から毎フレーム呼ばれる）で tick され、SetPosition 追従・Stop が効く。
            group->SetAutoEmitAll(true);
        } else {
            // ワンショット: 1回だけ全エミッタ発火（emitCount<=0 はエミッタ設定に従う）
            group->SetAutoEmitAll(false);
            group->EmitAll(emitCount > 0 ? emitCount : -1);
        }
        return h;
    }

    // ── System 単体として再生 ──────────────────────────────────────────────
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
    if (composite_)    composite_->SetPosition(pos);
    else if (group_)   group_->SetPosition(pos);
    else if (emitter_) emitter_->SetPosition(pos);
}

void EffectHandle::Stop()
{
    if (composite_) {
        composite_->Stop();
    } else if (group_) {
        group_->SetAutoEmitAll(false);
        group_->SetActive(false);
    } else if (emitter_) {
        emitter_->SetAutoEmit(false);
        emitter_->SetActive(false);
    }
}

// ============================================================================
// クエリ
// ============================================================================

bool EffectHandle::IsActive() const
{
    if (composite_) return composite_->IsActive();
    if (group_) return group_->IsActive();
    return emitter_ && emitter_->IsActive();
}
