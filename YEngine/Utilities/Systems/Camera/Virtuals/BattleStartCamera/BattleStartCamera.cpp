#include "BattleStartCamera.h"

// C++
#include <algorithm>
#include <cmath>

// Engine
#include <MathFunc.h>
#include "Easing.h"
#include "Systems/Input/Input.h"
#include <Systems/GameTime/GameTime.h>

#pragma region 小ユーティリティ関数群

/// <summary>
/// 線形補間（float
/// </summary>
static inline float LerpF(float a, float b, float t) { return a + (b - a) * t; }

/// <summary>
/// 線形補間（Vector3）
/// </summary>
static inline Vector3 LerpV(const Vector3& a, const Vector3& b, float t) {
	return { LerpF(a.x, b.x, t), LerpF(a.y, b.y, t), LerpF(a.z, b.z, t) };
}

/// <summary>
/// 角度補間（ラジアン）
/// </summary>
static inline float LerpAngle(float a, float b, float t) {
	float d = b - a;
	while (d > 3.14159265f) d -= 6.28318531f;
	while (d < -3.14159265f) d += 6.28318531f;
	return a + d * t;
}

/// <summary>
/// ローカル回転（XYZ回転）をオフセットに適用
/// </summary>
static inline Vector3 RotateOffsetXYZ(const Vector3& offset, const Vector3& eulerXYZ) {
	Matrix4x4 rot = MakeRotateMatrixXYZ(eulerXYZ);
	return TransformNormal(offset, rot);
}

/// <summary>
/// from→to の方向ベクトルからヨー/ピッチを計算
/// </summary>
static inline std::pair<float, float> YawPitchTo(const Vector3& from, const Vector3& to) {
	Vector3 dir = Normalize(to - from);
	float yaw = std::atan2f(dir.x, dir.z);
	float pitch = std::asinf(dir.y);
	return { yaw, pitch };
}

#pragma endregion

///=============================================================================================
/// メイン実装
///=============================================================================================

/// <summary>
/// 被写体を画角に収めるための適正距離を算出
/// </summary>
float BattleStartCamera::ComputeFitDistance(float subjectHeight, float fovY, float margin) const {
	float half = subjectHeight * 0.5f * std::max(1.0f, margin);
	float denom = std::tan(std::max(0.1f, fovY * 0.5f));
	return (denom > 0.0f) ? (half / denom) : half * 3.0f;
}

/// <summary>
/// ターゲットのYaw角（ラジアン）を取得
/// </summary>
float BattleStartCamera::GetTargetYawRad() const {
	return target_ ? target_->rotate_.y : 0.0f;
}

/// <summary>
/// Y軸回転を適用したベクトルを返す
/// </summary>
Vector3 BattleStartCamera::RotateY(const Vector3& v, float yawRad) const {
	float s = std::sinf(yawRad), c = std::cosf(yawRad);
	return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c };
}

/// <summary>
/// ターゲットを基準とするかどうかでワールド座標を算出
/// </summary>
Vector3 BattleStartCamera::ToWorldFromOffset(bool useRelativeToTarget,
	const Vector3& offset,
	const Vector3& offsetEuler) const
{
	Vector3 local = RotateOffsetXYZ(offset, offsetEuler);

	if (!useRelativeToTarget) {
		return local; // ワールド絶対位置
	}

	Vector3 targetPos = target_ ? target_->translate_ : Vector3{ 0,0,0 };
	Vector3 rotated = RotateY(local, GetTargetYawRad());
	return targetPos + rotated;
}

/// <summary>
/// 接近時の円弧パスを構築（Start→Hold）
/// </summary>
void BattleStartCamera::BuildApproachArc(const Vector3& startWorld, const Vector3& holdWorld) {
	Vector3 targetPos = target_ ? target_->translate_ : Vector3{ 0,0,0 };
	Vector3 vS = startWorld - targetPos;
	Vector3 vH = holdWorld - targetPos;

	arcStartAngle_ = std::atan2f(vS.x, vS.z);
	arcEndAngle_ = arcStartAngle_ + (p_.approachArcYawDeg * 3.14159265f / 180.0f);
	arcStartRadius_ = std::max(0.01f, std::sqrtf(vS.x * vS.x + vS.z * vS.z));
	arcEndRadius_ = std::max(0.01f, std::sqrtf(vH.x * vH.x + vH.z * vH.z));
	holdHeight_ = holdWorld.y;
}

/// <summary>
/// 現在位置からターゲットを注視するよう回転を更新
/// </summary>
void BattleStartCamera::LookAtTarget() {
	Vector3 targetPos = target_ ? target_->translate_ : Vector3{ 0,0,0 };
	auto [yaw, pitch] = YawPitchTo(transform_.translate, targetPos);
	transform_.rotate.y = yaw;
	transform_.rotate.x = -pitch;
	transform_.rotate.z = 0.0f;
}

/// <summary>
/// JSON設定登録・初期化
/// </summary>
void BattleStartCamera::InitJson() {
	json_ = std::make_unique<YoRigine::JsonManager>("BattleStartCamera", "Resources/Json/Cameras");
	json_->SetCategory("Cameras");
	json_->SetSubCategory("BattleStart");

	//------------------------------------------------------------
	// Timing
	//------------------------------------------------------------
	json_->SetTreePrefix("Timing");
	json_->Register("ApproachTime", &p_.approachTime);
	json_->Register("HoldTime", &p_.holdTime);
	json_->Register("ExitTime", &p_.exitTime);
	json_->Register("ApproachArcYawDeg", &p_.approachArcYawDeg);
	json_->Register("BankRollDeg", &p_.bankRollDeg);
	json_->ClearTreePrefix();

	//------------------------------------------------------------
	// Framing
	//------------------------------------------------------------
	json_->SetTreePrefix("Framing");
	json_->Register("SubjectHeight", &p_.subjectHeight);
	json_->Register("FovY", &p_.fovY);
	json_->Register("FitMargin", &p_.fitMargin);
	json_->ClearTreePrefix();

	//------------------------------------------------------------
	// Positions
	//------------------------------------------------------------
	json_->SetTreePrefix("Positions");
	json_->Register("UseStartRelativeToTarget", &p_.useStartRelativeToTarget);
	json_->Register("StartOffset", &p_.startOffset);
	json_->Register("StartOffsetRotate", &p_.startOffsetRotate);
	json_->Register("UseHoldRelativeToTarget", &p_.useHoldRelativeToTarget);
	json_->Register("HoldOffset", &p_.holdOffset);
	json_->Register("HoldOffsetRotate", &p_.holdOffsetRotate);
	json_->Register("UseFinalRelativeToTarget", &p_.useFinalRelativeToTarget);
	json_->Register("FinalOffset", &p_.finalOffset);
	json_->Register("FinalOffsetRotate", &p_.finalOffsetRotate);
	json_->ClearTreePrefix();

	//------------------------------------------------------------
	// ExitLook
	//------------------------------------------------------------
	json_->SetTreePrefix("ExitLook");
	json_->Register("LookAtTargetOnExit", &p_.lookAtTargetOnExit);
	json_->ClearTreePrefix();
}

/// <summary>
/// 初期化処理
/// </summary>
void BattleStartCamera::Initialize() {
	InitJson();

	stage_ = Stage::Approach;
	t_ = 0.0f;

	//------------------------------------------------------------
	// 各種初期値の算出
	//------------------------------------------------------------
	fitDist_ = ComputeFitDistance(p_.subjectHeight, p_.fovY, p_.fitMargin);

	Vector3 targetPos = target_ ? target_->translate_ : Vector3{ 0,0,0 };

	Vector3 startWorld = ToWorldFromOffset(p_.useStartRelativeToTarget, p_.startOffset, p_.startOffsetRotate);
	auto [startYaw, _] = YawPitchTo(startWorld, targetPos);
	Vector3 approachDir = Normalize(Vector3{ std::sinf(startYaw), 0.0f, std::cosf(startYaw) });

	Vector3 fitPos = targetPos - approachDir * fitDist_;
	fitPos.y = targetPos.y + LerpF(p_.startOffset.y, 0.0f, 0.5f);

	Vector3 holdLocal = RotateOffsetXYZ(p_.holdOffset, p_.holdOffsetRotate);
	Vector3 holdWorld = fitPos + RotateY(holdLocal, GetTargetYawRad());

	transform_.translate = startWorld;
	LookAtTarget();

	startPos_ = startWorld;
	holdPos_ = holdWorld;

	BuildApproachArc(startWorld, holdWorld);

	projectionMatrix_ = Inverse(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate));
}

/// <summary>
/// 更新処理（アプローチ → ホールド → 退出）
/// </summary>
void BattleStartCamera::Update() {
	if (stage_ == Stage::Done) return;

	//------------------------------------------------------------
	// スキップ入力（Aボタン）
	//------------------------------------------------------------
	if (YoRigine::Input::GetInstance()->IsPadPressed(0, GamePadButton::A)) {
		stage_ = Stage::Done;
		return;
	}

	const float dt = YoRigine::GameTime::GetDeltaTime();
	t_ += dt;

	Vector3 targetPos = target_ ? target_->translate_ : Vector3{ 0,0,0 };

	//------------------------------------------------------------
	// Approachフェーズ
	//------------------------------------------------------------
	if (stage_ == Stage::Approach) {
		const float s = std::clamp(t_ / std::max(0.001f, p_.approachTime), 0.0f, 1.0f);
		const float u = Easing::EaseInOutCubic(s);

		const float ang = LerpAngle(arcStartAngle_, arcEndAngle_, u);
		const float rad = LerpF(arcStartRadius_, arcEndRadius_, u);
		const float h = LerpF(startPos_.y, holdHeight_, u);

		Vector3 posLocalXZ = { std::sinf(ang) * rad, h, std::cosf(ang) * rad };
		transform_.translate = targetPos + posLocalXZ;

		LookAtTarget();

		const float bankRad = (p_.bankRollDeg != 0.0f) ? (p_.bankRollDeg * 3.14159265f / 180.0f) : 0.0f;
		transform_.rotate.z = bankRad * std::sinf(u * 3.14159265f);

		if (t_ >= p_.approachTime) {
			stage_ = Stage::Hold;
			t_ = 0.0f;
			transform_.translate = holdPos_;
			LookAtTarget();
			transform_.rotate.z = 0.0f;
		}
	}
	//------------------------------------------------------------
	// Holdフェーズ
	//------------------------------------------------------------
	else if (stage_ == Stage::Hold) {
		const float s = std::clamp(t_ / std::max(0.001f, p_.holdTime), 0.0f, 1.0f);
		const float u = Easing::EaseOutCubic(s);

		transform_.translate = LerpV(holdStartPos_, holdPos_, u);
		LookAtTarget();
		transform_.rotate.z = 0.0f;

		if (t_ >= p_.holdTime) {
			stage_ = Stage::Exit;
			t_ = 0.0f;
			exitStartPos_ = transform_.translate;
			Vector3 finalWorld = ToWorldFromOffset(p_.useFinalRelativeToTarget, p_.finalOffset, p_.finalOffsetRotate);
			finalPos_ = finalWorld;
		}
	}
	//------------------------------------------------------------
	// Exitフェーズ
	//------------------------------------------------------------
	else if (stage_ == Stage::Exit) {
		const float s = std::clamp(t_ / std::max(0.001f, p_.exitTime), 0.0f, 1.0f);
		const float u = Easing::EaseOutCubic(s);

		transform_.translate = LerpV(exitStartPos_, finalPos_, u);

		if (p_.lookAtTargetOnExit) {
			LookAtTarget();
		}

		transform_.rotate.z = 0.0f;

		if (t_ >= p_.exitTime) {
			stage_ = Stage::Done;
		}
	}

	//------------------------------------------------------------
	// ビュー行列更新
	//------------------------------------------------------------
	projectionMatrix_ = Inverse(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate));
}
void BattleStartCamera::DrawDebugGui()
{

}
