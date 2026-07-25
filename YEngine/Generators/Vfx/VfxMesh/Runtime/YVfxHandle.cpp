#include "YVfxHandle.h"
#include "VfxMeshSpawner.h"

YVfxHandle YVfxHandle::Play(const std::string& assetName,
                            const Vector3&     position,
                            float              scale,
                            bool               loop,
                            float              timeScale)
{
    uint32_t id = VfxMeshSpawner::GetInstance()->Spawn(assetName, position, scale, loop, timeScale);
    return YVfxHandle(id);
}

void YVfxHandle::PlayOneShot(const std::string& assetName,
                             const Vector3&     position,
                             float              scale,
                             float              timeScale)
{
    VfxMeshSpawner::GetInstance()->Spawn(assetName, position, scale, false, timeScale);
}

void YVfxHandle::Emit(const std::string& assetName,
                      const Vector3&     position,
                      float              lifetime,
                      float              scale)
{
    // 呼ばれるたびにワンショットで 1 発生成する（撃ちっぱなし）。
    // loop=false / timeScale=1、寿命は lifetime(秒) を明示指定して自動破棄させる。
    VfxMeshSpawner::GetInstance()->Spawn(assetName, position, scale,
                                         /*loop*/ false, /*timeScale*/ 1.0f,
                                         /*lifetime*/ lifetime);
}

YVfxHandle YVfxHandle::PlayBolt(const std::string& assetName,
                                const Vector3&     start,
                                const Vector3&     end,
                                bool               loop,
                                float              timeScale)
{
    uint32_t id = VfxMeshSpawner::GetInstance()->SpawnBolt(assetName, start, end, loop, timeScale);
    return YVfxHandle(id);
}

void YVfxHandle::SetPosition(const Vector3& pos)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetPosition(id_, pos);
}

void YVfxHandle::SetScale(float scale)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetScale(id_, scale);
}

void YVfxHandle::SetTimeScale(float timeScale)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetTimeScale(id_, timeScale);
}

void YVfxHandle::SetEndpoints(const Vector3& start, const Vector3& end)
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->SetEndpoints(id_, start, end);
}

void YVfxHandle::Stop()
{
    if (id_ == 0) return;
    VfxMeshSpawner::GetInstance()->Stop(id_);
    id_ = 0;
}

bool YVfxHandle::IsAlive() const
{
    if (id_ == 0) return false;
    return VfxMeshSpawner::GetInstance()->IsAlive(id_);
}
