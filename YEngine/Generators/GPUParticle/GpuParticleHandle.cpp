#include "GpuParticleHandle.h"

#include "GpuEmitManager.h"
#include <Debugger/Logger.h>

using YoRigine::GpuEmitManager;

GpuParticleHandle GpuParticleHandle::Play(const std::string& groupName, const Vector3& position)
{
    GpuParticleHandle handle;

    auto* mgr = GpuEmitManager::GetInstance();
    if (!mgr->HasGroup(groupName)) {
        Logger("GpuParticleHandle::Play : グループ '" + groupName + "' が見つかりません");
        return handle; // valid_ = false
    }

    mgr->SetGroupPosition(groupName, position);
    mgr->PlayEmitterGroup(groupName);

    handle.groupName_ = groupName;
    handle.valid_ = true;
    return handle;
}

void GpuParticleHandle::PlayOneShot(const std::string& groupName, const Vector3& position, float countPerEmitter)
{
    auto* mgr = GpuEmitManager::GetInstance();
    if (!mgr->HasGroup(groupName)) {
        Logger("GpuParticleHandle::PlayOneShot : グループ '" + groupName + "' が見つかりません");
        return;
    }
    // EmitGroups は count<0 で各エミッタの設定値を使い、linger で寿命ぶんシミュレーションを継続する
    mgr->EmitGroups(groupName, position, countPerEmitter);
}

void GpuParticleHandle::SetPosition(const Vector3& pos)
{
    if (!valid_) return;
    GpuEmitManager::GetInstance()->SetGroupPosition(groupName_, pos);
}

void GpuParticleHandle::Stop()
{
    if (!valid_) return;
    GpuEmitManager::GetInstance()->StopEmitterGroup(groupName_);
    valid_ = false;
}

bool GpuParticleHandle::IsActive() const
{
    if (!valid_) return false;
    return GpuEmitManager::GetInstance()->IsGroupPlaying(groupName_);
}
