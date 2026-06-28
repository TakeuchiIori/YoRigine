#include "PipCameraSystem.h"

// Engine
#include "DirectXCommon.h"
#include "Vector4.h"
#include "SceneSystems/SceneManager.h"
#include "SceneSystems/BaseScene.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

PipCameraSystem* PipCameraSystem::GetInstance() {
	static PipCameraSystem instance;
	return &instance;
}

void PipCameraSystem::Initialize() {
	// カメラ実体生成 + 基本パラメータ
	pipCamera_ = std::make_unique<Camera>();
	pipCamera_->SetTranslate({ 0.0f, 10.0f, -20.0f });
	pipCamera_->SetRotate({ 0.4f, 0.0f, 0.0f });
	pipCamera_->SetFovY(0.45f);
	pipCamera_->SetAspectRatio(static_cast<float>(rtWidth_) / static_cast<float>(rtHeight_));
	pipCamera_->SetNearClip(0.1f);
	pipCamera_->SetFarClip(1000.0f);
	pipCamera_->UpdateMatrix();

	// RTV / DSV を確保
	CreateRenderResources();
	InitJson();
}

void PipCameraSystem::CreateRenderResources() {
	if (resourcesCreated_) return;

	auto* dxCommon = YoRigine::DirectXCommon::GetInstance();
	if (!dxCommon) return;

	auto* rtvMgr = dxCommon->GetRTVManager();
	auto* dsvMgr = dxCommon->GetDSVManager();
	if (!rtvMgr || !dsvMgr) return;

	// PiP 用 RTV (SRV つき → ImGui::Image で表示可能)
	rtvMgr->Create(
		rtName_,
		rtWidth_,
		rtHeight_,
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		Vector4{ 0.05f, 0.05f, 0.10f, 1.0f }, // 視認しやすい暗い青のクリア色
		true
	);

	// PiP 用 DSV (Phase 1b の 2nd レンダーパスで使う)
	dsvMgr->Create(
		dsvName_,
		rtWidth_,
		rtHeight_,
		DXGI_FORMAT_D24_UNORM_S8_UINT,
		false,
		1.0f,
		0
	);

	resourcesCreated_ = true;
}

void PipCameraSystem::InitJson()
{
	jsonManager_ = std::make_unique<YoRigine::JsonManager>("PipCamera", "Resources/Json/Cameras/");
	jsonManager_->SetCategory("PipCamera");
	jsonManager_->Register("Enabled", &enabled_);
	jsonManager_->Register("Position", &pipCamera_->transform_.translate);
	jsonManager_->Register("Rotation", &pipCamera_->transform_.rotate);
	jsonManager_->Register("FovY", &pipCamera_->fovY_);
}

void PipCameraSystem::Update() {
	if (!pipCamera_) return;
	// アスペクト比は解像度変更があった場合に追従させたいので毎フレーム反映
	pipCamera_->SetAspectRatio(static_cast<float>(rtWidth_) / static_cast<float>(rtHeight_));
	pipCamera_->UpdateMatrix();
}

void PipCameraSystem::Finalize() {
	pipCamera_.reset();
	resourcesCreated_ = false;
}

void PipCameraSystem::ApplyToCamera(Camera* target) {
	if (!target || !pipCamera_) return;

	// 元の値を退避
	saved_.transform = target->transform_;
	saved_.fovY      = target->fovY_;
	saved_.aspect    = target->aspectRatio_;
	saved_.nearClip  = target->nearClip_;
	saved_.farClip   = target->farClip_;
	saved_.valid     = true;

	// PiP カメラの値を流し込む
	target->transform_   = pipCamera_->transform_;
	target->fovY_        = pipCamera_->fovY_;
	target->aspectRatio_ = pipCamera_->aspectRatio_;
	target->nearClip_    = pipCamera_->nearClip_;
	target->farClip_     = pipCamera_->farClip_;

	// 行列再計算 + GPU バッファに反映
	target->UpdateMatrix();
	target->Update();
}

void PipCameraSystem::SnapToSceneCamera(const Camera* src) {
	if (!src || !pipCamera_) return;
	pipCamera_->transform_   = src->transform_;
	pipCamera_->fovY_        = src->fovY_;
	pipCamera_->nearClip_    = src->nearClip_;
	pipCamera_->farClip_     = src->farClip_;
	// aspectRatio_ は PiP RT のアスペクトのままにする (PiP 解像度に合わせる)
	pipCamera_->UpdateMatrix();
}

void PipCameraSystem::RestoreCamera(Camera* target) {
	if (!target || !saved_.valid) return;

	target->transform_   = saved_.transform;
	target->fovY_        = saved_.fovY;
	target->aspectRatio_ = saved_.aspect;
	target->nearClip_    = saved_.nearClip;
	target->farClip_     = saved_.farClip;

	target->UpdateMatrix();
	target->Update();

	saved_.valid = false;
}

void PipCameraSystem::DrawImGuiWindow() {
#ifdef USE_IMGUI
	if (!pipCamera_) return;

	bool dirty = false;

	if (ImGui::Checkbox("PiP 有効", &enabled_)) {
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("シーンカメラに揃える")) {
		auto* scene = SceneManager::GetInstance()->GetScene();
		if (scene) {
			if (const Camera* sc = scene->GetSceneCamera()) {
				SnapToSceneCamera(sc);
				dirty = true;
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("保存")) {
		dirty = true;
	}
	ImGui::Separator();

	ImGui::Text("カメラ設定");
	Vector3 pos = pipCamera_->GetTranslate();
	if (ImGui::DragFloat3("位置", &pos.x, 0.1f)) {
		pipCamera_->SetTranslate(pos);
		dirty = true;
	}

	Vector3 rot = pipCamera_->transform_.rotate;
	if (ImGui::DragFloat3("回転 (rad)", &rot.x, 0.01f)) {
		pipCamera_->SetRotate(rot);
		dirty = true;
	}

	float fov = pipCamera_->fovY_;
	if (ImGui::SliderFloat("FOV (rad)", &fov, 0.1f, 1.5f)) {
		pipCamera_->SetFovY(fov);
		dirty = true;
	}

	// PipCameraSystem はシーン跨ぎのシングルトンで JsonManager の
	// シーン終了時自動 Save 経路に乗らないため、編集があった瞬間に
	// 明示的に Save しておく (値はそれぞれ Register で参照渡しなので即反映)。
	if (dirty && jsonManager_) {
		jsonManager_->Save();
	}

	ImGui::Separator();
	ImGui::Text("解像度: %u x %u", rtWidth_, rtHeight_);

	// プレビュー画面 (ImGui::Image)
	auto* dxCommon = YoRigine::DirectXCommon::GetInstance();
	if (dxCommon && resourcesCreated_) {
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float aspect = static_cast<float>(rtWidth_) / static_cast<float>(rtHeight_);
		float availAsp = (avail.y > 0.1f) ? (avail.x / avail.y) : 1.0f;
		ImVec2 imageSize;
		if (availAsp > aspect) {
			imageSize.y = avail.y;
			imageSize.x = imageSize.y * aspect;
		} else {
			imageSize.x = avail.x;
			imageSize.y = imageSize.x / aspect;
		}
		ImTextureID texId = (ImTextureID)dxCommon->GetRTVSrvGPU(rtName_).ptr;
		ImGui::Image(texId, imageSize);
		if (!enabled_) {
			ImGui::TextDisabled("(PiP 無効 - チェックボックス ON でクリア色のみ表示)");
		}
	}
#endif
}
