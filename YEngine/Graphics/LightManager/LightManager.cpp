#include "LightManager.h"
// C++
#include <algorithm>
#include <vector>
#include <limits>

// Engine
#include "DirectXCommon.h"
#include "Object3D/Object3dCommon.h"
#include "DsvManager.h"
#include "Loaders/Json/JsonManager.h"
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
	// デストラクタ（unique_ptr<JsonManager> 用に cpp 側で定義）
	//=====================================================================
	LightManager::~LightManager() = default;

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

		// シャドウマップ設定の JSON 永続化
		// Register が自動 LoadAll するので、ファイルがあれば起動時に値が復元される
		shadowJson_ = std::make_unique<JsonManager>("ShadowMap", "Resources/Json/Lighting/");
		shadowJson_->Register("shadowDistance", &shadowMapSettings_.shadowDistance);
		shadowJson_->Register("orthoWidth", &shadowMapSettings_.orthoWidth);
		shadowJson_->Register("orthoHeight", &shadowMapSettings_.orthoHeight);
		shadowJson_->Register("nearZ", &shadowMapSettings_.nearZ);
		shadowJson_->Register("farZ", &shadowMapSettings_.farZ);
	}

	//=====================================================================
	// GPUへデータ転送
	//=====================================================================
	void LightManager::TransferData()
	{
		// ポイントライトの同期
		pointLights_->count = static_cast<int32_t>(pointLightInstances_.size());
		for (size_t i = 0; i < pointLightInstances_.size(); ++i) {
			pointLights_->lights[i] = pointLightInstances_[i].data;
		}

		// スポットライトの同期
		spotLights_->count = static_cast<int32_t>(spotLightInstances_.size());
		for (size_t i = 0; i < spotLightInstances_.size(); ++i) {
			spotLights_->lights[i] = spotLightInstances_[i].data;
		}
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
	// shadowMapSettings_ の意味:
	//   shadowDistance : 仮想ライト位置をターゲットから何単位後ろに置くか
	//                    （= lightView 空間での target.z）。
	//                    0 なら 200.0f をフォールバックで使用。
	//   nearZ          : ターゲットから「ライト方向（手前）」へどこまで影に入れるか
	//   farZ           : ターゲットから「ライト反対方向（奥）」へどこまで影に入れるか
	//   orthoWidth/Height : XY 方向の覆い範囲
	void LightManager::UpdateShadowMatrix(Camera* camera)
	{
		camera_ = camera;

		// ライト方向は必ず正規化
		Vector3 lightDir = Normalize(directionalLight_->direction);

		// シャドウマップの「中心」を決める。
		// 単純に camera 位置にすると、カメラを上(光源方向)へ振ったときに
		// 画面に映っているのは「カメラ前方の地面」なのに、シャドウフラスタムは
		// カメラ位置中心のまま動かないので前方の地面が外れて影が一気に消える。
		// → カメラ前方を水平面に投影し、その方向へ少し進めた点を中心にする。
		Vector3 cameraPos = camera_->transform_.translate;
		Matrix4x4 camRot = MakeRotateMatrixXYZ(camera_->transform_.rotate);
		Vector3 camForward = TransformNormal(Vector3{ 0.0f, 0.0f, 1.0f }, camRot);

		// 水平成分のみで前方を決める（ピッチでフォーカスが上下に飛ばないように）
		Vector3 forwardHoriz{ camForward.x, 0.0f, camForward.z };
		float horizLen = std::sqrt(forwardHoriz.x * forwardHoriz.x + forwardHoriz.z * forwardHoriz.z);
		if (horizLen > 1.0e-3f) {
			forwardHoriz.x /= horizLen;
			forwardHoriz.z /= horizLen;
		}
		else {
			// カメラ真上/真下向きの縮退ケース。カメラ位置をそのまま使う
			forwardHoriz = Vector3{ 0.0f, 0.0f, 0.0f };
		}

		// フォーカス距離（典型的な三人称カメラと target の距離 ~35 を想定）
		constexpr float kFocusDistance = 30.0f;
		Vector3 target = cameraPos + forwardHoriz * kFocusDistance;

		// 仮想ライトをターゲットからどれだけ後ろに置くか
		// JSON で 0 の場合はフォールバック値を使う（旧挙動互換 + 安全側）
		float lightBackDistance = shadowMapSettings_.shadowDistance > 0.1f
			? shadowMapSettings_.shadowDistance
			: 200.0f;

		// 仮想ライト位置
		Vector3 lightPos = target - lightDir * lightBackDistance;

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
		const float shadowMapSize = static_cast<float>(DsvManager::kShadowmapHeight);
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

		// Z 軸の範囲設定:
		// target は lightView 空間で z = lightBackDistance に来る。
		// そこから手前(nearZ)/奥(farZ) のオフセットで前後を広げる。
		float minZ = lightBackDistance - shadowMapSettings_.nearZ;
		float maxZ = lightBackDistance + shadowMapSettings_.farZ;

		// 安全策: 近接面はクリップ空間 z >= 0 を満たすため微小正値で下限を切る
		if (minZ < 0.1f) minZ = 0.1f;
		// 退化したフラスタムを防ぐ
		if (maxZ < minZ + 0.1f) maxZ = minZ + 0.1f;

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
	// ポイントライトの追加（名前で追加）
	//===========================================================================
	void LightManager::AddPointLight(const std::string& name, const PointLightData& data)
	{
		if (pointLightIndexMap_.contains(name)) {
			pointLightInstances_[pointLightIndexMap_[name]].data = data;
			return;
		}
		if (pointLightInstances_.size() >= kMaxSpotLights) return;

		pointLightIndexMap_[name] = pointLightInstances_.size();
		pointLightInstances_.push_back({ name, data });
	}

	//===========================================================================
	// ポイントライトの取得
	//===========================================================================
	LightManager::PointLightData* LightManager::GetPointLight(const std::string& name)
	{
		if (pointLightIndexMap_.contains(name)) {
			return &pointLightInstances_[pointLightIndexMap_[name]].data;
		}
		return nullptr;
	}

	//===========================================================================
	// ポイントライトの削除（名前を指定して削除）
	//===========================================================================
	void LightManager::RemovePointLight(const std::string& name)
	{
		if (!pointLightIndexMap_.contains(name)) return;

		size_t targetIdx = pointLightIndexMap_[name];
		size_t lastIdx = pointLightInstances_.size() - 1;

		// 削除対象が末尾でないなら、末尾要素を削除場所に移動
		if (targetIdx != lastIdx) {
			pointLightInstances_[targetIdx] = std::move(pointLightInstances_.back());
			// 移動した要素のインデックス情報をマップで更新
			pointLightIndexMap_[pointLightInstances_[targetIdx].name] = targetIdx;
		}

		pointLightInstances_.pop_back();
		pointLightIndexMap_.erase(name);
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

	//===========================================================================
	// スポットライトの追加（名前で追加）
	//===========================================================================
	void LightManager::AddSpotLight(const std::string& name, const SpotLightData& data)
	{
		if (spotLightIndexMap_.contains(name)) {
			spotLightInstances_[spotLightIndexMap_[name]].data = data;
			return;
		}
		if (spotLightInstances_.size() >= kMaxSpotLights) return;

		spotLightIndexMap_[name] = spotLightInstances_.size();
		spotLightInstances_.push_back({ name, data });
	}

	//===========================================================================
	// スポットライトの取得（名前で取得）
	//===========================================================================
	LightManager::SpotLightData* LightManager::GetSpotLight(const std::string& name)
	{
		if (spotLightIndexMap_.contains(name)) {
			return &spotLightInstances_[spotLightIndexMap_[name]].data;
		}
		return nullptr;
	}

	//===========================================================================
	// スポットライトの削除（名前で削除）
	//===========================================================================
	void LightManager::RemoveSpotLight(const std::string& name)
	{
		if (!spotLightIndexMap_.contains(name)) return;

		size_t targetIdx = spotLightIndexMap_[name];
		size_t lastIdx = spotLightInstances_.size() - 1;

		// 削除対象が末尾でないなら、末尾要素を削除場所に移動
		if (targetIdx != lastIdx) {
			spotLightInstances_[targetIdx] = std::move(spotLightInstances_.back());
			// 移動した要素のインデックス情報をマップで更新
			spotLightIndexMap_[spotLightInstances_[targetIdx].name] = targetIdx;
		}

		spotLightInstances_.pop_back();
		spotLightIndexMap_.erase(name);
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
			ImGui::Text("Count: %zu / %d", pointLightInstances_.size(), kMaxPointLights);

			// 追加ボタン
			if (pointLightInstances_.size() < kMaxPointLights) {
				if (ImGui::Button("Add Point Light")) {
					static int plCounter = 0;
					std::string newName = "PointLight_" + std::to_string(plCounter++);
					AddPointLight(newName, {
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
			for (size_t i = 0; i < pointLightInstances_.size(); ++i) {
				auto& instance = pointLightInstances_[i];
				PointLightData& pl = instance.data;

				ImGui::PushID(instance.name.c_str()); // 名前をIDにして衝突回避
				std::string label = instance.name;
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
						RemovePointLight(instance.name);
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
			ImGui::Text("Count: %zu / %d", spotLightInstances_.size(), kMaxSpotLights);

			// 追加ボタン
			if (spotLightInstances_.size() < kMaxSpotLights) {
				if (ImGui::Button("Add Spot Light")) {
					static int slCounter = 0;
					std::string newName = "SpotLight_" + std::to_string(slCounter++);
					AddSpotLight(newName, {
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
			for (size_t i = 0; i < spotLightInstances_.size(); ++i) {
				auto& instance = spotLightInstances_[i];
				SpotLightData& sl = instance.data;

				ImGui::PushID(instance.name.c_str());
				std::string label = instance.name;
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
					///*** 角度を degree で編集（cosAngle=内側, cosFalloffStart=外側）***///
					constexpr float kRad2Deg = 180.0f / std::numbers::pi_v<float>;
					constexpr float kDeg2Rad = std::numbers::pi_v<float> / 180.0f;

					float innerDeg = std::acos(std::clamp(sl.cosAngle, -1.0f, 1.0f)) * kRad2Deg;
					float outerDeg = std::acos(std::clamp(sl.cosFalloffStart, -1.0f, 1.0f)) * kRad2Deg;

					// cos → degree（内側・外側）
					// 内側角度スライダー（外角より必ず小さく）
					if (ImGui::SliderFloat("Inner Angle [deg]", &innerDeg, 0.0f, 89.0f, "%.1f deg")) {
						innerDeg = std::min(innerDeg, outerDeg - 1.0f);
						sl.cosAngle = std::cos(innerDeg * kDeg2Rad);
					}

					// 外側角度スライダー（内角より必ず大きく）
					if (ImGui::SliderFloat("Outer Angle [deg]", &outerDeg, 1.0f, 90.0f, "%.1f deg")) {
						outerDeg = std::max(outerDeg, innerDeg + 1.0f);
						sl.cosFalloffStart = std::cos(outerDeg * kDeg2Rad);
					}

					ImGui::TextDisabled("cosAngle(inner)=%.3f  cosFalloffStart(outer)=%.3f",
						sl.cosAngle, sl.cosFalloffStart);

					if (ImGui::Button("Remove")) {
						RemoveSpotLight(instance.name);
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
			ImGui::DragFloat("Light Back Distance (shadowDistance)", &shadowMapSettings_.shadowDistance, 1.0f, 0.0f, 1000.0f);
			ImGui::TextDisabled("  ターゲットから仮想ライト位置までの距離。0 なら 200 を使用");

			ImGui::DragFloat("Ortho Width", &shadowMapSettings_.orthoWidth, 1.0f, 0.0f, 1000.0f);
			ImGui::DragFloat("Ortho Height", &shadowMapSettings_.orthoHeight, 1.0f, 0.0f, 1000.0f);
			ImGui::TextDisabled("  影が覆う XY 範囲（光から見た幅・高さ）");

			ImGui::DragFloat("Near Range (nearZ)", &shadowMapSettings_.nearZ, 1.0f, 0.0f, 1000.0f);
			ImGui::TextDisabled("  ターゲットからライト方向（手前）に何単位ぶん影に入れるか");

			ImGui::DragFloat("Far Range (farZ)", &shadowMapSettings_.farZ, 1.0f, 0.0f, 1000.0f);
			ImGui::TextDisabled("  ターゲットからライト反対方向（奥）に何単位ぶん影に入れるか");

			ImGui::Separator();
			if (ImGui::Button("Save Shadowmap Settings") && shadowJson_) {
				shadowJson_->Save();
			}
		}

#endif // USE_IMGUI
	}
}