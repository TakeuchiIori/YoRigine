#include "VfxMeshHandle.h"
#include "VfxMeshSpawner.h"

VfxMeshHandle VfxMeshHandle::Play(const std::string& assetName,
                                  const Vector3&     position,
                                  float              scale,
                                  bool               loop,
                                  float              timeScale)
{
    uint32_t id = VfxMeshSpawner::GetInstance()->Spawn(assetName, position, scale, loop, timeScale);
    return VfxMeshHandle(id);
}

void VfxMeshHandle::PlayOneShot(const std::string& assetName,
                                const Vector3&     position,
                                float              scale,
                                float              timeScale)
{
    VfxMeshSpawner::GetInstance()->Spawn(assetName, position, scale, false, timeScale);
}

VfxMeshHandle VfxMeshHandle::PlayBolt(const std::string& assetName,
                                      const Vector3&     start,
                                      const Vector3&     end,
                                      bool               loop,
                                      float              timeScale)
{
    uint32_t id = VfxMeshSpawner::GetInstance()->SpawnBolt(assetName, start, end, loop, timeScale);
    return VfxMeshHandle(id);
}

void VfxMeshHandle::SetPosition(const Vector3& pos)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetPosition(id_, pos);
}

void VfxMeshHandle::SetScale(float scale)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetScale(id_, scale);
}

void VfxMeshHandle::SetTimeScale(float timeScale)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetTimeScale(id_, timeScale);
}

void VfxMeshHandle::SetEndpoints(const Vector3& start, const Vector3& end)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetEndpoints(id_, start, end);
}

void VfxMeshHandle::Stop()
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->Stop(id_);
    id_ = 0;
}

bool VfxMeshHandle::IsAlive() const
{
    if (id_ == 0) return false;
    return VfxMeshSpawner::GetInstance()->IsAlive(id_);
}
