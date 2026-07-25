#include "AreaEffectManager.h"

#include <algorithm>

void AreaEffectManager::RegisterTarget(IAreaEffectTarget* target)
{
	if (!target) return;
	if (std::find(targets_.begin(), targets_.end(), target) != targets_.end()) return;
	targets_.push_back(target);
}

void AreaEffectManager::UnregisterTarget(IAreaEffectTarget* target)
{
	if (!target) return;

	targets_.erase(std::remove(targets_.begin(), targets_.end(), target), targets_.end());

	// 各エリアが内部に保持する in/out 状態からも忘却する（ダングリングキー防止）。
	for (auto& [name, effect] : effects_) {
		if (effect && effect->GetArea()) {
			effect->GetArea()->ForgetTarget(target);
		}
	}
}

void AreaEffectManager::ClearTargets()
{
	targets_.clear();
}

void AreaEffectManager::AddEffect(const std::string& name, std::shared_ptr<AreaEffectBase> effect)
{
	if (!effect) return;
	effects_[name] = effect;
}

void AreaEffectManager::RemoveEffect(const std::string& name)
{
	effects_.erase(name);
}

std::shared_ptr<AreaEffectBase> AreaEffectManager::GetEffect(const std::string& name)
{
	auto it = effects_.find(name);
	return (it != effects_.end()) ? it->second : nullptr;
}

void AreaEffectManager::ClearEffects()
{
	effects_.clear();
}

void AreaEffectManager::Clear()
{
	targets_.clear();
	effects_.clear();
}

void AreaEffectManager::Update(float deltaTime)
{
	if (effects_.empty() || targets_.empty()) return;

	// 生存中の対象の位置を1回だけ収集して全エリアで使い回す。
	scratch_.clear();
	scratch_.reserve(targets_.size());
	for (IAreaEffectTarget* t : targets_) {
		if (!t || !t->IsEffectTargetAlive()) continue;
		scratch_.push_back({ t, t->GetEffectPosition() });
	}

	for (auto& [name, effect] : effects_) {
		if (effect) effect->Drive(scratch_, deltaTime);
	}
}

void AreaEffectManager::Draw(YoRigine::Line* line)
{
	for (auto& [name, effect] : effects_) {
		if (effect) effect->Draw(line);
	}
}
