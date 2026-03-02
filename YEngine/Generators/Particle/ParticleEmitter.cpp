#include "ParticleEmitter.h"
#ifdef USE_IMGUI
#include "imgui.h"
#endif

#include <Systems/GameTime/GameTime.h>

/// <summary>
/// コンストラクタ
/// ・対象のパーティクル名
/// ・初期位置
/// ・一度に発生させる個数
/// を指定してエミッターを生成
/// </summary>
ParticleEmitter::ParticleEmitter(const std::string& particleName, const Vector3& position, uint32_t count)
	: particleName_(particleName),
	position_(position),
	count_(count),
	emissionRate_(10.0f),   // 1秒あたりのエミッション数
	timer_(0.0f),
	isActive_(true),
	autoEmit_(false) {
}

/// <summary>
/// 毎フレーム更新処理
/// 自動エミッションが有効な場合のみ、一定間隔でパーティクルを発生させる
/// </summary>
void ParticleEmitter::Update(float deltaTime) {
	if (!isActive_ || !autoEmit_) return;

	// 前回の Emit からの経過時間を測定
	timer_ += deltaTime;

	// 1秒に emissionRate_ 回 Emit する
	if (timer_ >= 1.0f / emissionRate_) {
		YoRigine::ParticleManager::GetInstance()->Emit(particleName_, position_, count_);
		timer_ = 0.0f;    // インターバルリセット
	}
}

/// <summary>
/// 外部から明示的に Emit する
/// count < 0 の場合はデフォルト個数 count_ を使用する
/// </summary>
void ParticleEmitter::Emit(int count) {
	if (count < 0) count = count_;
	YoRigine::ParticleManager::GetInstance()->Emit(particleName_, position_, count);
}

/// <summary>
/// バースト（一度に大量発生）
/// </summary>
void ParticleEmitter::EmitBurst(int count) {
	YoRigine::ParticleManager::GetInstance()->EmitBurst(particleName_, position_, count);
}

/// <summary>
/// 追従エミット
/// pos の位置にパーティクルを発生させる
/// （エミッター自身の position_ は使用しない）
/// </summary>
void ParticleEmitter::FollowEmit(const Vector3& pos, int count)
{
	YoRigine::ParticleManager::GetInstance()->Emit(particleName_, pos, count);
}

/// <summary>
/// エミットされるパーティクルの重力を変更する
/// </summary>
void ParticleEmitter::SetParticleGravity(const Vector3& gravity) {
	auto* system = YoRigine::ParticleManager::GetInstance()->GetSystem(particleName_);
	if (system) {
		system->GetSettings().SetGravity(gravity);
	}
}

/// <summary>
/// パーティクルの開始色と終了色を設定
/// グラデーション発色に使用
/// </summary>
void ParticleEmitter::SetParticleColor(const Vector4& startColor, const Vector4& endColor) {
	auto* system = YoRigine::ParticleManager::GetInstance()->GetSystem(particleName_);
	if (system) {
		system->GetSettings().SetStartColor(startColor);
		system->GetSettings().SetEndColor(endColor);
	}
}

/// <summary>
/// パーティクルの初期速度を設定
/// </summary>
void ParticleEmitter::SetParticleSpeed(float speed) {
	auto* system = YoRigine::ParticleManager::GetInstance()->GetSystem(particleName_);
	if (system) {
		system->GetSettings().SetSpeed(speed);
	}
}

/// <summary>
/// パーティクルの寿命範囲（最短 / 最長時間）を設定
/// </summary>
void ParticleEmitter::SetParticleLifeTime(const Vector2& lifeTimeRange) {
	auto* system = YoRigine::ParticleManager::GetInstance()->GetSystem(particleName_);
	if (system) {
		system->GetSettings().SetLifeTimeRange(lifeTimeRange);
	}
}
