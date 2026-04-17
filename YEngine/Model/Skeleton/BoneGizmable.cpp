#include "BoneGizmable.h"
#include <Motion/MotionEditor.h>

#ifdef USE_IMGUI
#include "ImGuizmo.h"

void BoneGizmable::UpdateFromJoint()
{
	// Editor 経由で対象 Joint のワールド行列を取得
	Matrix4x4 wMat = editor_->GetJointWorldMatrix(boneName_);

	float t[3], rDeg[3], s[3];
	ImGuizmo::DecomposeMatrixToComponents(&wMat.m[0][0], t, rDeg, s);

	worldPos_ = { t[0], t[1], t[2] };
	// ImGuizmo は Degree(度数法) で返すため、Radian(弧度法) に変換して保持
	worldRot_ = { rDeg[0] * (3.14159265f / 180.0f), rDeg[1] * (3.14159265f / 180.0f), rDeg[2] * (3.14159265f / 180.0f) };
	worldScale_ = { s[0], s[1], s[2] };
	isDirty_ = false;
}

void BoneGizmable::OnGizmoManipulationEnd()
{
	if (!isDirty_) return;

	// 1. ギズモで操作された新しいワールド行列を作成
	float t[3] = { worldPos_.x, worldPos_.y, worldPos_.z };
	float rDeg[3] = { worldRot_.x * (180.0f / 3.14159265f), worldRot_.y * (180.0f / 3.14159265f), worldRot_.z * (180.0f / 3.14159265f) };
	float s[3] = { worldScale_.x, worldScale_.y, worldScale_.z };

	Matrix4x4 newWorldMat;
	ImGuizmo::RecomposeMatrixFromComponents(t, rDeg, s, &newWorldMat.m[0][0]);

	// 2. 新しいワールド行列の適用を MotionEditor に委譲する
	editor_->ApplyBoneGizmoTransform(boneName_, newWorldMat);

	isDirty_ = false;
}

#endif // USE_IMGUI