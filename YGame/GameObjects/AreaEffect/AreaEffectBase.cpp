#include "AreaEffectBase.h"

void AreaEffectBase::Setup(const Vector3& center, float radius, float tickInterval)
{
	area_ = std::make_shared<CircleArea>();
	area_->Initialize(center, radius);
	area_->SetPurpose(AreaPurpose::Trigger);      // 閉じ込め用ではなくトリガー
	area_->SetStayTickInterval(tickInterval);

	// CircleArea のコールバック(void* key)を派生の仮想関数へ橋渡し。
	// key は AreaEffectManager が渡す IAreaEffectTarget*（同一値で往復するので安全）。
	area_->SetOnEnterArea([this](void* key, const Vector3&) {
		OnEnter(static_cast<IAreaEffectTarget*>(key));
	});
	area_->SetOnStayArea([this](void* key, const Vector3&) {
		OnStay(static_cast<IAreaEffectTarget*>(key));
	});
	area_->SetOnExitArea([this](void* key, const Vector3&) {
		OnExit(static_cast<IAreaEffectTarget*>(key));
	});
}

void AreaEffectBase::Drive(const std::vector<Target>& targets, float deltaTime)
{
	if (!area_ || !area_->IsActive()) return;

	for (const auto& t : targets) {
		if (!t.ptr) continue;
		area_->Update(t.position, t.ptr, deltaTime);
	}
}

void AreaEffectBase::Draw(Line* line)
{
	if (area_) area_->Draw(line);
}
