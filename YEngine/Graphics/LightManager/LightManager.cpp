#include "LightManager.h"
// C++
#include <algorithm>
#include <vector>
#include <limits>

// Engine
#include "DirectXCommon.h"
#include "Object3D/Object3dCommon.h"
#include "DsvManager.h"
// Math
#include "MathFunc.h"


#ifdef USE_IMGUI
#include "imgui.h"
#endif // _DEBUG

namespace YoRigine {
	//=====================================================================
	// シングルトン取得
	//=====================================================================>
	LightManager* LightManager::GetInstance()
	{
		static LightManager instance;
		return &instance;
	}

	//=====================================================================
	// 初期化
	//=====================================================================
	void LightManager::Initialize()
	{
		// Object3d 共通処理取得
		this->object3dCommon_ = Object3dCommon::GetInstance();

		// デフォルトカメラを取得（Specular 用）
		this->camera_ = object3dCommon_->GetDefaultCamera();

		// 各ライト種類ごとの定数バッファを GPU 上に作成
		CreateDirectionalLightResource();
		CreatePointLightResource();
		CreateSpotLightResource();
		CreateShadowResource();
	}

	//=====================================================================
	// コマンドリストへ CBV 設定
	//=====================================================================
	void LightManager::SetCommandList(UINT directionalIndex, UINT pointIndex, UINT spotIndex)
	{
		object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(directionalIndex, directionalLightResource_->GetGPUVirtualAddress());
		object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(pointIndex, pointLightResource_->GetGPUVirtualAddress());
		object3dCommon_->GetDxCommon()->GetCommandList()->SetGraphicsRootConstantBufferView(spotIndex, spotLightResource_->GetGPUVirtualAddress());
	}

	//=====================================================================
	// 平行光源リソース作成
	//=====================================================================
	void LightManager::CreateDirectionalLightResource()
	{
		directionalLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLight));
		directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight_));

		directionalLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		directionalLight_->direction = Normalize({ 0.0f, -1.0f, 1.0f });
		directionalLight_->intensity = 1.0f;
		directionalLight_->enableDirectionalLight = true;
	}

	//=====================================================================
	// ポイントライトリソース作成
	//=====================================================================
	void LightManager::CreatePointLightResource()
	{
		pointLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(PointLight));
		pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLight_));

		pointLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		pointLight_->position = { 0.0f, 2.0f, 0.0f };
		pointLight_->intensity = 1.0f;
		pointLight_->radius = 10.0f;
		pointLight_->decay = 1.0f;
		pointLight_->enablePointLight = false;
	}


	//=====================================================================
	// スポットライトリソース作成
	//=====================================================================
	void LightManager::CreateSpotLightResource()
	{
		spotLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(SpotLight));
		spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLight_));

		spotLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		spotLight_->position = { 2.0f, 1.25f, 0.0f };
		spotLight_->distance = 7.0f;
		spotLight_->direction = Normalize(Vector3{ -1.0f,-1.0f,0.0f });
		spotLight_->intensity = 4.0f;
		spotLight_->decay = 2.0f;

		// cosAngle, cosFalloffStart は角度ではなく“コサイン値”に注意
		spotLight_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);      // 60°
		spotLight_->cosFalloffStart = std::cos(std::numbers::pi_v<float> / 4.0f); // 45°

		spotLight_->enableSpotLight = false;
	}

	/*==========================================================================
	影のリソース作成
	//========================================================================*/
	void LightManager::CreateShadowResource()
	{
		shadowResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(ShadowMatrix));
		shadowResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadow_));

		// 初期値は単位行列
		shadow_->lightViewProjection = MakeIdentity4x4();
	}


	/*==========================================================================
	影の計算処理（現状平行光源のみ）
	//========================================================================*/
	void LightManager::UpdateShadowMatrix(Camera* camera)
	{
		camera_ = camera;

		// ライト方向は必ず正規化
		Vector3 lightDir = Normalize(directionalLight_->direction);

		// シャドウマップの生成中心（カメラ位置）
		Vector3 target = camera_->transform_.translate;

		// ★ライトの位置設定
		// カメラ位置からライト方向に大きく離した場所に「仮想的なライト位置」を置く
		float distanceResults = 200.0f;
		Vector3 lightPos = target - lightDir * distanceResults;

		// ★垂直方向の対策：ライトが真上/真下に近い場合、Upベクトルを調整
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		if (std::abs(lightDir.y) > 0.99f) {
			up = Vector3(1.0f, 0.0f, 0.0f);
		}

		// ライトビュー行列
		Matrix4x4 lightView = MatrixLookAtLH(lightPos, target, up);

		// -------------------------------------------------------------
		// ★ ピクセルスナップ処理（ちらつき防止）
		// ワールド原点 (0,0,0) を基準にしてテクセルグリッドを固定します
		// -------------------------------------------------------------

		// 範囲設定
		float width = shadowMapSettings_.orthoWidth;
		float height = shadowMapSettings_.orthoHeight;
		float halfWidth = width * 0.5f;
		float halfHeight = height * 0.5f;

		// テクセルサイズ
		const float shadowMapSize = DsvManager::kShadowmapHeight;
		float unitX = width / shadowMapSize;
		float unitY = height / shadowMapSize;

		// ライトビュー空間での「ワールド原点」と「ターゲット位置」を取得
		Vector3 worldOriginInLight = Transform(Vector3(0.0f, 0.0f, 0.0f), lightView);
		Vector3 targetInLight = Transform(target, lightView);

		// カメラ（ターゲット）を中心にしたいが、グリッドはずらしたくない
		// 理想的なビューポートの左端・上端
		float idealMinX = targetInLight.x - halfWidth;
		float idealMinY = targetInLight.y - halfHeight;

		// ワールド原点からの距離を測り、ユニット単位で切り捨てる（スナップ）
		float distFromOriginX = idealMinX - worldOriginInLight.x;
		float distFromOriginY = idealMinY - worldOriginInLight.y;

		float snappedDistX = std::floor(distFromOriginX / unitX) * unitX;
		float snappedDistY = std::floor(distFromOriginY / unitY) * unitY;

		// 最終的な境界を決定（ワールド原点 + スナップ済みの距離）
		float minX = worldOriginInLight.x + snappedDistX;
		float maxX = minX + width;
		float minY = worldOriginInLight.y + snappedDistY;
		float maxY = minY + height;

		// ★ Z軸の範囲設定（ShadowmapSettingsの値を使用）
		// 修正：以前の `targetInLight.z - nearZ` (=199) ではライト寄りの空間がクリップされるため、
		// minZ はライト原点基準の `nearZ` をそのまま使用して、遮蔽物（カメラとライトの間）をカバーします。
		// maxZ はカメラ位置よりさらに奥（farZ分）まで含めます。
		float minZ = shadowMapSettings_.nearZ;
		float maxZ = targetInLight.z + shadowMapSettings_.farZ;

		// ライトの正射影行列を作成
		Matrix4x4 lightProj = MakeOrthographicMatrix(
			minX, maxY, maxX, minY,
			minZ, maxZ
		);

		// 最終的なライトビュー射影行列
		shadow_->lightViewProjection = lightView * lightProj;
	}
	/*==========================================================================
	平行光源のセット
	//========================================================================*/
	void LightManager::SetDirectionalLight(const Vector4& color, const Vector3& direction, float intensity, bool enable)
	{
		directionalLight_->color = color;
		directionalLight_->direction = direction;
		directionalLight_->intensity = intensity;
		directionalLight_->enableDirectionalLight = enable;
	}
	/*==========================================================================
	ポイントライトのセット
	//========================================================================*/
	void LightManager::SetPointLight(const Vector4& color, const Vector3& position, float intensity, float radius, float decay, bool enable)
	{
		pointLight_->color = color;
		pointLight_->position = position;
		pointLight_->intensity = intensity;
		pointLight_->radius = radius;
		pointLight_->decay = decay;
		pointLight_->enablePointLight = enable;
	}

	//=====================================================================
	// ImGui ライティング編集ウィンドウ
	//=====================================================================
	void LightManager::ShowLightingEditor()
	{
#ifdef USE_IMGUI

		//------------------------------------------------------------
		// 平行光源
		//------------------------------------------------------------
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

		//------------------------------------------------------------
		// ポイントライト
		//------------------------------------------------------------
		ImGui::Separator();
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
		if (ImGui::SliderFloat("Point Radius", &radius, 0.0f, 1000.0f, "%.2f")) {
			SetPointLightRadius(radius);
		}

		float decay = GetPointLightDecay();
		if (ImGui::SliderFloat("Point Decay", &decay, 0.0f, 10.0f, "%.2f")) {
			SetPointLightDecay(decay);
		}

		//------------------------------------------------------------
		// スポットライト
		//------------------------------------------------------------
		ImGui::Separator();
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

		Vector3 spotLightDirection = Normalize(GetSpotLightDirection());
		if (ImGui::SliderFloat3("Spot Direction", &spotLightDirection.x, 0.0f, 1.0f, "%.2f")) {
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

		//------------------------------------------------------------
		// シャドウマップ	
		//------------------------------------------------------------
		ImGui::Separator();
		ImGui::Text("Shadowmap Settings");
		float shadowDistance = shadowMapSettings_.shadowDistance;
		if (ImGui::DragFloat("Shadow Distance", &shadowDistance, 1.0f, 500.0f)) {
			shadowMapSettings_.shadowDistance = shadowDistance;
		}
		float orthoWidth = shadowMapSettings_.orthoWidth;
		if (ImGui::DragFloat("Shadow orthoWidth", &orthoWidth, 1.0f, 500.0f)) {
			shadowMapSettings_.orthoWidth = orthoWidth;
		}
		float orthoHeight = shadowMapSettings_.orthoHeight;
		if (ImGui::DragFloat("Shadow orthoHeight", &orthoHeight, 1.0f, 500.0f)) {
			shadowMapSettings_.orthoHeight = orthoHeight;
		}
		float nearZ = shadowMapSettings_.nearZ;
		if (ImGui::DragFloat("Shadow nearZ", &nearZ, 1.0f, 500.0f)) {
			shadowMapSettings_.nearZ = nearZ;
		}
		float farZ = shadowMapSettings_.farZ;
		if (ImGui::DragFloat("Shadow farZ", &farZ, 1.0f, 500.0f)) {
			shadowMapSettings_.farZ = farZ;
		}

#endif // _DEBUG
	}
}