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
	// 平行光源リソース作成
	//=====================================================================
	void LightManager::CreateDirectionalLightResource()
	{
		directionalLightResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(sizeof(DirectionalLightData));
		directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLight_));

		directionalLight_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
		directionalLight_->direction = Normalize({ 0.0f, -1.0f, 1.0f });
		directionalLight_->intensity = 1.0f;
		directionalLight_->isEnableDirectionalLighting = true;
	}

	//=====================================================================
	// ポイントライトリソース作成
	//=====================================================================
	void LightManager::CreatePointLightResource() {
		UINT64 size = (sizeof(PointLightArray) + 255) & ~255;
		pointLightsResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(size);
		pointLightsResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLights_));
		ZeroMemory(pointLights_, sizeof(PointLightArray));
		pointLights_->count = 0;
	}

	//=====================================================================
	// スポットライトリソース作成
	//=====================================================================
	void LightManager::CreateSpotLightResource() {
		UINT64 size = (sizeof(SpotLightArray) + 255) & ~255;
		spotLightsResource_ = object3dCommon_->GetDxCommon()->CreateBufferResource(size);
		spotLightsResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLights_));
		ZeroMemory(spotLights_, sizeof(SpotLightArray));
		spotLights_->count = 0;
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

	//===========================================================================
	// 影の計算処理（現状平行光源のみ）
	//===========================================================================
	void LightManager::UpdateShadowMatrix(Camera* camera)
	{
		camera_ = camera;

		// ライト方向は必ず正規化
		Vector3 lightDir = Normalize(directionalLight_->direction);

		// シャドウマップの生成中心（カメラ位置）
		Vector3 target = camera_->transform_.translate;

		// ライトの位置設定
		// カメラ位置からライト方向に大きく離した場所に「仮想的なライト位置」を置く
		float distanceResults = 200.0f;
		Vector3 lightPos = target - lightDir * distanceResults;

		// 垂直方向の対策：ライトが真上/真下に近い場合、Upベクトルを調整
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		if (std::abs(lightDir.y) > 0.99f) {
			up = Vector3(1.0f, 0.0f, 0.0f);
		}

		// ライトビュー行列
		Matrix4x4 lightView = MatrixLookAtLH(lightPos, target, up);

		// -------------------------------------------------------------
		// ピクセルスナップ処理（ちらつき防止）
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

		// Z軸の範囲設定（ShadowmapSettingsの値を使用）
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

	//===========================================================================
	// オブジェクト用
	//===========================================================================
	void LightManager::SetCommandList(UINT directionalIndex, UINT pointIndex, UINT spotIndex) {
		auto cmdList = object3dCommon_->GetDxCommon()->GetCommandList();
		cmdList->SetGraphicsRootConstantBufferView(directionalIndex, directionalLightResource_->GetGPUVirtualAddress());
		cmdList->SetGraphicsRootConstantBufferView(pointIndex, pointLightsResource_->GetGPUVirtualAddress());
		cmdList->SetGraphicsRootConstantBufferView(spotIndex, spotLightsResource_->GetGPUVirtualAddress());
	}

	//===========================================================================
	// 平行光源のみ
	//===========================================================================
	void LightManager::SetCommandListDirectionalOnly(UINT directionalIndex) {
		object3dCommon_->GetDxCommon()->GetCommandList()
			->SetGraphicsRootConstantBufferView(directionalIndex, directionalLightResource_->GetGPUVirtualAddress());
	}

	//===========================================================================
	// ポイントライトの追加
	//===========================================================================
	int LightManager::AddPointLight(const PointLightData& light) {
		assert(pointLights_->count < kMaxPointLights);
		int idx = pointLights_->count++;
		pointLights_->lights[idx] = light;
		return idx;
	}

	//===========================================================================
	// ポイントライトの更新
	//===========================================================================
	void LightManager::UpdatePointLight(int index, const PointLightData& light) {
		assert(index >= 0 && index < pointLights_->count);
		pointLights_->lights[index] = light;
	}
	
	//===========================================================================
	// ポイントライトの削除
	//===========================================================================
	void LightManager::RemovePointLight(int index) {
		assert(index >= 0 && index < pointLights_->count);
		int last = --pointLights_->count;
		if (index != last)
			pointLights_->lights[index] = pointLights_->lights[last];
	}

	//===========================================================================
	// スポットライトの追加
	//===========================================================================
	int LightManager::AddSpotLight(const SpotLightData& light) {
		assert(spotLights_->count < kMaxSpotLights);
		int idx = spotLights_->count++;
		spotLights_->lights[idx] = light;
		return idx;
	}

	//===========================================================================
	// スポットライトの更新
	//===========================================================================
	void LightManager::UpdateSpotLight(int index, const SpotLightData& light) {
		assert(index >= 0 && index < spotLights_->count);
		spotLights_->lights[index] = light;
	}

	//===========================================================================
	// スポットライトの削除
	//===========================================================================
	void LightManager::RemoveSpotLight(int index) {
		assert(index >= 0 && index < spotLights_->count);
		int last = --spotLights_->count;
		if (index != last)
			spotLights_->lights[index] = spotLights_->lights[last];
	}


	//=====================================================================
	// ImGui ライティング編集ウィンドウ
	//=====================================================================
	void LightManager::ShowLightingEditor() {
#ifdef USE_IMGUI

		//------------------------------------------------------------
		// 平行光源
		//------------------------------------------------------------
		if (ImGui::CollapsingHeader("Directional Light")) {
			bool enabled = IsDirectionalLightEnabled();
			if (ImGui::Checkbox("Enabled##Dir", &enabled))
				SetDirectionalLightEnabled(enabled);

			Vector3 dir = GetDirectionalLightDirection();
			if (ImGui::SliderFloat3("Direction", &dir.x, -1.0f, 1.0f, "%.2f"))
				SetDirectionalLightDirection(dir);

			Vector4 color = GetDirectionalLightColor();
			if (ImGui::ColorEdit4("Color##Dir", &color.x))
				SetDirectionalLightColor(color);

			float intensity = GetDirectionalLightIntensity();
			if (ImGui::SliderFloat("Intensity##Dir", &intensity, 0.0f, 10.0f, "%.2f"))
				SetDirectionalLightIntensity(intensity);
		}

		//------------------------------------------------------------
		// ポイントライト
		//------------------------------------------------------------
		if (ImGui::CollapsingHeader("Point Lights")) {
			ImGui::Text("Count: %d / %d", pointLights_->count, kMaxPointLights);

			// 追加ボタン
			if (pointLights_->count < kMaxPointLights) {
				if (ImGui::Button("Add Point Light")) {
					AddPointLight({
						.color = { 1.0f, 1.0f, 1.0f, 1.0f },
						.position = { 0.0f, 2.0f, 0.0f },
						.intensity = 1.0f,
						.isEnablePointLight = true,
						.radius = 10.0f,
						.decay = 1.0f,
						});
				}
			}

			// 各ライトの編集
			for (int i = 0; i < pointLights_->count; ++i) {
				PointLightData& pl = pointLights_->lights[i];

				ImGui::PushID(i);
				// ラベルを折りたたみで表示
				std::string label = "Point Light [" + std::to_string(i) + "]";
				if (ImGui::TreeNode(label.c_str())) {
					bool enabled = pl.isEnablePointLight != 0;
					if (ImGui::Checkbox("Enabled", &enabled))
						pl.isEnablePointLight = enabled;

					ImGui::ColorEdit4("Color", &pl.color.x);
					ImGui::DragFloat3("Position", &pl.position.x, 0.1f);
					ImGui::SliderFloat("Intensity", &pl.intensity, 0.0f, 10.0f, "%.2f");
					ImGui::SliderFloat("Radius", &pl.radius, 0.0f, 100.0f, "%.2f");
					ImGui::SliderFloat("Decay", &pl.decay, 0.0f, 10.0f, "%.2f");

					// 削除ボタン
					if (ImGui::Button("Remove")) {
						RemovePointLight(i);
						ImGui::TreePop();
						ImGui::PopID();
						break; // 削除で配列が変わるのでループを抜ける
					}

					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}

		//------------------------------------------------------------
		// スポットライト
		//------------------------------------------------------------
		if (ImGui::CollapsingHeader("Spot Lights")) {
			ImGui::Text("Count: %d / %d", spotLights_->count, kMaxSpotLights);

			// 追加ボタン
			if (spotLights_->count < kMaxSpotLights) {
				if (ImGui::Button("Add Spot Light")) {
					AddSpotLight({
						.color = { 1.0f, 1.0f, 1.0f, 1.0f },
						.position = { 0.0f, 3.0f, 0.0f },
						.intensity = 4.0f,
						.direction = Normalize(Vector3{ 0.0f, -1.0f, 0.0f }),
						.distance = 7.0f,
						.decay = 2.0f,
						.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f),
						.cosFalloffStart = std::cos(std::numbers::pi_v<float> / 4.0f),
						.isEnableSpotLight = true,
						});
				}
			}

			// 各ライトの編集
			for (int i = 0; i < spotLights_->count; ++i) {
				SpotLightData& sl = spotLights_->lights[i];

				ImGui::PushID(i + kMaxPointLights); // ポイントライトとIDが被らないようにオフセット
				std::string label = "Spot Light [" + std::to_string(i) + "]";
				if (ImGui::TreeNode(label.c_str())) {
					bool enabled = sl.isEnableSpotLight != 0;
					if (ImGui::Checkbox("Enabled", &enabled))
						sl.isEnableSpotLight = enabled;

					ImGui::ColorEdit4("Color", &sl.color.x);
					ImGui::DragFloat3("Position", &sl.position.x, 0.1f);
					ImGui::DragFloat3("Direction", &sl.direction.x, 0.01f, -1.0f, 1.0f);
					ImGui::SliderFloat("Intensity", &sl.intensity, 0.0f, 100.0f, "%.2f");
					ImGui::SliderFloat("Distance", &sl.distance, 0.0f, 200.0f, "%.2f");
					ImGui::SliderFloat("Decay", &sl.decay, 0.0f, 100.0f, "%.2f");
					ImGui::SliderFloat("CosAngle", &sl.cosAngle, 0.0f, 1.0f, "%.2f");
					ImGui::SliderFloat("CosFalloffStart", &sl.cosFalloffStart, 0.0f, 1.0f, "%.2f");

					if (ImGui::Button("Remove")) {
						RemoveSpotLight(i);
						ImGui::TreePop();
						ImGui::PopID();
						break;
					}

					ImGui::TreePop();
				}
				ImGui::PopID();
			}
		}

		//------------------------------------------------------------
		// シャドウマップ
		//------------------------------------------------------------
		if (ImGui::CollapsingHeader("Shadowmap Settings")) {
			ImGui::DragFloat("Shadow Distance", &shadowMapSettings_.shadowDistance, 1.0f, 0.0f, 500.0f);
			ImGui::DragFloat("Ortho Width", &shadowMapSettings_.orthoWidth, 1.0f, 0.0f, 500.0f);
			ImGui::DragFloat("Ortho Height", &shadowMapSettings_.orthoHeight, 1.0f, 0.0f, 500.0f);
			ImGui::DragFloat("Near Z", &shadowMapSettings_.nearZ, 1.0f, 0.0f, 500.0f);
			ImGui::DragFloat("Far Z", &shadowMapSettings_.farZ, 1.0f, 0.0f, 500.0f);
		}

#endif // USE_IMGUI
	}
}