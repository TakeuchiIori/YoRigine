#pragma once

#include <Vector3.h>
#include <Matrix4x4.h>
#include <WorldTransform/WorldTransform.h>
#include <Loaders/Json/JsonManager.h>
#include <memory>

/// <summary>
/// Tales of Graces 風の「戦闘開始カメラ」
/// - アプローチ: ターゲット周りを扇状に回り込み＋ドリー（軽いバンク）
/// - ホールド: 肩越し/対面など任意の据えカット
/// - 退出: 任意の位置へフェードアウト用/通常カメラへの受け渡し
/// 位置指定は「ターゲット基準（ターゲットの向きに整列）」または「ワールド絶対」
/// </summary>
class BattleStartCamera
{
public:
	void Initialize();
	void Update();

	void SetTarget(WorldTransform* wt) { target_ = wt; }
	const Matrix4x4& GetViewMatrix() const { return matView_; }
	bool IsFinished() const { return stage_ == Stage::Done; }

	// カメラの現在姿勢（必要に応じて外から読む想定）
	Vector3 translate_{ 0.0f, 2.0f, -8.0f };
	Vector3 rotate_{ 0.0f, 0.0f, 0.0f };   // x=pitch(下向き+), y=yaw(右旋回+), z=roll(右傾き+)
	Vector3 scale_{ 1.0f, 1.0f, 1.0f };
	Matrix4x4 matView_{};

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

	// 円弧補間の事前計算（アプローチ）
	void BuildApproachArc(const Vector3& startWorld, const Vector3& holdWorld);

	// 注視を現在姿勢から算出（ターゲットを見る）
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
