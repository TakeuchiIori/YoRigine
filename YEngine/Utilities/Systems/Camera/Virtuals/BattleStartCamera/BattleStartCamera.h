#pragma once
#include "../VirtualCamera.h"
#include <WorldTransform/WorldTransform.h>
#include "Loaders/Json/JsonManager.h"

/// <summary>
/// バトル開始時のカメラクラス
/// </summary>
class BattleStartCamera : public VirtualCamera
{
public:
	void Initialize() override;
	void Update() override;
	void DrawDebugGui() override;

	void Save(nlohmann::json& j) const override;
	void Load(const nlohmann::json& j) override;

	void SetTarget(WorldTransform* wt) { target_ = wt; }
	bool IsFinished() const { return stage_ == Stage::Done; }


private:
	enum class Stage { Approach, Hold, Exit, Done };

	struct Params
	{
		// ===== Timing =====
		float approachTime = 0.9f;  // 寄り（回り込み）時間
		float holdTime = 0.6f;  // 据えカット時間
		float exitTime = 0.8f;  // 退出時間

		// ===== Framing / Fit =====
		float subjectHeight = 1.6f; // 画面に入れたい被写体高さ
		float fovY = 0.7f;
		float fitMargin = 1.15f; // 少し余白

		// ===== Arc (Approach) =====
		float approachArcYawDeg = 60.0f; // 回り込み角度（+右回り / -左回り）
		float bankRollDeg = 6.0f;  // 回り込み中の最大ロール（0で無効）

		// ===== Positions =====
		// Start
		bool    useStartRelativeToTarget = true;  // true: ターゲット基準, false: ワールド絶対
		Vector3 startOffset{ 0.0f, 2.0f, -10.0f };    // 基準（ターゲット向きが前）
		Vector3 startOffsetRotate{ 0.0f, 0.0f, 0.0f }; // ローカル回転(XYZラジアン)

		// Hold（肩越し/対面の据え位置）
		bool    useHoldRelativeToTarget = true;
		Vector3 holdOffset{ 2.0f, 1.6f, -5.0f };
		Vector3 holdOffsetRotate{ 0.0f, 0.0f, 0.0f };

		// Exit（退出用）
		bool    useFinalRelativeToTarget = true;
		Vector3 finalOffset{ -6.0f, 3.0f, -8.0f };
		Vector3 finalOffsetRotate{ 0.0f, 0.0f, 0.0f };

		// Exit の注視
		bool    lookAtTargetOnExit = true;
	} p_;

private:
	// JSON 登録
	void InitJson();

	// 基本計算
	float ComputeFitDistance(float subjectHeight, float fovY, float margin) const;
	float GetTargetYawRad() const;
	Vector3 RotateY(const Vector3& v, float yawRad) const;

	// オフセットをターゲット基準/ワールド絶対でワールド座標に変換
	Vector3 ToWorldFromOffset(bool useRelativeToTarget,
		const Vector3& offset,
		const Vector3& offsetEuler) const;

	// 円弧補間の事前計算
	void BuildApproachArc(const Vector3& startWorld, const Vector3& holdWorld);

	// 注視を現在姿勢から算出
	void LookAtTarget();

private:
	const WorldTransform* target_ = nullptr;
	std::unique_ptr<YoRigine::JsonManager> json_;

	Stage stage_ = Stage::Approach;
	float t_ = 0.0f;

	// 端点
	Vector3 startPos_{}, holdPos_{}, finalPos_{}, exitStartPos_{}, holdStartPos_;

	// 円弧補間（XZ平面・中心はターゲット）
	float arcStartAngle_ = 0.0f;
	float arcEndAngle_ = 0.0f;
	float arcStartRadius_ = 0.0f;
	float arcEndRadius_ = 0.0f;
	float holdHeight_ = 0.0f;

	// 距離（fit）
	float fitDist_ = 6.0f;
};

