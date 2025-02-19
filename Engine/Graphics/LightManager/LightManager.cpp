#include "LightManager.h"
// C++
#include <algorithm>

// Engine
#include "DX./DirectXCommon.h"
#include "Object3D/Object3dCommon.h"

// Math
#include "MathFunc.h"

#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG


LightManager* LightManager::GetInstance()
{
    static LightManager instance;
    return &instance;
}

void LightManager::Initialize()
{
  
    this->object3dCommon_ = Object3dCommon::GetInstance();
    // デフォルトカメラのセット
    this->camera_ = object3dCommon_->GetDefaultCamera();
    // リソース作成関数の呼び出し
    CreateDirectionalLightResource();
    CreatePointLightResource();
    CreateSpecularReflectionResource();
    CreateSpotLightResource();
}

void LightManager::SetCommandList()
{
    if (camera_) {
        cameraData_->worldPosition = camera_->GetTranslate();
    }
    // 平行光源CB
    object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());
    // 鏡面反射CB
    object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(4, specularReflectionResource_->GetGPUVirtualAddress());
    // ポイントライト
    object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());
    // スポットライト
    object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
}



void LightManager::CreateDirectionalLightResource()
{
    directionalLightResource_ =object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight_));
    directionalLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLight_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLight_->intensity = 1.0f;
    directionalLight_->enableDirectionalLight = true;
}

void LightManager::CreatePointLightResource()
{
    pointLightResource_ =object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(PointLight));
    pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLight_));
    pointLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLight_->position = { 0.0f, 2.0f, 0.0f };
    pointLight_->intensity = 1.0f;
    pointLight_->radius = 10.0f;
    pointLight_->decay = 1.0f;
    pointLight_->enablePointLight = false;
}

void LightManager::CreateSpecularReflectionResource()
{
    specularReflectionResource_ =object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(CameraForGPU));
    specularReflectionResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
    cameraData_->worldPosition = { 0.0f, 0.0f, 0.0f };
    cameraData_->enableSpecular = false;
    cameraData_->isHalfVector = false;
}

void LightManager::CreateSpotLightResource()
{
    spotLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(SpotLight));
    spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLight_));
    spotLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    spotLight_->position = { 2.0f, 1.25f, 0.0f };
    spotLight_->distance = 7.0f;
    spotLight_->direction = Normalize(Vector3 { -1.0f,-1.0f,0.0f });
    spotLight_->intensity = 4.0f;
    spotLight_->decay = 2.0f;
    spotLight_->cosAngle =
        std::cos(std::numbers::pi_v<float> / 3.0f);
    spotLight_->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 4.0f); // 45度 追加
    spotLight_->enableSpotLight = false;
}

void LightManager::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity, bool enable)
{
    directionalLight_->color = color;
    directionalLight_->direction = direction;
    directionalLight_->intensity = intensity;
    directionalLight_->enableDirectionalLight = enable;
}

void LightManager::SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay, bool enable)
{
    pointLight_->color = color;
    pointLight_->position = position;
    pointLight_->intensity = intensity;
    pointLight_->radius = radius;
    pointLight_->decay = decay;
    pointLight_->enablePointLight = enable;
}

void LightManager::SetSpecularReflection(bool enabled, bool isHalfVector)
{
    cameraData_->enableSpecular = enabled;
    cameraData_->isHalfVector = isHalfVector;
}
void LightManager::ShowLightingEditor()
{
#ifdef _DEBUG
    if (ImGui::Begin("Lighting Editor")) {
        // 平行光源
        ImGui::Text("Directional Light");
        bool directionalLightEnabled = IsDirectionalLightEnabled();
        if (ImGui::Checkbox("Directional Enabled", &directionalLightEnabled)) {
            SetDirectionalLightEnabled(directionalLightEnabled);
        }

        Vector3 lightDirection = GetDirectionalLightDirection();
        if (ImGui::SliderFloat3("Direction", &lightDirection.x, -1.0f, 1.0f, "%.2f")) {
            SetDirectionalLightDirection(lightDirection);
        }

        Vector4 lightColor = GetDirectionalLightColor();
        if (ImGui::ColorEdit4("Color", &lightColor.x)) {
            SetDirectionalLightColor(lightColor);
        }

        float lightIntensity = GetDirectionalLightIntensity();
        if (ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 10.0f, "%.2f")) {
            SetDirectionalLightIntensity(lightIntensity);
        }

        // ポイントライト
        ImGui::Separator(); // 区切り線
        ImGui::Text("Point Light");
        bool pointLightEnabled = IsPointLightEnabled();
        if (ImGui::Checkbox("Enabled", &pointLightEnabled)) {
            SetPointLightEnabled(pointLightEnabled);
        }

        Vector4 pointLightColor = GetPointLightColor();
        if (ImGui::ColorEdit4("Point Color", &pointLightColor.x)) {
            SetPointLightColor(pointLightColor);
        }

        Vector3 pointLightPosition = GetPointLightPosition();
        if (ImGui::SliderFloat3("Position", &pointLightPosition.x, -10.0f, 10.0f, "%.2f")) {
            SetPointLightPosition(pointLightPosition);
        }

        float pointLightIntensity = GetPointLightIntensity();
        if (ImGui::SliderFloat("Point Intensity", &pointLightIntensity, 0.0f, 10.0f, "%.2f")) {
            SetPointLightIntensity(pointLightIntensity);
        }

        float radius = GetPointLightRadius();
        if (ImGui::SliderFloat("Point Radius", &radius, 0.0f, 10.0f, "%.2f")) {
            SetPointLightRadius(radius);
        }

        float decay = GetPointLightDecay();
        if (ImGui::SliderFloat("Point Decay", &decay, 0.0f, 10.0f, "%.2f")) {
            SetPointLightDecay(decay);
        }

        // スポットライト
        ImGui::Separator(); // 区切り線
        ImGui::Text("Spot Light");
        bool spotLightEnabled = IsSpotLightEnabled();
        if (ImGui::Checkbox("Spot Enabled", &spotLightEnabled)) {
            SetSpotLightEnabled(spotLightEnabled);
        }

        Vector4 spotLightColor = GetSpotLightColor();
        if (ImGui::ColorEdit4("Spot Color", &spotLightColor.x)) {
            SetSpotLightColor(spotLightColor);
        }

        Vector3 spotLightPosition = GetSpotLightPosition();
        if (ImGui::SliderFloat3("Spot Position", &spotLightPosition.x, -10.0f, 10.0f, "%.2f")) {
            SetSpotLightPosition(spotLightPosition);
        }

        Vector3 spotLightDirection = GetSpotLightDirection();
        if (ImGui::SliderFloat3("Spot Direction", &spotLightDirection.x, -10.0f, 10.0f, "%.2f")) {
            SetSpotLightDirection(spotLightDirection);
        }

        float spotLightIntensity = GetSpotLightIntensity();
        if (ImGui::SliderFloat("Spot Intensity", &spotLightIntensity, 0.0f, 100.0f, "%.2f")) {
            SetSpotLightIntensity(spotLightIntensity);
        }

        float spotLightDistance = GetSpotLightDistance();
        if (ImGui::SliderFloat("Spot Distance", &spotLightDistance, 0.0f, 200.0f, "%.2f")) {
            SetSpotLightDistance(spotLightDistance);
        }

        float spotLightDecay = GetSpotLightDecay();
        if (ImGui::SliderFloat("Spot Decay", &spotLightDecay, 0.0f, 100.0f, "%.2f")) {
            SetSpotLightDecay(spotLightDecay);
        }

        float spotLightCosAngle = GetSpotLightCosAngle();
        if (ImGui::SliderFloat("Spot Angle", &spotLightCosAngle, 0.0f, 1.0f, "%.2f")) {
            SetSpotLightCosAngle(spotLightCosAngle);
        }

        float spotLightCosFalloffStart = spotLight_->cosFalloffStart;
        if (ImGui::SliderFloat("Spot Falloff Start", &spotLightCosFalloffStart, 0.0f, 1.0f, "%.2f")) {
            spotLight_->cosFalloffStart = spotLightCosFalloffStart;
        }




        // 鏡面反射
        ImGui::Separator(); // 区切り線
        ImGui::Text("Specular Reflection");
        bool specularEnabled = IsSpecularEnabled();
        if (ImGui::Checkbox("Enable Specular", &specularEnabled)) {
            SetSpecularEnabled(specularEnabled);
        }

        bool isHalfVector = IsHalfVectorUsed();
        if (ImGui::Checkbox("Use Half Vector", &isHalfVector)) {
            SetHalfVectorUsed(isHalfVector);
        }
    }
    ImGui::End();
#endif // _DEBUG
}
