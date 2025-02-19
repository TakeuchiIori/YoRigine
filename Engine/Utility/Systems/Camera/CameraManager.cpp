#include "CameraManager.h"

// C++
#include <algorithm>

std::shared_ptr<Camera> CameraManager::AddCamera()
{
    auto camera = std::make_shared<Camera>();
    cameras_.emplace_back(camera);
    return camera;
}

void CameraManager::RemoveCamera(std::shared_ptr<Camera> camera)
{
    cameras_.erase(std::remove(cameras_.begin(), cameras_.end(), camera), cameras_.end());
}

void CameraManager::SetCurrentCamera(std::shared_ptr<Camera> camera)
{
    currentCamera_ = camera;
}

std::shared_ptr<Camera> CameraManager::GetCurrentCamera() const
{
    return currentCamera_;
}

void CameraManager::UpdateAllCameras()
{
    for (auto& camera : cameras_)
    {
        camera->Update();
    }
}
