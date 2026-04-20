#include "MotionEditor.h"

// Engine
#include "Systems/Camera/Camera.h"
#include "Model.h"
#include "../Core/MotionSystem.h"
#include "Skeleton/Joint.h"
#include "Skeleton/Skeleton.h"
#include "Skeleton/BoneGizmable.h"
#include <Editor/Editor.h>
#include "Object3D/ObjectManager.h" 

// Panels
#include "Panels/MenuBarPanel.h"
#include "Panels/SavePanel.h"
#include "Panels/ToolbarPanel.h"
#include "Panels/BoneListPanel.h"
#include "Panels/PropertyPanel.h"
#include "Panels/TimelinePanel.h"
#include "Panels/StatusBarPanel.h"

#ifdef USE_IMGUI
#include "ImGuizmo.h"
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

// ============================================================
// Context Helper Implementation
// ============================================================
Object3d* MotionEditorContext::GetTargetObject() const {
	if (targetObjectId != -1) {
		return ObjectManager::GetInstance()->GetObject3dById(targetObjectId);
	}
	return nullptr;
}

MotionEditor::MotionEditor() {}
MotionEditor::~MotionEditor() {}

// ============================================================
// 初期化
// ============================================================
void MotionEditor::Initialize(Camera* camera)
{
	context_.camera = camera;
	previewTransform_.Initialize();

	lineDrawer_ = std::make_unique<Line>();
	lineDrawer_->SetCamera(context_.camera);
	lineDrawer_->Initialize();
	lineDrawer_->SetColor({ 0.5f, 0.5f, 0.5f, 1.0f });

#ifdef USE_IMGUI
	boneObj_ = Object3d::Create("ICO.obj");
	gizmoCtrl_.Initialize();
	gizmoCtrl_.SetMode(GizmoController::Mode::Local);
#endif

	// ============================================================
	// コンテキストのコールバック設定 (UIとロジックの分離)
	// ============================================================
	context_.SyncJointToBuffer = [this]() { SyncJointToBuffer(context_.selBone); };
	context_.SyncBufferToJoint = [this]() { SyncBufferToJoint(); };

	// KF追加の実処理
	context_.AddKeyframe = [this](const std::string& bone, float time) {
		if (!context_.currentMotion) return;
		auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
		if (!nodeAnims.count(bone)) return;

		Vector3 newT = { context_.editT[0], context_.editT[1], context_.editT[2] };
		Quaternion newR = EulerToQuaternion({ context_.editR[0] * (kPi / 180), context_.editR[1] * (kPi / 180), context_.editR[2] * (kPi / 180) });
		Vector3 newS = { context_.editS[0], context_.editS[1], context_.editS[2] };
		Motion::NodeAnimation snap = nodeAnims[bone];

		context_.history.Execute(MakeLambdaCommand("KF挿入: " + bone,
			[this, bone, time, newT, newR, newS]() {
				auto& na = context_.currentMotion->animation_.nodeAnimations_[bone];
				auto insertOrReplace = [&]<typename T>(auto& kfs, T val) {
					using KF = typename std::remove_reference<decltype(kfs)>::type::value_type;
					auto it = std::find_if(kfs.begin(), kfs.end(),
						[time](const KF& k) { return std::abs(k.time - time) < 1e-4f; });
					if (it != kfs.end()) { it->value = val; }
					else {
						KF k; k.time = time; k.value = val; kfs.push_back(k);
						std::sort(kfs.begin(), kfs.end(), [](const KF& a, const KF& b) { return a.time < b.time; });
					}
				};
				insertOrReplace(na.translate.keyframes, newT);
				insertOrReplace(na.rotate.keyframes, newR);
				insertOrReplace(na.scale.keyframes, newS);
				context_.requireTimelineRebuild = true;
			},
			[this, bone, snap]() {
				context_.currentMotion->animation_.nodeAnimations_[bone] = snap;
				context_.requireTimelineRebuild = true;
			}
		));
		};

	// KF削除の実処理
	context_.DeleteKeyframe = [this](const std::string& bone, float time) {
		if (!context_.currentMotion) return;
		auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
		if (!nodeAnims.count(bone)) return;

		Motion::NodeAnimation snap = nodeAnims[bone];
		context_.history.Execute(MakeLambdaCommand("KF削除: " + bone,
			[this, bone, time]() {
				auto& na = context_.currentMotion->animation_.nodeAnimations_[bone];
				auto rem = [time](auto& kfs) {
					kfs.erase(std::remove_if(kfs.begin(), kfs.end(),
						[time](const auto& k) { return std::abs(k.time - time) < 1e-4f; }), kfs.end());
					};
				rem(na.translate.keyframes);
				rem(na.rotate.keyframes);
				rem(na.scale.keyframes);
				context_.requireTimelineRebuild = true;
			},
			[this, bone, snap]() {
				context_.currentMotion->animation_.nodeAnimations_[bone] = snap;
				context_.requireTimelineRebuild = true;
			}
		));
		};

	// ============================================================
	// パネルの登録
	// ============================================================
	RegisterPanel(std::make_unique<MenuBarPanel>());
	RegisterPanel(std::make_unique<SavePanel>());
	RegisterPanel(std::make_unique<ToolbarPanel>());
	RegisterPanel(std::make_unique<BoneListPanel>());
	RegisterPanel(std::make_unique<PropertyPanel>());
	RegisterPanel(std::make_unique<TimelinePanel>());
	RegisterPanel(std::make_unique<StatusBarPanel>());
}

void MotionEditor::RegisterPanel(std::unique_ptr<IMotionEditorPanel> panel)
{
	panel->Initialize(&context_);
	panels_.push_back(std::move(panel));
}

// ============================================================
// 更新
// ============================================================
void MotionEditor::Update()
{
	Object3d* target = context_.GetTargetObject();
	if (!target) return;

	Model* m = target->GetModel();
	MotionSystem* ms = m ? m->GetMotionSystem() : nullptr;

	if (ms) {
		float msTime = ms->GetAnimationTime();
		if (std::abs(msTime - context_.scrubTime) > 1e-4f) {
			context_.scrubTime = msTime;
			// UIとの同期は TimelinePanel が担当するためここでは時間更新のみ
		}
	}

	if (m && m->GetSkeleton()) {
		if (ms && !context_.isPlaying && context_.currentMotion) {
			context_.currentMotion->ApplyAnimation(m->GetSkeleton()->GetJoints(), context_.scrubTime);

			if (!context_.selBone.empty()) {
#ifdef USE_IMGUI
				if (draggingBone_ || gizmoCtrl_.IsUsing()) {
					SyncBufferToJoint();
				}
				else {
					SyncJointToBuffer(context_.selBone);
				}
#else
				SyncJointToBuffer(context_.selBone);
#endif
			}
		}

		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) {
			m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
		}
	}

	previewTransform_.UpdateMatrix();

	// 各パネルのロジック更新
	for (auto& panel : panels_) {
		if (panel->IsActive()) panel->Update();
	}
}

// ============================================================
// エディタUIの描画
// ============================================================
void MotionEditor::ShowEditor()
{
#ifdef USE_IMGUI
	ImGui::SetNextWindowSize(ImVec2(1200, 750), ImGuiCond_FirstUseEver);
	ImGui::Begin("Motion Editor", nullptr, ImGuiWindowFlags_MenuBar);

	Object3d* target = context_.GetTargetObject();

	// ---------------- ボーン選択＆ギズモ処理 ----------------
	if (context_.isDrawBone && target && target->GetModel() && target->GetModel()->GetSkeleton()) {
		if (ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
			gizmoCtrl_.ClearPickables();
			boneGizmables_.clear();

			auto& joints = target->GetModel()->GetSkeleton()->GetJoints();
			for (const auto& joint : joints) {
				auto bg = std::make_unique<BoneGizmable>(this, joint.GetName());
				bg->UpdateFromJoint();
				gizmoCtrl_.RegisterPickable(bg.get());
				boneGizmables_.push_back(std::move(bg));
			}

			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();

			IGizmable* picked = gizmoCtrl_.TryPickObject(ImGui::GetMousePos(), viewPos, viewSize, context_.camera);
			if (picked) {
				BoneGizmable* bg = static_cast<BoneGizmable*>(picked);
				context_.selBone = bg->GetBoneName();
				SyncJointToBuffer(context_.selBone);
				context_.statusMsg = "ボーン選択: " + context_.selBone;
			}
		}

		if (!context_.selBone.empty()) {
			static BoneGizmable currentSelectedGizmo(this, "");
			if (currentSelectedGizmo.GetBoneName() != context_.selBone) {
				currentSelectedGizmo = BoneGizmable(this, context_.selBone);
			}
			currentSelectedGizmo.UpdateFromJoint();

			std::vector<IGizmable*> gizmoTargets = { &currentSelectedGizmo };
			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
			gizmoCtrl_.Draw(context_.camera, gizmoTargets, viewPos, viewSize);
		}
	}

	// ---------------- キーボードショートカット ----------------
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		context_.history.HandleKeyInput();

		if (ImGui::IsKeyPressed(ImGuiKey_Space) && target) {
			if (context_.isPlaying) {
				if (target->GetModel() && target->GetModel()->GetMotionSystem()) target->GetModel()->GetMotionSystem()->Stop();
				context_.isPlaying = false;
				context_.statusMsg = "[ Space ] 一時停止";
			}
			else {
				if (target->GetModel() && target->GetModel()->GetMotionSystem()) {
					auto* ms = target->GetModel()->GetMotionSystem();
					float savedTime = ms->GetAnimationTime();
					if (savedTime >= ms->GetDuration() || ms->IsFinished()) savedTime = 0.0f;
					if (context_.isLoop) target->PlayLoop(); else target->PlayOnce();
					ms->SetAnimationTime(savedTime);
				}
				context_.isPlaying = true;
				context_.statusMsg = "[ Space ] 再生";
			}
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) && context_.selKF.IsValid() && context_.currentMotion) {
			if (context_.DeleteKeyframe) {
				auto& na = context_.currentMotion->animation_.nodeAnimations_[context_.selKF.boneName];
				float t = 0;
				if (context_.selKF.index < (int)na.translate.keyframes.size())
					t = na.translate.keyframes[context_.selKF.index].time;
				context_.DeleteKeyframe(context_.selKF.boneName, t);
			}
			context_.selKF.Clear();
			context_.statusMsg = "[ Del ] キーフレーム削除";
		}
	}

	if (!target || !target->GetModel()) {
		// MenuBarとSaveLoad(ポップアップ)だけは描画しておく
		panels_[0]->DrawImGui(); // MenuBarPanel
		panels_[1]->DrawImGui(); // SaveLoadPanel

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40);
		float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((w - 220) * 0.5f);
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "シーンエディタでモデルを選択してください");
		ImGui::End();
		return;
	}

	// ---------------- パネル群の描画 ----------------
	// 0: MenuBar, 1: SaveLoad, 2: Toolbar, 3: BoneList, 4: Property, 5: Timeline, 6: StatusBar
	panels_[0]->DrawImGui(); // MenuBar
	panels_[1]->DrawImGui(); // SaveLoad
	panels_[2]->DrawImGui(); // Toolbar
	ImGui::Separator();

	// レイアウト：上部は左右分割（BoneList と Property）
	float contentW = ImGui::GetContentRegionAvail().x;
	float upperH = ImGui::GetContentRegionAvail().y - context_.timelineH - 30.0f; // タイムラインとステータスバーの分を引く

	ImGui::BeginChild("##upper", ImVec2(contentW, upperH), false);
	{
		// 左側: BoneList
		ImGui::BeginChild("##bones", ImVec2(context_.bonePanelW, 0), true);
		panels_[3]->DrawImGui();
		ImGui::EndChild();

		ImGui::SameLine();

		// リサイズ用セパレータ
		ImGuiIO& io = ImGui::GetIO();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		ImGui::InvisibleButton("##vsep", ImVec2(4, upperH));
		if (ImGui::IsItemActive()) {
			context_.bonePanelW = std::clamp(context_.bonePanelW + io.MouseDelta.x, 80.0f, contentW - 100.0f);
		}
		if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
			ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(100, 130, 200, 180));
		}
		ImGui::PopStyleVar();

		ImGui::SameLine();

		// 右側: Property
		ImGui::BeginChild("##props", ImVec2(0, 0), true);
		panels_[4]->DrawImGui();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	// リサイズ用水平セパレータ
	ImGuiIO& io = ImGui::GetIO();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::InvisibleButton("##hsep", ImVec2(contentW, 4.0f));
	if (ImGui::IsItemActive()) {
		context_.timelineH -= io.MouseDelta.y;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(100, 130, 200, 180));
	}
	ImGui::PopStyleVar();

	// 下部: Timeline
	panels_[5]->DrawImGui();

	// 最下部: StatusBar
	panels_[6]->DrawImGui();

	ImGui::End();
#endif
}

// ============================================================
// 3D 描画・機能実装群
// ============================================================
void MotionEditor::Draw()
{
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target) {
#ifdef USE_IMGUI
		if (boneObj_ && target->GetModel() && target->GetModel()->GetSkeleton()) {
			auto& joints = target->GetModel()->GetSkeleton()->GetJoints();
			if (boneWorldTransforms_.size() != joints.size()) {
				boneWorldTransforms_.resize(joints.size());
				for (auto& wt : boneWorldTransforms_) wt.Initialize();
			}

			for (size_t i = 0; i < joints.size(); ++i) {
				auto& wt = boneWorldTransforms_[i];
				Matrix4x4 targetMat = GetTargetWorldMatrix();
				Matrix4x4 wMat = joints[i].GetWorldTransform().matWorld_ * targetMat;

				float t[3], rDeg[3], s[3];
				ImGuizmo::DecomposeMatrixToComponents(&wMat.m[0][0], t, rDeg, s);

				wt.translate_ = { t[0], t[1], t[2] };
				wt.rotate_ = { rDeg[0] * (kPi / 180.f), rDeg[1] * (kPi / 180.f), rDeg[2] * (kPi / 180.f) };
				wt.scale_ = { 0.1f, 0.1f, 0.1f };
				wt.UpdateMatrix();

				if (context_.selBone == joints[i].GetName()) boneObj_->SetMaterialColor({ 1.0f, 0.85f, 0.2f, 1.0f });
				else boneObj_->SetMaterialColor({ 0.5f, 0.5f, 0.5f, 1.0f });

				boneObj_->Draw(context_.camera, wt);
			}
		}
#endif
	}
}

void MotionEditor::DrawGizmo()
{
#ifdef USE_IMGUI
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target && target->GetModel() && target->GetModel()->GetSkeleton()) {
		if (!context_.selBone.empty()) {
			static BoneGizmable currentSelectedGizmo(this, "");
			if (currentSelectedGizmo.GetBoneName() != context_.selBone) {
				currentSelectedGizmo = BoneGizmable(this, context_.selBone);
			}
			if (!gizmoCtrl_.IsUsing()) currentSelectedGizmo.UpdateFromJoint();

			std::vector<IGizmable*> gizmoTargets = { &currentSelectedGizmo };
			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
			gizmoCtrl_.Draw(context_.camera, gizmoTargets, viewPos, viewSize);
		}
	}
#endif
}

void MotionEditor::DrawBone()
{
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target) {
		lineDrawer_->SetCamera(context_.camera);
		target->DrawBone(*lineDrawer_.get(), GetTargetWorldMatrix());
	}
}

void MotionEditor::SetTargetObjectId(int id) {
	Object3d* obj = nullptr;
	if (id != -1) {
		obj = ObjectManager::GetInstance()->GetObject3dById(id);
		if (obj && obj->GetModel()) {
			if (obj->GetModel()->GetMotionSystem() == nullptr) id = -1;
		}
		else id = -1;
	}

	if (context_.targetObjectId != id) {
		context_.targetObjectId = id;

		if (context_.targetObjectId != -1) {
			Object3d* target = context_.GetTargetObject();
			Model* model = target->GetModel();
			MotionSystem* ms = model->GetMotionSystem();

			context_.loadFileName = model->GetName();
			context_.currentMotion = ms->GetAnimation();
			context_.selectedAnimKey = "";

			if (context_.currentMotion) {
				for (auto& [key, motion] : Model::animationCache_) {
					if (&motion == context_.currentMotion) {
						context_.selectedAnimKey = key;
						break;
					}
				}
				context_.requireTimelineRebuild = true;
			}

			context_.scrubTime = ms->GetAnimationTime();
			context_.isPlaying = true;
			context_.statusMsg = "対象オブジェクトを同期: " + context_.loadFileName;
		}
		else {
			context_.currentMotion = nullptr;
			context_.selectedAnimKey = "";
			context_.selBone = "";
			context_.selKF.Clear();
			context_.loadFileName = "";
			context_.statusMsg = "Ready";
		}
	}
}

void MotionEditor::InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr)
{
	if (!context_.currentMotion) return;
	auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
	if (!nodeAnims.count(bone)) return;

	auto insertOrReplace = [&]<typename T>(auto& kfs, T val) {
		using KF = typename std::remove_reference<decltype(kfs)>::type::value_type;
		auto it = std::find_if(kfs.begin(), kfs.end(), [time](const KF& k) { return std::abs(k.time - time) < 1e-4f; });
		if (it != kfs.end()) { it->value = val; }
		else {
			KF k; k.time = time; k.value = val; kfs.push_back(k);
			std::sort(kfs.begin(), kfs.end(), [](const KF& a, const KF& b) { return a.time < b.time; });
		}
	};

	insertOrReplace(nodeAnims[bone].translate.keyframes, tr.translate);
	insertOrReplace(nodeAnims[bone].rotate.keyframes, tr.rotate);
	insertOrReplace(nodeAnims[bone].scale.keyframes, tr.scale);
}

void MotionEditor::SavePose(float time)
{
	Object3d* target = context_.GetTargetObject();
	if (!target || !target->GetModel() || !target->GetModel()->GetSkeleton() || !context_.currentMotion) return;

	Skeleton* skeleton = target->GetModel()->GetSkeleton();
	auto oldAnims = context_.currentMotion->animation_.nodeAnimations_;

	for (const auto& joint : skeleton->GetJoints()) {
		if (context_.currentMotion->animation_.nodeAnimations_.count(joint.GetName())) {
			InsertKeyframeFromTransform(joint.GetName(), time, joint.GetTransform());
		}
	}

	auto newAnims = context_.currentMotion->animation_.nodeAnimations_;

	context_.history.Execute(MakeLambdaCommand("全ポーズ保存 @ " + std::to_string(time) + "s",
		[this, newAnims]() {
			if (context_.currentMotion) context_.currentMotion->animation_.nodeAnimations_ = newAnims;
			context_.requireTimelineRebuild = true;
		},
		[this, oldAnims]() {
			if (context_.currentMotion) context_.currentMotion->animation_.nodeAnimations_ = oldAnims;
			context_.requireTimelineRebuild = true;
		}
	));
	context_.requireTimelineRebuild = true;
}

void MotionEditor::SetJointTransform(const std::string& bone, const QuaternionTransform& tr)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	j->SetTransform(tr);
	Object3d* target = context_.GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}

Joint* MotionEditor::FindJoint(const std::string& name) const
{
	Object3d* target = context_.GetTargetObject();
	if (!target || !target->GetModel() || !target->GetModel()->GetSkeleton()) return nullptr;
	return target->GetModel()->GetSkeleton()->GetJointByName(name);
}

QuaternionTransform MotionEditor::BufferToTransform() const
{
	QuaternionTransform tr;
	tr.translate = { context_.editT[0], context_.editT[1], context_.editT[2] };
	tr.rotate = EulerToQuaternion({ context_.editR[0] * (kPi / 180), context_.editR[1] * (kPi / 180), context_.editR[2] * (kPi / 180) });
	tr.scale = { context_.editS[0], context_.editS[1], context_.editS[2] };
	return tr;
}

void MotionEditor::SyncJointToBuffer(const std::string& bone)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	const auto& tr = j->GetTransform();
	context_.editT[0] = tr.translate.x; context_.editT[1] = tr.translate.y; context_.editT[2] = tr.translate.z;
	Vector3 e = QuaternionToEuler(tr.rotate);
	context_.editR[0] = e.x * (180 / kPi); context_.editR[1] = e.y * (180 / kPi); context_.editR[2] = e.z * (180 / kPi);
	context_.editS[0] = tr.scale.x; context_.editS[1] = tr.scale.y; context_.editS[2] = tr.scale.z;
}

void MotionEditor::SyncBufferToJoint()
{
	Joint* j = FindJoint(context_.selBone);
	if (j) {
		j->SetTransform(BufferToTransform());
		Object3d* target = context_.GetTargetObject();
		Model* m = target ? target->GetModel() : nullptr;
		if (m && m->GetSkeleton()) {
			m->GetSkeleton()->Update();
			if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
		}
	}
}

Matrix4x4 MotionEditor::GetTargetWorldMatrix() const {
	if (context_.targetObjectId != -1) {
		auto* placedObj = ObjectManager::GetInstance()->GetObjectById(context_.targetObjectId);
		if (placedObj && placedObj->worldTransform) return placedObj->worldTransform->GetMatWorld();
	}
	return previewTransform_.matWorld_;
}

Matrix4x4 MotionEditor::GetJointWorldMatrix(const std::string& boneName) const
{
	Joint* j = FindJoint(boneName);
	if (j) return j->GetWorldTransform().matWorld_ * GetTargetWorldMatrix();
	return MakeIdentity4x4();
}

void MotionEditor::ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat)
{
	Joint* j = FindJoint(boneName);
	if (!j) return;

	Matrix4x4 parentWorld = MakeIdentity4x4();
	if (j->GetWorldTransform().parent_) {
		parentWorld = j->GetWorldTransform().parent_->matWorld_ * GetTargetWorldMatrix();
	}
	else {
		parentWorld = GetTargetWorldMatrix();
	}

	Matrix4x4 newLocalMat = newWorldMat * Inverse(parentWorld);

	float lT[3], lRDeg[3], lS[3];
#ifdef USE_IMGUI
	ImGuizmo::DecomposeMatrixToComponents(&newLocalMat.m[0][0], lT, lRDeg, lS);
#endif
	if (context_.selBone == boneName) {
		context_.editT[0] = lT[0]; context_.editT[1] = lT[1]; context_.editT[2] = lT[2];
		context_.editR[0] = lRDeg[0]; context_.editR[1] = lRDeg[1]; context_.editR[2] = lRDeg[2];
		context_.editS[0] = lS[0]; context_.editS[1] = lS[1]; context_.editS[2] = lS[2];
	}

	QuaternionTransform tr;
	tr.translate = { lT[0], lT[1], lT[2] };
	tr.rotate = EulerToQuaternion({ lRDeg[0] * (kPi / 180), lRDeg[1] * (kPi / 180), lRDeg[2] * (kPi / 180) });
	tr.scale = { lS[0], lS[1], lS[2] };

	j->SetTransform(tr);

	Object3d* target = context_.GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}