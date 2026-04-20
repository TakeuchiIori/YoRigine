#pragma once
#include <string>
#include <vector>
#include <memory>

#include "Object3d/Object3d.h"
#include "WorldTransform/WorldTransform.h"
#include <Graphics/Drawer/LineManager/Line.h>

#include "MotionEditorContext.h"
#include "Panels/IMotionEditorPanel.h"

#ifdef USE_IMGUI
#include "Debugger/Gizmo/GizmoController.h"
#endif

class Camera;
class Joint;

#ifdef USE_IMGUI
class BoneGizmable;
#endif

class MotionEditor
{
public:
	MotionEditor();
	~MotionEditor();

	void Initialize(Camera* camera);
	void Update();
	void Draw();
	void DrawGizmo();
	void DrawBone();
	void ShowEditor();

	void SetTargetObjectId(int id);
	int GetTargetObjectId() const { return context_.targetObjectId; }
	void SetCamera(Camera* camera) { context_.camera = camera; }
	bool IsDrawBone() const { return context_.isDrawBone; }

	Matrix4x4 GetJointWorldMatrix(const std::string& boneName) const;
	void ApplyBoneGizmoTransform(const std::string& boneName, const Matrix4x4& newWorldMat);

	void RegisterPanel(std::unique_ptr<IMotionEditorPanel> panel);

private:
	void SavePose(float time);
	void InsertKeyframeFromTransform(const std::string& bone, float time, const QuaternionTransform& tr);

	void SetJointTransform(const std::string& bone, const QuaternionTransform& tr);
	Joint* FindJoint(const std::string& name) const;
	QuaternionTransform BufferToTransform() const;
	void SyncJointToBuffer(const std::string& bone);
	void SyncBufferToJoint();

	Matrix4x4 GetTargetWorldMatrix() const;

private:
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