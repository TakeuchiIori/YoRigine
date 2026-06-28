#pragma once

#ifdef USE_IMGUI

#include "Debugger/Gizmo/IGizmable.h"
#include "MathFunc.h"

class MotionEditor;

/// <summary>
/// ボーンを GizmoController で操作するためのアダプタクラス
/// </summary>
class BoneGizmable : public IGizmable
{
public:
	BoneGizmable(MotionEditor* editor, const std::string& boneName)
		: editor_(editor), boneName_(boneName) {
	}

	// =================================================================
	// IGizmable のオーバーライド
	// =================================================================

	std::string GetGizmoLabel() const override { return "Bone:" + boneName_; }

	// ICO.obj の 1/10 スケールに合わせてピッキング（クリック判定）の半径を小さく設定
	float GetGizmoPickRadius() const override { return 0.2f; }

	Vector3 GetGizmoPosition() const override { return worldPos_; }
	void SetGizmoPosition(const Vector3& pos) override { worldPos_ = pos; isDirty_ = true; }

	Vector3 GetGizmoRotation() const override { return worldRot_; }
	void SetGizmoRotation(const Vector3& rot) override { worldRot_ = rot; isDirty_ = true; }

	Vector3 GetGizmoScale() const override { return worldScale_; }
	void SetGizmoScale(const Vector3& scale) override { worldScale_ = scale; isDirty_ = true; }

	// =================================================================
	// 同期処理
	// =================================================================

	/// <summary>
	/// 毎フ��ーム、実際の Joint が持つ WorldTransform からワールド座標を算出して同期する
	/// （ギズモを描画する直前に呼び出す）
	/// </summary>
	void UpdateFromJoint();

	/// <summary>
	/// ギズモ操作終了時（マウスドラッグ中～終了時）に呼ばれる。
	/// 新しいワールド座標を MotionEditor に渡し、適用を依頼する。
	/// </summary>
	void OnGizmoManipulationEnd() override;

	const std::string& GetBoneName() const { return boneName_; }

private:
	MotionEditor* editor_ = nullptr;
	std::string boneName_;

	Vector3 worldPos_{};
	Vector3 worldRot_{};
	Vector3 worldScale_{ 1, 1, 1 };

	// ギズモによって値が変更されたフラグ
	bool isDirty_ = false;
};

#endif // USE_IMGUI