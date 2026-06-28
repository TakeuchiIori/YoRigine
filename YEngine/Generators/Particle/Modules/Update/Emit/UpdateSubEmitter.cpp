#include "UpdateSubEmitter.h"
#include "../../ParticleAttribute.h"
#include "../../../ParticleMath.h"
#include "../../../YParticleManager.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include <vector>
#endif

void UpdateSubEmitter::OnUpdate(ParticleAttribute* attrs, uint32_t index, float dt)
{
    if (trigger_ != Trigger::Continuous) return;
    if (childSystem_.empty()) return;

    // per-particle・ステートレスな確率発生で、おおよそ rate_ 個/秒 を撒く。
    float chance = rate_ * dt * probability_;
    if (ParticleMath::Random01() < chance) {
        EmitChild(attrs[index].position);
    }
}

void UpdateSubEmitter::OnDeath(ParticleAttribute* attrs, uint32_t index)
{
    if (trigger_ != Trigger::OnDeath) return;
    if (childSystem_.empty()) return;

    // 発生確率の抽選
    if (probability_ < 1.0f && ParticleMath::Random01() > probability_) return;

    EmitChild(attrs[index].position);
}

void UpdateSubEmitter::EmitChild(const Vector3& position)
{
    // 子システムが無ければ Emit 側で無視される（安全）
    YParticleManager::GetInstance().Emit(childSystem_, position, count_);
}

void UpdateSubEmitter::DrawEditor()
{
#ifdef USE_IMGUI
    // --- トリガ種別 ---
    const char* triggerLabels[] = { "死亡時 (OnDeath)", "継続 (Continuous)" };
    int trig = static_cast<int>(trigger_);
    if (ImGui::Combo("トリガ", &trig, triggerLabels, IM_ARRAYSIZE(triggerLabels))) {
        trigger_ = static_cast<Trigger>(trig);
    }

    // --- 子システム選択（登録済みシステムから選ぶ） ---
    std::vector<std::string> names = YParticleManager::GetInstance().GetAllSystemNames();
    const char* preview = childSystem_.empty() ? "(未選択)" : childSystem_.c_str();
    if (ImGui::BeginCombo("子システム", preview)) {
        for (const auto& n : names) {
            bool selected = (n == childSystem_);
            if (ImGui::Selectable(n.c_str(), selected)) {
                childSystem_ = n;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // --- 発生パラメータ ---
    ImGui::DragInt("発生数", &count_, 1.0f, 1, 256);
    if (trigger_ == Trigger::Continuous) {
        ImGui::SliderFloat("発生レート(個/秒)", &rate_, 0.0f, 200.0f);
    }
    ImGui::SliderFloat("発生確率", &probability_, 0.0f, 1.0f);

    ImGui::TextWrapped("親の粒から子システムを発生させます。OnDeath=死亡時に1回 / Continuous=生存中に継続。"
                       "子の同時数は子システムの maxParticles で頭打ちになります。");
#endif
}

void UpdateSubEmitter::SaveToJson(nlohmann::json& json) const
{
    json = {
        {"childSystem", childSystem_},
        {"trigger",     static_cast<int>(trigger_)},
        {"count",       count_},
        {"rate",        rate_},
        {"probability", probability_}
    };
}

void UpdateSubEmitter::LoadFromJson(const nlohmann::json& json)
{
    if (json.contains("childSystem")) childSystem_ = json["childSystem"].get<std::string>();
    if (json.contains("trigger"))     trigger_     = static_cast<Trigger>(json["trigger"].get<int>());
    if (json.contains("count"))       count_       = json["count"].get<int>();
    if (json.contains("rate"))        rate_        = json["rate"].get<float>();
    if (json.contains("probability")) probability_ = json["probability"].get<float>();
}
