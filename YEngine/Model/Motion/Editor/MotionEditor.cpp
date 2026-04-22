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

// App
#include "Panels/MenuBarPanel.h"
#include "Panels/SavePanel.h"
#include "Panels/ToolbarPanel.h"
#include "Panels/BoneListPanel.h"
#include "Panels/PropertyPanel.h"
#include "Panels/TimelinePanel.h"
#include "Panels/StatusBarPanel.h"

// External
#ifdef USE_IMGUI
#include "ImGuizmo.h"
#include <imgui.h>
#endif

// Standard
#include <algorithm>
#include <cmath>

// ============================================================
// コンテキストヘルパーの実装
// ============================================================
Object3d* MotionEditorContext::GetTargetObject() const {
	if (targetObjectId != -1) {
		return ObjectManager::GetInstance()->GetObject3dById(targetObjectId);
	}
	return nullptr;
}

// ============================================================
// コンストラクタ
// ============================================================
MotionEditor::MotionEditor() {}

// ============================================================
// デストラクタ
// ============================================================
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
	// デバッグ表示用のボーンオブジェクト生成
	boneObj_ = Object3d::Create("ICO.obj");
	gizmoCtrl_.Initialize();
	gizmoCtrl_.SetMode(GizmoController::Mode::Local);
#endif

	//------------------------------------------------------------
	// コンテキストのコールバック設定 (UIとロジックの分離)
	//------------------------------------------------------------
	context_.SyncJointToBuffer = [this]() { SyncJointToBuffer(context_.selBone); };
	context_.SyncBufferToJoint = [this]() { SyncBufferToJoint(); };

	// キーフレーム追加処理のラムダ登録
	context_.AddKeyframe = [this](const std::string& bone, float time) {
		if (!context_.currentMotion) return;
		auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
		if (!nodeAnims.count(bone)) return;

		// 現在のUI編集バッファからトランスフォームを作成
		Vector3 newT = { context_.editT[0], context_.editT[1], context_.editT[2] };
		Quaternion newR = EulerToQuaternion({ context_.editR[0] * (kPi / 180), context_.editR[1] * (kPi / 180), context_.editR[2] * (kPi / 180) });
		Vector3 newS = { context_.editS[0], context_.editS[1], context_.editS[2] };
		Motion::NodeAnimation snap = nodeAnims[bone];

		// コマンド履歴に登録して実行
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

	// キーフレーム削除処理のラムダ登録
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

	//------------------------------------------------------------
	// パネルの登録
	//------------------------------------------------------------
	RegisterPanel(std::make_unique<MenuBarPanel>());
	RegisterPanel(std::make_unique<SavePanel>());
	RegisterPanel(std::make_unique<ToolbarPanel>());
	RegisterPanel(std::make_unique<BoneListPanel>());
	RegisterPanel(std::make_unique<PropertyPanel>());
	RegisterPanel(std::make_unique<TimelinePanel>());
	RegisterPanel(std::make_unique<StatusBarPanel>());
}

// ============================================================
// パネル登録
// ============================================================
void MotionEditor::RegisterPanel(std::unique_ptr<IMotionEditorPanel> panel)
{
	panel->Initialize(&context_);
	panels_.push_back(std::move(panel));
}

// ============================================================
// 更新処理
// ============================================================
void MotionEditor::Update()
{
	Object3d* target = context_.GetTargetObject();
	if (!target) return;

	Model* m = target->GetModel();
	MotionSystem* ms = m ? m->GetMotionSystem() : nullptr;

	//------------------------------------------------------------
	// アニメーション再生時間の同期
	//------------------------------------------------------------
	if (ms) {
		float msTime = ms->GetAnimationTime();
		if (std::abs(msTime - context_.scrubTime) > 1e-4f) {
			context_.scrubTime = msTime;
		}
	}

	//------------------------------------------------------------
	// ボーン・スケルトンの更新
	//------------------------------------------------------------
	if (m && m->GetSkeleton()) {
		// 非再生中の場合、スクラブ時間に合わせたポーズを強制適用
		if (ms && !context_.isPlaying && context_.currentMotion) {
			context_.currentMotion->ApplyAnimation(m->GetSkeleton()->GetJoints(), context_.scrubTime);

			if (!context_.selBone.empty()) {
#ifdef USE_IMGUI
				// 操作中の場合はジョイントへ同期、それ以外はジョイントから取得
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

		// 行列計算とスキニング用パレットの更新
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) {
			m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
		}
	}

	previewTransform_.UpdateMatrix();

	// 各登録パネルのロジックを回す
	for (auto& panel : panels_) {
		if (panel->IsActive()) panel->Update();
	}
}

// ============================================================
// エディタUIの描画表示
// ============================================================
void MotionEditor::ShowEditor()
{
#ifdef USE_IMGUI
	ImGui::SetNextWindowSize(ImVec2(1200, 750), ImGuiCond_FirstUseEver);
	ImGui::Begin("Motion Editor", nullptr, ImGuiWindowFlags_MenuBar);

	Object3d* target = context_.GetTargetObject();

	//------------------------------------------------------------
	// 3Dビュー上でのボーン選択＆ギズモ処理
	//------------------------------------------------------------
	if (context_.isDrawBone && target && target->GetModel() && target->GetModel()->GetSkeleton()) {
		// クリックによるボーンのピッキング判定
		if (ImGui::IsMouseClicked(0) && !ImGuizmo::IsOver() && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)) {
			gizmoCtrl_.ClearPickables();
			boneGizmables_.clear();

			// 全ジョイントをピッキング対象として登録
			auto& joints = target->GetModel()->GetSkeleton()->GetJoints();
			for (const auto& joint : joints) {
				auto bg = std::make_unique<BoneGizmable>(this, joint.GetName());
				bg->UpdateFromJoint();
				gizmoCtrl_.RegisterPickable(bg.get());
				boneGizmables_.push_back(std::move(bg));
			}

			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();

			// レイキャスト等による当たり判定実行
			IGizmable* picked = gizmoCtrl_.TryPickObject(ImGui::GetMousePos(), viewPos, viewSize, context_.camera);
			if (picked) {
				BoneGizmable* bg = static_cast<BoneGizmable*>(picked);
				context_.selBone = bg->GetBoneName();
				SyncJointToBuffer(context_.selBone);
				context_.statusMsg = "ボーン選択: " + context_.selBone;
			}
		}

		// 選択中ボーンに対するトランスフォームギズモの表示
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

	//------------------------------------------------------------
	// キーボードショートカット処理
	//------------------------------------------------------------
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		context_.history.HandleKeyInput();

		// 再生・一時停止の切り替え
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
		// 選択中のキーフレーム削除
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

	//------------------------------------------------------------
	// オブジェクト未選択時のガード表示
	//------------------------------------------------------------
	if (!target || !target->GetModel()) {
		panels_[0]->DrawImGui(); // MenuBar
		panels_[1]->DrawImGui(); // SaveLoad

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 40);
		float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX((w - 220) * 0.5f);
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "シーンエディタでモデルを選択してください");
		ImGui::End();
		return;
	}

	//------------------------------------------------------------
	// メインパネル群のレイアウト構成
	//------------------------------------------------------------
	panels_[0]->DrawImGui(); // MenuBar
	panels_[1]->DrawImGui(); // SaveLoad
	panels_[2]->DrawImGui(); // Toolbar
	ImGui::Separator();

	float contentW = ImGui::GetContentRegionAvail().x;
	float upperH = ImGui::GetContentRegionAvail().y - context_.timelineH - 30.0f;

	// 上部エリア: ボーンリスト(左) と プロパティ(右)
	ImGui::BeginChild("##upper", ImVec2(contentW, upperH), false);
	{
		ImGui::BeginChild("##bones", ImVec2(context_.bonePanelW, 0), true);
		panels_[3]->DrawImGui();
		ImGui::EndChild();

		ImGui::SameLine();

		// 垂直リサイズバーの処理
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

		ImGui::BeginChild("##props", ImVec2(0, 0), true);
		panels_[4]->DrawImGui();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	// 水平リサイズバーの処理 (タイムラインの高さ調節)
	ImGuiIO& io_h = ImGui::GetIO();
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	ImGui::InvisibleButton("##hsep", ImVec2(contentW, 4.0f));
	if (ImGui::IsItemActive()) {
		context_.timelineH -= io_h.MouseDelta.y;
	}
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(100, 130, 200, 180));
	}
	ImGui::PopStyleVar();

	panels_[5]->DrawImGui(); // Timeline
	panels_[6]->DrawImGui(); // StatusBar

	ImGui::End();
#endif
}

// ============================================================
// モデル描画
// ============================================================
void MotionEditor::Draw()
{
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target) {
#ifdef USE_IMGUI
		// デバッグ用ボーン形状（ICO球など）の描画
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

				// 選択ボーンをハイライト
				if (context_.selBone == joints[i].GetName()) boneObj_->SetMaterialColor({ 1.0f, 0.85f, 0.2f, 1.0f });
				else boneObj_->SetMaterialColor({ 0.5f, 0.5f, 0.5f, 1.0f });

				boneObj_->Draw(context_.camera, wt);
			}
		}
#endif
	}
}

// ============================================================
// ギズモの描画
// ============================================================
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
			// ギズモ操作中でなければ最新のボーン位置に同期
			if (!gizmoCtrl_.IsUsing()) currentSelectedGizmo.UpdateFromJoint();

			std::vector<IGizmable*> gizmoTargets = { &currentSelectedGizmo };
			ImVec2 viewPos = Editor::GetInstance()->GetGameViewPos();
			ImVec2 viewSize = Editor::GetInstance()->GetGameViewSize();
			gizmoCtrl_.Draw(context_.camera, gizmoTargets, viewPos, viewSize);
		}
	}
#endif
}

// ============================================================
// ボーン構造の描画（ライン表示）
// ============================================================
void MotionEditor::DrawBone()
{
	Object3d* target = context_.GetTargetObject();
	if (context_.isDrawBone && target) {
		lineDrawer_->SetCamera(context_.camera);
		target->DrawBone(*lineDrawer_.get(), GetTargetWorldMatrix());
	}
}

// ============================================================
// ターゲットオブジェクトの設定
// ============================================================
void MotionEditor::SetTargetObjectId(int id) {
	Object3d* obj = nullptr;
	if (id != -1) {
		obj = ObjectManager::GetInstance()->GetObject3dById(id);
		// アニメーションシステムを持っていないオブジェクトは対象外とするガード
		if (obj && obj->GetModel()) {
			if (obj->GetModel()->GetMotionSystem() == nullptr) id = -1;
		}
		else id = -1;
	}

	if (context_.targetObjectId != id) {
		context_.targetObjectId = id;

		// ターゲット変更に伴うコンテキストの初期化・同期
		if (context_.targetObjectId != -1) {
			Object3d* target = context_.GetTargetObject();
			Model* model = target->GetModel();
			MotionSystem* ms = model->GetMotionSystem();

			context_.loadFileName = model->GetName();
			context_.currentMotion = ms->GetAnimation();
			context_.selectedAnimKey = "";

			// キャッシュからアニメーションキーを逆引き
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
			// 解除時のクリーンアップ
			context_.currentMotion = nullptr;
			context_.selectedAnimKey = "";
			context_.selBone = "";
			context_.selKF.Clear();
			context_.loadFileName = "";
			context_.statusMsg = "Ready";
		}
	}
}

// ============================================================
// トランスフォームからキーフレームを挿入
// ============================================================
void MotionEditor::InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr)
{
	if (!context_.currentMotion) return;
	auto& nodeAnims = context_.currentMotion->animation_.nodeAnimations_;
	if (!nodeAnims.count(bone)) return;

	// 指定時間に既存KFがあれば上書き、なければ新規挿入
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

// ============================================================
// 現在のポーズを全ボーン分保存
// ============================================================
void MotionEditor::SavePose(float time)
{
	Object3d* target = context_.GetTargetObject();
	if (!target || !target->GetModel() || !target->GetModel()->GetSkeleton() || !context_.currentMotion) return;

	Skeleton* skeleton = target->GetModel()->GetSkeleton();
	auto oldAnims = context_.currentMotion->animation_.nodeAnimations_;

	// 全ジョイントの現在の姿勢をKFとして刻む
	for (const auto& joint : skeleton->GetJoints()) {
		if (context_.currentMotion->animation_.nodeAnimations_.count(joint.GetName())) {
			InsertKeyframeFromTransform(joint.GetName(), time, joint.GetTransform());
		}
	}

	auto newAnims = context_.currentMotion->animation_.nodeAnimations_;

	// コマンドとして履歴登録
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

// ============================================================
// ジョイントのトランスフォーム設定
// ============================================================
void MotionEditor::SetJointTransform(const std::string& bone, const QuaternionTransform& tr)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	j->SetTransform(tr);

	// トランスフォーム変更をスケルトン全体に伝搬させる
	Object3d* target = context_.GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}

// ============================================================
// ボーン名からジョイントを検索
// ============================================================
Joint* MotionEditor::FindJoint(const std::string& name) const
{
	Object3d* target = context_.GetTargetObject();
	if (!target || !target->GetModel() || !target->GetModel()->GetSkeleton()) return nullptr;
	return target->GetModel()->GetSkeleton()->GetJointByName(name);
}

// ============================================================
// 編集バッファからトランスフォーム構造体を作成
// ============================================================
QuaternionTransform MotionEditor::BufferToTransform() const
{
	QuaternionTransform tr;
	tr.translate = { context_.editT[0], context_.editT[1], context_.editT[2] };
	tr.rotate = EulerToQuaternion({ context_.editR[0] * (kPi / 180), context_.editR[1] * (kPi / 180), context_.editR[2] * (kPi / 180) });
	tr.scale = { context_.editS[0], context_.editS[1], context_.editS[2] };
	return tr;
}

// ============================================================
// ジョイントの値を編集バッファへ同期
// ============================================================
void MotionEditor::SyncJointToBuffer(const std::string& bone)
{
	Joint* j = FindJoint(bone);
	if (!j) return;
	const auto& tr = j->GetTransform();

	// 内部値をUI用のバッファ（float配列）に展開
	context_.editT[0] = tr.translate.x; context_.editT[1] = tr.translate.y; context_.editT[2] = tr.translate.z;
	Vector3 e = QuaternionToEuler(tr.rotate);
	context_.editR[0] = e.x * (180 / kPi); context_.editR[1] = e.y * (180 / kPi); context_.editR[2] = e.z * (180 / kPi);
	context_.editS[0] = tr.scale.x; context_.editS[1] = tr.scale.y; context_.editS[2] = tr.scale.z;
}

// ============================================================
// 編集バッファの値をジョイントへ同期
// ============================================================
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

// ============================================================
// ターゲットのワールド行列を取得
// ============================================================
Matrix4x4 MotionEditor::GetTargetWorldMatrix() const {
	// 配置済みオブジェクトの場合はそのワールド行列、そうでなければプレビュー用行列を返す
	if (context_.targetObjectId != -1) {
		auto* placedObj = ObjectManager::GetInstance()->GetObjectById(context_.targetObjectId);
		if (placedObj && placedObj->worldTransform) return placedObj->worldTransform->GetMatWorld();
	}
	return previewTransform_.matWorld_;
}

// ============================================================
// ジョイントのワールド行列を取得
// ============================================================
Matrix4x4 MotionEditor::GetJointWorldMatrix(const std::string& boneName) const
{
	Joint* j = FindJoint(boneName);
	if (j) return j->GetWorldTransform().matWorld_ * GetTargetWorldMatrix();
	return MakeIdentity4x4();
}

// ============================================================
// ギズモ操作によるボーン変換の適用
// ============================================================
void MotionEditor::ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat)
{
	Joint* j = FindJoint(boneName);
	if (!j) return;

	//------------------------------------------------------------
	// 親のワールド行列を考慮して新しいローカル行列を算出
	//------------------------------------------------------------
	Matrix4x4 parentWorld = MakeIdentity4x4();
	if (j->GetWorldTransform().parent_) {
		parentWorld = j->GetWorldTransform().parent_->matWorld_ * GetTargetWorldMatrix();
	}
	else {
		parentWorld = GetTargetWorldMatrix();
	}

	// 逆行列を掛けてローカル空間に戻す
	Matrix4x4 newLocalMat = newWorldMat * Inverse(parentWorld);

	// 行列を要素（TRS）に分解してバッファとジョイントに適用
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

	//------------------------------------------------------------
	// 表示の即時反映
	//------------------------------------------------------------
	Object3d* target = context_.GetTargetObject();
	Model* m = target ? target->GetModel() : nullptr;
	if (m && m->GetSkeleton()) {
		m->GetSkeleton()->Update();
		if (m->GetSkinCluster()) m->GetSkinCluster()->UpdateMatrixPalette(m->GetSkeleton()->GetJoints());
	}
}