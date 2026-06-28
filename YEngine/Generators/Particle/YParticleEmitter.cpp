#include "YParticleEmitter.h"

//=================================================================
// コンストラクタ
//=================================================================

YParticleEmitter::YParticleEmitter(const std::string& systemName, const Vector3& position)
	: systemName_(systemName),
	position_(position),
	emissionRate_(10.0f),
	emissionTimer_(0.0f),
	emitCount_(1),
	isActive_(true),
	autoEmit_(true)
{
}

//=================================================================
// 更新
//=================================================================

void YParticleEmitter::Update(float deltaTime) {
	// 無効または自動発生がオフの場合は何もしない
	if (!isActive_ || !autoEmit_) {
		return;
	}

	// 対象システムが存在するかチェック
	auto* system = GetTargetSystem();
	if (!system) {
		return;
	}

	// エミッターの位置をシステムにセット
	Matrix4x4 myMatrix = MakeTranslateMatrix(position_);
	system->SetParentMatrix(myMatrix);

	// 発生タイマーを更新
	emissionTimer_ += deltaTime;

	// 発生間隔を計算（秒間発生数から）
	float interval = (emissionRate_ > 0.0f) ? (1.0f / emissionRate_) : 0.0f;

	// 発生間隔に達したらパーティクルを発生
	while (emissionTimer_ >= interval) {
		EmitInternal(position_, emitCount_);
		emissionTimer_ -= interval;
	}
}

//=================================================================
// パーティクル発生
//=================================================================

/// <summary>
/// 射出
/// </summary>
/// <param name="count"></param>
void YParticleEmitter::Emit(int count) {
	if (!isActive_) return;
	// カウント未指定の場合はデフォルト値を使用
	if (count < 0) count = emitCount_;
	EmitInternal(position_, count);
}

/// <summary>
/// 常時射出
/// </summary>
/// <param name="count"></param>
void YParticleEmitter::EmitBurst(int count) {
	if (!isActive_) return;
	EmitInternal(position_, count);
}

/// <summary>
/// 追従しながら射出
/// </summary>
/// <param name="position"></param>
/// <param name="count"></param>
void YParticleEmitter::FollowEmit(const Vector3& position, int count) {
	if (!isActive_) return;
	EmitInternal(position, count);
}

/// <summary>
/// リセット関数
/// </summary>
void YParticleEmitter::Reset()
{
	emissionTimer_ = 0.0f;
}

/// <summary>
/// エミッター内に射出
/// </summary>
/// <param name="position"></param>
/// <param name="count"></param>
void YParticleEmitter::EmitInternal(const Vector3& position, int count)
{
	auto* system = GetTargetSystem();
	if (!system) return;

	// まとめて発生させるのではなく、1つずつ位置をずらして発生させる
	for (int i = 0; i < count; ++i) {
		// 形状に応じたランダムなオフセットを取得
		Vector3 offset = shape_ ? shape_->GeneratePoint() : Vector3{ 0,0,0 };

		// 最終的な発生位置
		Vector3 spawnPos = position + offset;

		// システムへ1つ発生要求
		// ※システム側が「Emit(pos, count)」で単純にposに生成する仕様であると仮定
		system->Emit(spawnPos, 1);
	}
}

//=================================================================
// 形状設定メソッド
//=================================================================

void YParticleEmitter::SetShapePoint() {
	shape_ = std::make_unique<YEmitterPoint>();
}

// ── 球体 ──
void YParticleEmitter::SetShapeSphere(float radius, bool shellOnly) {
	shape_ = std::make_unique<YEmitterSphere>(radius, shellOnly);
}

void YParticleEmitter::SetShapeSphereRange(float minRadius, float maxRadius) {
	shape_ = std::make_unique<YEmitterSphere>(minRadius, maxRadius);
}

// ── 箱型 ──
void YParticleEmitter::SetShapeBox(const Vector3& size) {
	shape_ = std::make_unique<YEmitterBox>(size);
}

void YParticleEmitter::SetShapeBoxRange(const Vector3& minSize, const Vector3& maxSize) {
	shape_ = std::make_unique<YEmitterBox>(minSize, maxSize);
}

// ── コーン ──
void YParticleEmitter::SetShapeCone(float outerAngleDeg, float height,
                                    const Vector3& direction) {
	auto cone = std::make_unique<YEmitterCone>(outerAngleDeg, height, direction);
	shape_ = std::move(cone);
}

void YParticleEmitter::SetShapeConeRange(float innerAngleDeg, float outerAngleDeg,
                                         float height,
                                         float minRadius, float maxRadius,
                                         const Vector3& direction) {
	auto cone = std::make_unique<YEmitterCone>();
	cone->innerAngle = innerAngleDeg;
	cone->outerAngle = outerAngleDeg;
	cone->height     = height;
	cone->minRadius  = minRadius;
	cone->maxRadius  = maxRadius;
	cone->direction  = direction;
	shape_ = std::move(cone);
}