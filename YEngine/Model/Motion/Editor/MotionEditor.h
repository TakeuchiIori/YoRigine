#pragma once

// Engine
#include "Object3d/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include <Graphics/Drawer/LineManager/Line.h>
#ifdef USE_IMGUI
#include "Debugger/Gizmo/GizmoController.h"
#endif

// App
#include "MotionEditorContext.h"
#include "Panels/IMotionEditorPanel.h"

// Standard
#include <string>
#include <vector>
#include <memory>

// ============================================================
// 前方宣言
// ============================================================
class Camera;
class Joint;
#ifdef USE_IMGUI
class BoneGizmable;
#endif

// ============================================================
// モーションエディタクラス
// ============================================================
class MotionEditor
{
public:
	// ============================================================
	// 基本関数
	// ============================================================
	MotionEditor();
	~MotionEditor();

	void Initialize(Camera* camera);
	void Update();
	void Draw();
	void DrawGizmo();
	void DrawBone();
	void ShowEditor();

	// ============================================================
	// 公開関数
	// ============================================================
	void SetTargetObjectId(int id);
	int GetTargetObjectId() const { return context_.targetObjectId; }
	void SetCamera(Camera* camera)
	{
		context_.camera = camera;
		if (lineDrawer_) {
			lineDrawer_->SetCamera(camera);
		}
	}
	bool IsDrawBone() const { return context_.isDrawBone; }

	Matrix4x4 GetJointWorldMatrix(const std::string& boneName) const;
	void ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat);

	void RegisterPanel(std::unique_ptr<IMotionEditorPanel> panel);

private:
	// ============================================================
	// 内部処理関数
	// ============================================================
	void SavePose(float time);
	void InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr);

	void SetJointTransform(const std::string& bone, const QuaternionTransform& tr);
	Joint* FindJoint(const std::string& name) const;
	QuaternionTransform BufferToTransform() const;
	void SyncJointToBuffer(const std::string& bone);
	void SyncBufferToJoint();

	Matrix4x4 GetTargetWorldMatrix() const;

private:
	// ============================================================
	// メンバ変数
	// ============================================================
	static constexpr float kPi = 3.14159265f;

	MotionEditorContext context_;
	std::vector<std::unique_ptr<IMotionEditorPanel>> panels_;

	WorldTransform previewTransform_;
	std::unique_ptr<Line> lineDrawer_;

#ifdef USE_IMGUI
	std::unique_ptr<Object3d> boneObj_;
	GizmoController gizmoCtrl_;
	std::vector<std::unique_ptr<BoneGizmable>> boneGizmables_;
	std::vector<WorldTransform> boneWorldTransforms_;
#endif

	bool draggingBone_ = false;
	QuaternionTransform boneSnap_ = {};
};
