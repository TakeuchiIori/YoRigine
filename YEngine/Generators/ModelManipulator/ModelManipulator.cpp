#include "ModelManipulator.h"

#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cfloat>

#include "ModelManager.h"
#include <Collision/Core/CollisionTypeIdDef.h>
#include <Collision/OBB/OBBCollider.h>
#include <Collision/Sphere/SphereCollider.h>
#include <Drawer/InstancedObject3d.h>
#include "WorldTransform/WorldTransform.h"
#include "Model.h"
#include "MathFunc.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"
#include <Editor/Editor.h>
#include <Debugger/Gizmo/IGizmable.h>
#include <DirectX/DirectXCommon.h>
#endif
#include <Debugger/Logger.h>
#include <Collision/Core/CollisionManager.h>

namespace YoRigine {

	ModelManipulator* ModelManipulator::instance_ = nullptr;
	ModelManipulator* ModelManipulator::GetInstance() {
		if (!instance_) instance_ = new ModelManipulator;
		return instance_;
	}

	// ============================================================
	// 初期化
	// ============================================================
	void ModelManipulator::Initialize() {
		if (isInitialized_) return; // 重複初期化を防止

		objectManager_ = ObjectManager::GetInstance();
		serializer_.SetObjectManager(objectManager_);
		serializer_.SetModelFolderPath(modelFolderPath_);

		prefabMgr_.SetObjectManager(objectManager_);
		prefabMgr_.SetSerializer(&serializer_);
		prefabMgr_.ScanPrefabFolder();

		selector_.SetObjectManager(objectManager_);
		motionEditor_.Initialize(camera_);
		colliderCubes_.Initialize();
		colliderSpheres_.Initialize();
		colliderLineCapsule_.Initialize();

#ifdef USE_IMGUI
		pickBuffer_ = PickBuffer::GetInstance();
		pickBuffer_->Initialize(); // PickBuffer もここで一度だけ初期化
		selector_.SetPickBuffer(pickBuffer_);

		browser_.SetModelFolderPath(modelFolderPath_);
		browser_.SetPlaceCallback([this](const std::string& path) { PlaceObject(path); });
		browser_.ScanModelFolder();

		editorUI_.SetObjectManager(objectManager_);
		editorUI_.SetSelector(&selector_);
		editorUI_.SetPrefabManager(&prefabMgr_);
		editorUI_.SetSerializer(&serializer_);
		editorUI_.SetGizmoController(&gizmoCtrl_);
		editorUI_.SetPlaceCallback([this](const std::string& path) { PlaceObject(path); });
		editorUI_.SetSaveCallback([this]() { serializer_.SaveScene(jsonPath_); });
		editorUI_.SetLoadCallback([this]() { serializer_.LoadScene(jsonPath_); });
		editorUI_.SetColliderDebugFlag(&showColliderDebug_);
		editorUI_.SetColliderSelectedOnlyFlag(&showColliderSelectedOnly_);
		editorUI_.SetBroadPhaseGridFlag(&showBroadPhaseGrid_);
		editorUI_.SetBroadPhaseGridRadius(&broadPhaseGridDrawRadius_);
		editorUI_.SetDrawFrustumCullingFlag(&enableDrawFrustumCulling_);

		gizmoCtrl_.Initialize();

		// Editor へのメニュー登録もここで一度だけ行う
		Editor::GetInstance()->RegisterMenuBar([this] { editorUI_.DrawMenuBar(); });
#endif

		isInitialized_ = true;
	}

	// =============================================================================
	// Update
	//
	// ■ Pick Pass の順番
	//   1. BeginPickPass()     ← RT クリア・パイプラインセット
	//   2. DrawForPick()       ← 全オブジェクトを PickPSO で描画（IDを焼き込む）
	//   3. EndPickPass()       ← クリックがあった座標を 1px コピー + Signal
	//   4. selector_.Update()  ← クリック検出時は RequestPick、
	//                            前フレームの結果があれば ReadPickResult
	//
	//   PickBuffer の RT にはこのフレームの描画結果が入っている。
	//   クリック座標は RequestPick で登録され、EndPickPass でコピーされ、
	//   次フレームの Update の先頭で ReadPickResult が読み取る。
	// =============================================================================
	void ModelManipulator::Update() {
		if (!isInitialized_) return;

#ifdef USE_IMGUI
		if (!ImGui::GetIO().WantTextInput) {
			if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
				serializer_.SaveScene(jsonPath_);
				std::cout << "[ModelManipulator] Saved (Ctrl+S)\n";
			}
		}

		ShortcutKey();

		motionEditor_.SetTargetObjectId(selector_.GetPrimaryId());
		motionEditor_.Update();
		// ── selector_.Update() だけここで行う（GPU命令は積まない）──
		// シーンエディタが非アクティブ (オブジェクト一覧が閉じてる) ならクリック選択を無効化
		selector_.SetCamera(camera_);
		selector_.Update(
			IsSceneEditorActive(),
			Editor::GetInstance()->GetGameViewPos(),
			Editor::GetInstance()->GetGameViewSize());
#endif

		objectManager_->Update();
	}

	// ============================================================
	// 描画
	// ============================================================
	void ModelManipulator::Draw() {
		if (!isInitialized_ || !camera_) return;

		// Frustum culling: 視錐台を抽出 (有効時のみ)
		Frustum frustum{};
		const bool useCulling = enableDrawFrustumCulling_;
		if (useCulling) {
			frustum = FrustumUtil::ExtractFromViewProjection(
				camera_->GetViewProjectionMatrix());
		}

		auto boundsFor = [&](const auto* obj) -> AABB {
			// コライダー有効時はそのワールドAABB を流用
			if (obj->collider && obj->colliderEnabled) {
				return YoRigine::CollisionManager::ComputeWorldAABB(obj->collider.get());
			}
			// 無いときは position ± (scale * factor) で大雑把に
			float ex = std::fabs(obj->scale.x) * drawBoundsScaleFactor_;
			float ey = std::fabs(obj->scale.y) * drawBoundsScaleFactor_;
			float ez = std::fabs(obj->scale.z) * drawBoundsScaleFactor_;
			return AABB{
				{ obj->position.x - ex, obj->position.y - ey, obj->position.z - ez },
				{ obj->position.x + ex, obj->position.y + ey, obj->position.z + ez }
			};
		};

		// インスタンシング: 非アニメオブジェクトをモデル単位でまとめて描画
		auto* instRenderer = ::InstancedObject3d::GetInstance();
		instRenderer->Begin();
		const Matrix4x4& vp = camera_->GetViewProjectionMatrix();

		for (auto* obj : objectManager_->GetAllActiveObjects()) {
			if (!obj || !obj->object || !obj->worldTransform) continue;

			// Frustum 外ならスキップ
			if (useCulling) {
				AABB bounds = boundsFor(obj);
				if (!FrustumUtil::IsAABBVisible(frustum, bounds)) continue;
			}

			// ボーン表示中の対象はスキップ
			bool isTargetAndBoneDraw = (motionEditor_.IsDrawBone() && obj->id == motionEditor_.GetTargetObjectId());
			if (isTargetAndBoneDraw) continue;

			Model* model = obj->object->GetModel();
			const bool canInstance = (model && !obj->isAnimation && !model->GetHasBones());

			if (canInstance) {
				// インスタンスバッファに積む
				::InstancedObject3d::InstanceData inst{};
				const Matrix4x4& world = obj->worldTransform->GetMatWorld();
				inst.World = world;
				inst.WVP = world * vp;
				// WIT = Transpose(Inverse(World)) を手計算
				{
					Matrix4x4 inv = Inverse(world);
					Matrix4x4& wit = inst.WorldInverseTranspose;
					for (int r = 0; r < 4; ++r)
						for (int c = 0; c < 4; ++c)
							wit.m[r][c] = inv.m[c][r];
				}
				inst.color = obj->color;
				inst.uvTransform = MakeScaleMatrix({ obj->uvScale.x, obj->uvScale.y, 1.0f });
				inst.stochasticStrength = obj->uvStochastic;
				instRenderer->AddInstance(model, inst);

				// PickBuffer や他システムが worldTransform の CB を参照するため、
				// Object3d::Draw を経由しなくても CB を最新行列で同期する
				obj->worldTransform->SetMapWVP(inst.WVP);
				obj->worldTransform->SetMapWorld(world);
			} else {
				// アニメ付き / 特殊エフェクト: 従来の個別 Draw 経路
				obj->object->SetMaterialColor(obj->color);
				obj->object->Draw(camera_, *obj->worldTransform);
			}
		}

		// インスタンス分を1ドローコール/モデルでまとめて描画
		instRenderer->Flush(camera_);

		motionEditor_.Draw();
	}


	// ============================================================
	// 線の描画
	// ============================================================
	void ModelManipulator::DrawLine() {
		if (!isInitialized_ || !camera_) return;
		motionEditor_.DrawBone();

#ifdef USE_IMGUI
		// ── 選択中ハイライト（モデル外接の世界 AABB をオレンジ枠で表示） ──
		// 色のマテリアル上書きを避け、ここでビジュアルな選択フィードバックを与える。
		// コライダーデバッグ表示の ON/OFF に関係なく常時描画。
		if (selector_.HasSelection()) {
			const Vector4 kSelectionColor{ 1.0f, 0.55f, 0.0f, 1.0f }; // Blender 風オレンジ
			colliderCubes_.Begin();
			for (auto* obj : objectManager_->GetAllActiveObjects()) {
				if (!obj || !obj->object || !obj->worldTransform) continue;
				if (!selector_.IsSelected(obj->id)) continue;

				AABB local{};
				if (!objectManager_->ComputeModelLocalAABB(*obj, local)) continue;

				// ローカル AABB の 8 隅をワールド変換して、その AABB を求める
				const Vector3 corners[8] = {
					{ local.min.x, local.min.y, local.min.z },
					{ local.max.x, local.min.y, local.min.z },
					{ local.min.x, local.max.y, local.min.z },
					{ local.max.x, local.max.y, local.min.z },
					{ local.min.x, local.min.y, local.max.z },
					{ local.max.x, local.min.y, local.max.z },
					{ local.min.x, local.max.y, local.max.z },
					{ local.max.x, local.max.y, local.max.z },
				};
				const Matrix4x4& mw = obj->worldTransform->GetMatWorld();
				Vector3 wmn = {  FLT_MAX,  FLT_MAX,  FLT_MAX };
				Vector3 wmx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				for (const auto& c : corners) {
					Vector3 wp = Transform(c, mw);
					wmn.x = std::min(wmn.x, wp.x); wmn.y = std::min(wmn.y, wp.y); wmn.z = std::min(wmn.z, wp.z);
					wmx.x = std::max(wmx.x, wp.x); wmx.y = std::max(wmx.y, wp.y); wmx.z = std::max(wmx.z, wp.z);
				}
				colliderCubes_.AddAABB(wmn, wmx, kSelectionColor);
			}
			colliderCubes_.Flush();
		}

		if (!showColliderDebug_) return;

		// タイプID → 色
		auto colorForType = [](uint32_t typeKey) -> Vector4 {
			switch (static_cast<CollisionTypeIdDef>(typeKey)) {
			case CollisionTypeIdDef::kStaticWall:  return { 1.0f, 0.2f, 0.2f, 1.0f }; // 赤
			case CollisionTypeIdDef::kNavObstacle: return { 1.0f, 0.8f, 0.0f, 1.0f }; // 黄
			case CollisionTypeIdDef::kNavTrigger:  return { 0.2f, 0.5f, 1.0f, 1.0f }; // 青
			case CollisionTypeIdDef::kWaypoint:    return { 0.2f, 1.0f, 0.3f, 1.0f }; // 緑
			default:                               return { 0.6f, 0.6f, 0.6f, 1.0f }; // グレー
			}
		};

		// AABB/OBB は InstancedCube に、Sphere は InstancedSphere に集約
		colliderCubes_.Begin();
		colliderSpheres_.Begin();
		colliderLineCapsule_.SetColor({ 0.6f, 0.6f, 0.6f, 1.0f });

		for (auto* obj : objectManager_->GetAllActiveObjects()) {
			if (!obj || !obj->collider) continue;
			if (!obj->colliderEnabled) continue;
			if (showColliderSelectedOnly_ && !selector_.IsSelected(obj->id)) continue;

			const uint32_t typeKey = obj->collider->GetTypeID();
			const Vector4 col = colorForType(typeKey);

			if (auto* a = dynamic_cast<AABBCollider*>(obj->collider.get())) {
				colliderCubes_.AddAABB(a->GetAABB().min, a->GetAABB().max, col);
			} else if (auto* o = dynamic_cast<OBBCollider*>(obj->collider.get())) {
				colliderCubes_.AddOBB(o->GetOBB().center, o->GetOBB().rotation, o->GetOBB().size, col);
			} else if (auto* s = dynamic_cast<SphereCollider*>(obj->collider.get())) {
				colliderSpheres_.AddSphere(s->GetSphere().center, s->GetSphere().radius, col);
			} else if (auto* cap = dynamic_cast<CapsuleCollider*>(obj->collider.get())) {
				// Capsule のみ Line (低解像度)
				const auto& c = cap->GetCapsule();
				colliderLineCapsule_.DrawCapsule(c.start, c.end, c.radius, 12);
			}
		}

		// 1 DrawInstanced (Cube), 1 DrawInstanced (Sphere), 1 DrawCall (Capsule Line)
		colliderCubes_.Flush();
		colliderSpheres_.Flush();
		colliderLineCapsule_.DrawLine();

		// BroadPhase グリッド可視化 (カメラ周辺のみ) - 別 Flush で2回目の DrawInstanced
		if (showBroadPhaseGrid_ && camera_) {
			colliderCubes_.Begin();
			Vector3 camPos = camera_->GetTranslate();
			YoRigine::CollisionManager::GetInstance()
				->GetBroadPhaseGrid()
				.DrawDebugAroundCamera(&colliderCubes_, camPos, broadPhaseGridDrawRadius_,
					Vector4{ 0.3f, 0.8f, 0.3f, 0.4f });
			colliderCubes_.Flush();
		}
#endif
	}

	// ============================================================
	// ピックパスの描画
	// ============================================================
	void ModelManipulator::DrawPickPass() {
#ifdef USE_IMGUI
		if (!isInitialized_ || !pickBuffer_) return;

		if (!pickBuffer_->IsPickPending()) return; // クリックがないなら描画しない（無駄なGPU負荷を避ける）
		pickBuffer_->BeginPickPass();
		DrawForPick();
		pickBuffer_->EndPickPass();
#endif
	}

	// ============================================================
	// 影の描画
	// ============================================================
	void ModelManipulator::DrawShadow() {
		if (!isInitialized_) return;

		auto* instRenderer = ::InstancedObject3d::GetInstance();
		instRenderer->Begin();
		const bool hasCamera = (camera_ != nullptr);
		const Matrix4x4 vp = hasCamera ? camera_->GetViewProjectionMatrix() : MakeIdentity4x4();

		for (auto* obj : objectManager_->GetAllActiveObjects()) {
			if (!obj || !obj->object || !obj->worldTransform) continue;

			Model* model = obj->object->GetModel();
			const bool canInstance = (model && !obj->isAnimation && !model->GetHasBones());

			if (canInstance) {
				::InstancedObject3d::InstanceData inst{};
				const Matrix4x4& world = obj->worldTransform->GetMatWorld();
				inst.World = world;
				inst.WVP = world * vp;
				// WIT = Transpose(Inverse(World)) を手計算
				{
					Matrix4x4 inv = Inverse(world);
					Matrix4x4& wit = inst.WorldInverseTranspose;
					for (int r = 0; r < 4; ++r)
						for (int c = 0; c < 4; ++c)
							wit.m[r][c] = inv.m[c][r];
				}
				inst.color = obj->color;
				inst.uvTransform = MakeScaleMatrix({ obj->uvScale.x, obj->uvScale.y, 1.0f });
				inst.stochasticStrength = obj->uvStochastic;
				instRenderer->AddInstance(model, inst);
			} else {
				obj->object->DrawShadow(*obj->worldTransform);
			}
		}

		instRenderer->FlushShadow();
	}

	// ============================================================
	// ImGuiの描画
	// ============================================================
	void ModelManipulator::DrawImGui() {
#ifdef USE_IMGUI
		if (!isInitialized_) return;
		browser_.Draw();
		ImGui::Separator();
		if (editorUI_.GetShowObjectListPtr() && *editorUI_.GetShowObjectListPtr())
			editorUI_.DrawObjectList();
		if (editorUI_.GetShowTransformControlsPtr() && *editorUI_.GetShowTransformControlsPtr())
			editorUI_.DrawTransformControls();
		if (*editorUI_.GetShowDuplicateWindowPtr())
			editorUI_.DrawDuplicateWindow();
		if (*editorUI_.GetShowPrefabWindowPtr())
			editorUI_.DrawPrefabWindow();
		motionEditor_.ShowEditor();
#endif
	}

#ifdef USE_IMGUI
	// ============================================================
	// シーンエディタが有効か (オブジェクト一覧ウィンドウ + Editor 全体の表示)
	// 宣言 (ヘッダ) と呼び出し元はすべて USE_IMGUI ガード内に閉じているため、
	// Release ビルド (USE_IMGUI 未定義) ではこの関数は存在しない。
	// ============================================================
	bool ModelManipulator::IsSceneEditorActive() const {
		// 「モデル操作」ウィンドウ (MyGame で RegisterGameUI 登録された名前) が
		// 開かれているときのみ、選択・ギズモを有効化する。
		return Editor::GetInstance()->GetShowEditor()
			&& Editor::GetInstance()->IsGameUIVisible("モデル操作");
	}
#endif

	// ============================================================
	// ギズモの描画
	// ============================================================
	void ModelManipulator::DrawGizmo() {
#ifdef USE_IMGUI
		if (!camera_ || !selector_.HasSelection()) return;
		// オブジェクト一覧ウィンドウが閉じてるときはギズモも非表示
		if (!IsSceneEditorActive()) return;

		gizmables_.clear();
		std::vector<IGizmable*> targets;

		const auto& selectedIds = selector_.GetSelectedIds();
		// reallocによるポインタ無効化を防ぐため、事前に要素数を確保
		gizmables_.reserve(selectedIds.size());

		// 選択されているすべてのオブジェクトをリストに追加
		for (int id : selectedIds) {
			if (motionEditor_.IsDrawBone() && id == motionEditor_.GetTargetObjectId()) {
				continue;
			}
			auto* obj = objectManager_->GetObjectById(id);
			if (obj && obj->worldTransform) {
				gizmables_.emplace_back(obj, objectManager_);
			}
		}

		// pointerをtargetsに登録
		for (auto& g : gizmables_) {
			targets.push_back(&g);
		}

		if (!targets.empty()) {
			gizmoCtrl_.Draw(
				camera_,
				targets,
				Editor::GetInstance()->GetGameViewPos(),
				Editor::GetInstance()->GetGameViewSize());
		}

		motionEditor_.DrawGizmo();
#endif
	}

	// =============================================================================
	// DrawForPick
	//   BeginPickPass / EndPickPass の間に呼ぶ。
	//   DrawShadow と同じ頂点/インデックスバッファを流用し、
	//   PickPSO で ObjectID を R32_UINT RT に書き込む。
	// =============================================================================
	void ModelManipulator::DrawForPick() {
#ifdef USE_IMGUI
		if (!camera_ || !pickBuffer_) return;

		auto* cmd = DirectXCommon::GetInstance()->GetCommandList().Get();

		for (auto* obj : objectManager_->GetAllActiveObjects()) {
			if (!obj || !obj->object || !obj->worldTransform) continue;

			auto* model = obj->object->GetModel();
			if (!model) continue;

			// WVP 行列 CBV (b0)
			cmd->SetGraphicsRootConstantBufferView(
				PickBuffer::ROOT_PARAM_MVP_CBV,
				obj->worldTransform->GetConstBuffer()->GetGPUVirtualAddress());

			// ObjectID ルート定数 (b1)  ※ 0 は空選択予約なので +1
			uint32_t encodedID = static_cast<uint32_t>(obj->id) + 1;
			cmd->SetGraphicsRoot32BitConstant(
				PickBuffer::ROOT_PARAM_OBJECT_ID, encodedID, 0);

			// 頂点/インデックス描画（DrawShadow と同じ方式）
			for (auto& mesh : model->GetMeshes()) {
				if (mesh->HasBones() && model->GetSkinCluster()) {
					mesh->RecordDrawCommands(cmd, *model->GetSkinCluster());
				}
				else {
					mesh->RecordDrawCommands(cmd);
				}
				cmd->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
			}
		}
#endif
	}

	// ============================================================
	// 終了処理
	// ============================================================
	void ModelManipulator::Finalize() {
#ifdef USE_IMGUI
		// 状態リセットのみ（GPU同期は不要）
		if (pickBuffer_) {
			pickBuffer_->Reset();
		}
#endif
		if (objectManager_) objectManager_->Finalize();
		delete instance_;
		instance_ = nullptr;
	}

	// ============================================================
	// シーンの読み込み
	// ============================================================
	void ModelManipulator::LoadScene(const std::string& sceneName) {
		// システムが初期化されていなければ初期化する (安全策)
		if (!isInitialized_) Initialize();
#ifdef USE_IMGUI
		// PickBufferの状態をリセット
		if (pickBuffer_) {
			pickBuffer_->Reset();
		}
#endif
		jsonPath_ = "Resources/Json/Scenes/" + sceneName + ".json";
		// 前のシーンのオブジェクトを安全にクリア
		objectManager_->ClearAllObjects();
		// 新しいシーンデータをロード
		serializer_.LoadScene(jsonPath_);
		// 選択状態をリセット
		selector_.ClearSelection();
		// ログの出力
		Logger("[ModelManipulator] Scene Loaded: " + sceneName);
	}

	// ============================================================
	// ショートカットキーの処理
	// ============================================================
	void ModelManipulator::ShortcutKey() {
#ifdef USE_IMGUI
		ImGuiIO& io = ImGui::GetIO();

		// 「実際にテキスト入力中の時だけ」ブロックしたいので WantTextInput を使う。
		// WantCaptureKeyboard は ImGui ウィンドウにフォーカスがあるだけで true になり、
		// シーンエディタ表示中はほぼ常に true になって誤って全ショートカットを潰してしまう。
		if (io.WantTextInput) return;
		if (!IsSceneEditorActive()) return;

		if (io.KeyCtrl) {
			if (ImGui::IsKeyPressed(ImGuiKey_C)) {
				CopyObject();
			}
			if (ImGui::IsKeyPressed(ImGuiKey_V)) {
				PasteObject();
			}
			// Ctrl+G : 選択中を地面に吸着 (Snap to surface)
			if (ImGui::IsKeyPressed(ImGuiKey_G)) {
				editorUI_.SnapSelectedToSurface();
			}
		}
#endif
	}

	// ============================================================
	// 指定したモデルファイルをシーンに配置
	// ============================================================
	void ModelManipulator::PlaceObject(const std::string& modelPath) {
		try {
			if (modelPath.empty() || !std::filesystem::exists(modelPath)) {
				std::cout << "[ModelManipulator] Invalid path: " << modelPath << "\n";
				return;
			}

			std::filesystem::path full(modelPath);
			std::filesystem::path rel = std::filesystem::relative(full, modelFolderPath_);

			std::string ext = full.extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			bool isAnim = (ext == ".gltf" || ext == ".glb");

			auto* obj = objectManager_->CreateObject(rel.string(), isAnim);
			if (obj) {
				selector_.ClearSelection();
				selector_.AddToSelection(obj->id);
				objectManager_->UpdateObjectTransform(*obj);
				std::cout << "[ModelManipulator] Placed: " << full.filename() << "\n";
			}
		}
		catch (const std::exception& e) {
			std::cout << "[ModelManipulator] PlaceObject error: " << e.what() << "\n";
		}
	}

	// ============================================================
	// 選択したオブジェクトのコピー
	// ============================================================
	void ModelManipulator::CopyObject() {
		auto& selectID = selector_.GetSelectedIds();
		if (selectID.empty()) {
			std::cout << "[ModelManipulator] CopyObject: 選択なし、コピー対象なし\n";
			return;
		}

		copyObjectIDs_.assign(selectID.begin(), selectID.end());
		std::cout << "[ModelManipulator] CopyObject: " << copyObjectIDs_.size() << "件 コピー\n";
	}

	// ============================================================
	// コピーしたオブジェクトを貼り付け
	// ============================================================
	void ModelManipulator::PasteObject() {
		if (copyObjectIDs_.empty()) {
			std::cout << "[ModelManipulator] PasteObject: コピーバッファ空\n";
			return;
		}
		// 貼り付けたものを新しく選択状態にするためにクリアする
		selector_.ClearSelection();

		for (int id : copyObjectIDs_) {
			auto* srcObj = objectManager_->GetObjectById(id);
			if (!srcObj) continue;

			// 生成元のオブジェクト情報を参照してコピーを作成
			auto* newObj = objectManager_->CreateObject(srcObj->modelPath, srcObj->isAnimation, srcObj->animationName);
			if (!newObj) {
				std::cout << "[ModelManipulator] PasteObject: CreateObject失敗 (src ID=" << id << ")\n";
				continue;
			}
			newObj->position = srcObj->position + offsetCopyPos_;
			newObj->rotation = srcObj->rotation;
			newObj->scale = srcObj->scale;

			// コライダー設定をオブジェクト個別にコピー
			newObj->colliderEnabled      = srcObj->colliderEnabled;
			newObj->colliderTypeId       = srcObj->colliderTypeId;
			newObj->colliderShapeType    = srcObj->colliderShapeType;
			newObj->colliderAabbOffset   = srcObj->colliderAabbOffset;
			newObj->colliderObbCenter    = srcObj->colliderObbCenter;
			newObj->colliderObbSize      = srcObj->colliderObbSize;
			newObj->colliderObbEuler     = srcObj->colliderObbEuler;
			newObj->colliderSphereCenter = srcObj->colliderSphereCenter;
			newObj->colliderSphereRadius = srcObj->colliderSphereRadius;
			objectManager_->ApplyColliderTemplate(*newObj);

			// マテリアル色・UV (旧コードは抜けていたので明示的にコピー)
			newObj->color = srcObj->color;
			objectManager_->ApplyObjectColor(*newObj);
			newObj->uvScale = srcObj->uvScale;
			newObj->uvStochastic = srcObj->uvStochastic;
			objectManager_->ApplyObjectUV(*newObj);

			// 親子関係も維持
			newObj->parentID = srcObj->parentID;

			// 選択状態に追加
			selector_.AddToSelection(newObj->id);
			objectManager_->UpdateObjectTransform(*newObj);
		}
		std::cout << "[ModelManipulator] PasteObject: " << copyObjectIDs_.size() << "件 貼り付け\n";
	}
} // namespace YoRigine