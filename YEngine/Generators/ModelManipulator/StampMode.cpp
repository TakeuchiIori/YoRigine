#ifdef USE_IMGUI

#include "StampMode.h"

#include "Object3D/ObjectManager.h"
#include "Object3D/Object3dCommon.h"
#include "Collision/Core/CollisionManager.h"
#include "Ray/Raycast.h"
#include "Matrix4x4.h"
#include "Vector4.h"

#include <iostream>

namespace YoRigine {

	// ============================================================
	// モード切替
	// ============================================================
	void StampMode::Enter(int sourceObjectId) {
		if (!objectManager_) return;
		auto* src = objectManager_->GetObjectById(sourceObjectId);
		if (!src) {
			std::cout << "[StampMode] Enter: 不正な sourceId=" << sourceObjectId << "\n";
			return;
		}
		sourceObjectId_ = sourceObjectId;
		active_ = true;
		lastHitValid_ = false;
		BuildGhost(sourceObjectId);
		std::cout << "[StampMode] 開始 (src=" << sourceObjectId << ")\n";
	}

	void StampMode::Exit() {
		if (!active_) return;
		active_ = false;
		ghost_.reset();
		ghostWT_.reset();
		sourceObjectId_ = -1;
		lastHitValid_ = false;
		std::cout << "[StampMode] 終了\n";
	}

	// ============================================================
	// Update (毎フレーム)
	// ============================================================
	void StampMode::Update(const ImVec2& viewportPos, const ImVec2& viewportSize) {
		if (!active_) return;

		// 終了キー (Esc / 右クリック)
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)
			|| ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			Exit();
			return;
		}

		// ソースが消えた場合は終了 (削除/シーン切替対応)
		if (!objectManager_ || !objectManager_->GetObjectById(sourceObjectId_)) {
			Exit();
			return;
		}

		// ImGui のテキスト入力中はクリックを無視
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput) return;

		// ビューポート内かどうか判定
		ImVec2 mouse = ImGui::GetMousePos();
		const bool inView =
			mouse.x >= viewportPos.x && mouse.x <= viewportPos.x + viewportSize.x &&
			mouse.y >= viewportPos.y && mouse.y <= viewportPos.y + viewportSize.y;

		if (!inView) {
			lastHitValid_ = false;
			return;
		}

		// ヒット位置計算
		Vector3 hit{};
		lastHitValid_ = ComputePlacementPoint(viewportPos, viewportSize, hit);
		if (lastHitValid_) {
			lastHitPos_ = hit;
			if (ghostWT_) {
				ghostWT_->translate_ = hit;
				ghostWT_->UpdateMatrix();
			}
		}

		// 左クリックで実体化。
		// WantCaptureMouse はチェックしない: ゲームビュー自体が ImGui ウィンドウ内のため
		// 常に true になり、全クリックが弾かれてしまう (ObjectSelector も同様の理由で外している)。
		// ビューポート範囲チェックは inView で済ませているのでそれで十分。
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && lastHitValid_) {
			PlaceAt(lastHitPos_);
		}
	}

	// ============================================================
	// ゴースト描画 (ModelManipulator::Draw から呼ばれる)
	// ============================================================
	void StampMode::DrawGhost() {
		if (!active_ || !ghost_ || !ghostWT_ || !camera_ || !lastHitValid_) return;

		// 直前のインスタンス描画 (InstancedObject3d::Flush) で別の RS/PSO がバインドされている。
		// Object3d::Draw が参照する Root パラメータ配置に戻すため、ここで明示的に再設定する。
		Object3dCommon::GetInstance()->DrawPreference();

		// 薄い緑のティント色でゴースト感を出す (アルファブレンドが効かない PSO でも
		// 視覚的に明確に区別できる)
		ghost_->SetMaterialColor({ 0.4f, 1.0f, 0.5f, 0.5f });
		ghost_->Draw(camera_, *ghostWT_);
	}

	// ============================================================
	// 内部: スクリーン座標 → ワールド空間レイ
	// ============================================================
	bool StampMode::ScreenToWorldRay(
		float relX, float relY, float viewW, float viewH,
		Vector3& outOrigin, Vector3& outDir) const {
		if (!camera_ || viewW <= 0.0f || viewH <= 0.0f) return false;

		// NDC へ (Y は上向き正)
		const float ndcX =  2.0f * (relX / viewW) - 1.0f;
		const float ndcY =  1.0f - 2.0f * (relY / viewH);

		const Matrix4x4 invVP = Inverse(camera_->GetViewProjectionMatrix());

		// 近平面 (z=0) / 遠平面 (z=1) を逆変換してワールド座標を得る
		Vector4 nearH = Transform(Vector4{ ndcX, ndcY, 0.0f, 1.0f }, invVP);
		Vector4 farH  = Transform(Vector4{ ndcX, ndcY, 1.0f, 1.0f }, invVP);

		if (nearH.w == 0.0f || farH.w == 0.0f) return false;

		Vector3 nearP{ nearH.x / nearH.w, nearH.y / nearH.w, nearH.z / nearH.w };
		Vector3 farP { farH.x  / farH.w,  farH.y  / farH.w,  farH.z  / farH.w };

		Vector3 dir{ farP.x - nearP.x, farP.y - nearP.y, farP.z - nearP.z };
		outDir = Normalize(dir);
		outOrigin = nearP;
		return true;
	}

	// ============================================================
	// 内部: 配置点を計算 (Raycast or Y=0 平面フォールバック)
	// ============================================================
	bool StampMode::ComputePlacementPoint(
		const ImVec2& viewportPos, const ImVec2& viewportSize, Vector3& outPos) const {

		const float relX = ImGui::GetMousePos().x - viewportPos.x;
		const float relY = ImGui::GetMousePos().y - viewportPos.y;

		Vector3 origin{}, dir{};
		if (!ScreenToWorldRay(relX, relY, viewportSize.x, viewportSize.y, origin, dir)) {
			return false;
		}

		// CollisionManager で Raycast
		auto* cm = YoRigine::CollisionManager::GetInstance();
		if (cm) {
			Ray ray{ origin, dir };
			RaycastHit hit;
			// ソース自身は無視 (sourceId を type ID として直接見る方法は無いので
			// 「自身のコライダーを一時無効化」する方が確実)
			BaseCollider* srcCol = nullptr;
			if (auto* src = objectManager_->GetObjectById(sourceObjectId_)) {
				srcCol = src->collider.get();
			}
			const bool wasEnabled = srcCol && srcCol->IsCollisionEnabled();
			if (wasEnabled) srcCol->SetCollisionEnabled(false);

			const bool isHit = cm->Raycast(ray, 10000.0f, &hit, {});

			if (wasEnabled) srcCol->SetCollisionEnabled(true);

			if (isHit) {
				outPos = hit.hitPoint;
				return true;
			}
		}

		// フォールバック: Y=0 平面と交差
		if (dir.y == 0.0f) return false;
		const float t = -origin.y / dir.y;
		if (t <= 0.0f) return false; // カメラの後方
		outPos = { origin.x + dir.x * t, 0.0f, origin.z + dir.z * t };
		return true;
	}

	// ============================================================
	// 内部: ゴーストを構築
	// ============================================================
	void StampMode::BuildGhost(int sourceObjectId) {
		ghost_.reset();
		ghostWT_.reset();

		auto* src = objectManager_->GetObjectById(sourceObjectId);
		if (!src) return;

		// アニメーションはスキップして軽量に
		ghost_ = Object3d::Create(src->modelPath);
		if (!ghost_) return;

		ghostWT_ = std::make_unique<WorldTransform>();
		ghostWT_->Initialize();
		ghostWT_->scale_ = src->scale;
		ghostWT_->rotate_ = src->rotation;
		ghostWT_->translate_ = src->position;
		ghostWT_->UpdateMatrix();
	}

	// ============================================================
	// 内部: スタンプ元の複製を pos に実体化
	// ============================================================
	void StampMode::PlaceAt(const Vector3& pos) {
		if (!objectManager_) return;

		auto* dup = objectManager_->DuplicateObject(sourceObjectId_, { 0,0,0 });
		if (!dup) {
			std::cout << "[StampMode] PlaceAt: DuplicateObject 失敗\n";
			return;
		}
		dup->position = pos;
		objectManager_->UpdateObjectTransform(*dup);
		std::cout << "[StampMode] 配置 (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
	}

} // namespace YoRigine

#endif // USE_IMGUI
