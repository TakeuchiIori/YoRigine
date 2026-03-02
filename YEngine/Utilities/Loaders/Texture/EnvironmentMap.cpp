#include "EnvironmentMap.h"
#include "Loaders/Texture/TextureManager.h"

EnvironmentMap* EnvironmentMap::GetInstance()
{
    static EnvironmentMap instance;
    return &instance;
}



void EnvironmentMap::LoadEnvironmentTexture(const std::string& filePath)
{
    filePath_ = filePath;
    TextureManager::GetInstance()->LoadTexture(filePath_);
    srvIndex_ = TextureManager::GetInstance()->GetTextureIndexByFilePath(filePath_);
}

D3D12_GPU_DESCRIPTOR_HANDLE EnvironmentMap::GetSrvHandle() const
{
    return TextureManager::GetInstance()->GetsrvHandleGPU(filePath_);
}
