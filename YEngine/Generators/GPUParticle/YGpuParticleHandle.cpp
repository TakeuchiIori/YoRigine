#include "YGpuParticleHandle.h"

#include "YGpuEmitManager.h"
#include <Debugger/Logger.h>

using YoRigine::YGpuEmitManager;

YGpuParticleHandle YGpuParticleHandle::Play(const std::string& groupName, const Vector3& position)
{
    YGpuParticleHandle handle;

    auto* mgr = YGpuEmitManager::GetInstance();
    if (!mgr->HasGroup(groupName)) {
        Logger("YGpuParticleHandle::Play : グループ '" + groupName + "' が見つかりません");
        return handle; // valid_ = false
    }

    mgr->SetGroupPosition(groupName, position);
    mgr->PlayEmitterGroup(groupName);

    handle.groupName_ = groupName;
    handle.valid_ = true;
    return handle;
}

void YGpuParticleHandle::PlayOneShot(const std::string& groupName, const Vector3& position, float countPerEmitter)
{
    auto* mgr = YGpuEmitManager::GetInstance();
    if (!mgr->HasGroup(groupName)) {
        Logger("YGpuParticleHandle::PlayOneShot : グループ '" + groupName + "' が見つかりません");
        return;
    }
    // EmitGroups は count<0 で各エミッタの設定値を使い、linger で寿命ぶんシミュレーションを継続する
    mgr->EmitGroups(groupName, position, countPerEmitter);
}

void YGpuParticleHandle::SetPosition(const Vector3& pos)
{
    if (!valid_) return;
    YGpuEmitManager::GetInstance()->SetGroupPosition(groupName_, pos);
}

void YGpuParticleHandle::Stop()
{
    if (!valid_) return;
    YGpuEmitManager::GetInstance()->StopEmitterGroup(groupName_);
    valid_ = false;
}

bool YGpuParticleHandle::IsActive() const
{
    if (!valid_) return false;
    return YGpuEmitManager::GetInstance()->IsGroupPlaying(groupName_);
}
