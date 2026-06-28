#include "YEmitterGroup.h"

// コンストラクタ
YEmitterGroup::YEmitterGroup(const std::string& groupName)
    : groupName_(groupName)
    , isActive_(true)
    , position_({ 0, 0, 0 })
{
}

// デストラクタ
YEmitterGroup::~YEmitterGroup() = default;

// エミッターを追加
YParticleEmitter& YEmitterGroup::AddEmitter(const std::string& systemName, const Vector3& localOffset) {
    auto emitter = std::make_unique<YParticleEmitter>(systemName, position_ + localOffset);
    emitters_.push_back(std::move(emitter));
    return *emitters_.back();
}

// エミッター取得
YParticleEmitter* YEmitterGroup::GetEmitter(size_t index) {
    if (index < emitters_.size()) return emitters_[index].get();
    return nullptr;
}

// エミッター数
size_t YEmitterGroup::GetEmitterCount() const {
    return emitters_.size();
}

// エミッター削除
void YEmitterGroup::RemoveEmitter(size_t index) {
    if (index < emitters_.size()) {
        emitters_.erase(emitters_.begin() + index);
    }
}

// 更新
void YEmitterGroup::Update(float deltaTime) {
    if (!isActive_) return;
    for (auto& emitter : emitters_) {
        emitter->Update(deltaTime);
    }
}

// 全エミッター発生
void YEmitterGroup::EmitAll(int count) {
    for (auto& emitter : emitters_) {
        emitter->Emit(count);
    }
}

// 位置設定（オフセット維持）
void YEmitterGroup::SetPosition(const Vector3& pos) {
    for (auto& emitter : emitters_) {
        Vector3 oldPos = emitter->GetPosition();
        Vector3 offset = {
            oldPos.x - position_.x,
            oldPos.y - position_.y,
            oldPos.z - position_.z
        };
        emitter->SetPosition({ pos.x + offset.x, pos.y + offset.y, pos.z + offset.z });
    }
    position_ = pos;
}

// 一括自動発生設定
void YEmitterGroup::SetAutoEmitAll(bool autoEmit) {
    for (auto& emitter : emitters_) {
        emitter->SetAutoEmit(autoEmit);
    }
}

// アクセサ群
const std::string& YEmitterGroup::GetName() const { return groupName_; }
void YEmitterGroup::SetName(const std::string& name) { groupName_ = name; }
bool YEmitterGroup::IsActive() const { return isActive_; }
void YEmitterGroup::SetActive(bool active) {
    isActive_ = active;
    for (auto& e : emitters_) e->SetActive(active);
}
const Vector3& YEmitterGroup::GetPosition() const { return position_; }
const std::vector<std::unique_ptr<YParticleEmitter>>& YEmitterGroup::GetEmitters() const {
    return emitters_;
}

// JSON保存
nlohmann::json YEmitterGroup::SaveToJson() const {
    nlohmann::json j;
    j["groupName"] = groupName_;
    j["isActive"] = isActive_;
    j["position"] = { position_.x, position_.y, position_.z };
    j["emitters"] = nlohmann::json::array();

    for (const auto& emitter : emitters_) {
        nlohmann::json ej;
        ej["systemName"] = emitter->GetSystemName();
        const Vector3& pos = emitter->GetPosition();
        ej["offsetX"] = pos.x - position_.x;
        ej["offsetY"] = pos.y - position_.y;
        ej["offsetZ"] = pos.z - position_.z;
        ej["emissionRate"] = emitter->GetEmissionRate();
        ej["emitCount"] = emitter->GetEmitCount();
        ej["autoEmit"] = false; // ロード時に自動射出しないよう常にfalseで保存

        // 形状（min/max フィールドに対応）
        if (auto* shape = emitter->GetShape()) {
            ej["shapeType"] = static_cast<int>(shape->GetType());
            switch (shape->GetType()) {
            case YEmitterShape::Type::Sphere: {
                auto* s = static_cast<YEmitterSphere*>(shape);
                ej["shapeMinRadius"]  = s->minRadius;
                ej["shapeMaxRadius"]  = s->maxRadius;
                ej["shapeShellOnly"]  = s->shellOnly;
                // 後方互換のため旧フィールドも保存
                ej["shapeRadius"]     = s->maxRadius;
                break;
            }
            case YEmitterShape::Type::Box: {
                auto* b = static_cast<YEmitterBox*>(shape);
                ej["shapeMinSize"] = { b->minSize.x, b->minSize.y, b->minSize.z };
                ej["shapeMaxSize"] = { b->maxSize.x, b->maxSize.y, b->maxSize.z };
                // 後方互換
                ej["shapeSize"]    = { b->maxSize.x, b->maxSize.y, b->maxSize.z };
                break;
            }
            case YEmitterShape::Type::Cone: {
                auto* c = static_cast<YEmitterCone*>(shape);
                ej["shapeInnerAngle"] = c->innerAngle;
                ej["shapeOuterAngle"] = c->outerAngle;
                ej["shapeMinRadius"]  = c->minRadius;
                ej["shapeMaxRadius"]  = c->maxRadius;
                ej["shapeHeight"]     = c->height;
                ej["shapeDir"]        = { c->direction.x, c->direction.y, c->direction.z };
                break;
            }
            default: break;
            }
        }
        j["emitters"].push_back(ej);
    }
    return j;
}

// JSON読み込み
void YEmitterGroup::LoadFromJson(const nlohmann::json& j) {
    if (j.contains("groupName")) groupName_ = j["groupName"];
    if (j.contains("isActive"))  isActive_ = j["isActive"];
    if (j.contains("position")) {
        position_ = { j["position"][0], j["position"][1], j["position"][2] };
    }

    emitters_.clear();
    if (!j.contains("emitters")) return;

    for (const auto& ej : j["emitters"]) {
        std::string sysName = ej.value("systemName", "");
        Vector3 offset = {
            (float)ej.value("offsetX", 0.0),
            (float)ej.value("offsetY", 0.0),
            (float)ej.value("offsetZ", 0.0)
        };
        auto& emitter = AddEmitter(sysName, offset);

        emitter.SetEmissionRate((float)ej.value("emissionRate", 10.0));
        emitter.SetEmitCount(ej.value("emitCount", 1));
        emitter.SetAutoEmit(ej.value("autoEmit", false));

        // 形状復元（min/max 対応 + 後方互換）
        int shapeType = ej.value("shapeType", 0);
        switch (shapeType) {
        case static_cast<int>(YEmitterShape::Type::Point):
            emitter.SetShapePoint();
            break;
        case static_cast<int>(YEmitterShape::Type::Sphere): {
            // 新形式（minRadius/maxRadius）を優先、なければ旧形式にフォールバック
            if (ej.contains("shapeMinRadius") || ej.contains("shapeMaxRadius")) {
                float minR = (float)ej.value("shapeMinRadius", 0.0);
                float maxR = (float)ej.value("shapeMaxRadius", 1.0);
                emitter.SetShapeSphereRange(minR, maxR);
            } else {
                float r    = (float)ej.value("shapeRadius", 1.0);
                bool shell = ej.value("shapeShellOnly", false);
                emitter.SetShapeSphere(r, shell);
            }
            break;
        }
        case static_cast<int>(YEmitterShape::Type::Box): {
            if (ej.contains("shapeMinSize") || ej.contains("shapeMaxSize")) {
                Vector3 minSz = { 0,0,0 }, maxSz = { 1,1,1 };
                if (ej.contains("shapeMinSize")) {
                    minSz = { ej["shapeMinSize"][0], ej["shapeMinSize"][1], ej["shapeMinSize"][2] };
                }
                if (ej.contains("shapeMaxSize")) {
                    maxSz = { ej["shapeMaxSize"][0], ej["shapeMaxSize"][1], ej["shapeMaxSize"][2] };
                }
                emitter.SetShapeBoxRange(minSz, maxSz);
            } else {
                Vector3 sz = { 1,1,1 };
                if (ej.contains("shapeSize")) {
                    sz = { ej["shapeSize"][0], ej["shapeSize"][1], ej["shapeSize"][2] };
                }
                emitter.SetShapeBox(sz);
            }
            break;
        }
        case static_cast<int>(YEmitterShape::Type::Cone): {
            float innerAngle = (float)ej.value("shapeInnerAngle", 0.0);
            float outerAngle = (float)ej.value("shapeOuterAngle", 25.0);
            float minR       = (float)ej.value("shapeMinRadius",  0.0);
            float maxR       = (float)ej.value("shapeMaxRadius",  1.0);
            float h          = (float)ej.value("shapeHeight",     2.0);
            Vector3 dir      = { 0,1,0 };
            if (ej.contains("shapeDir")) {
                dir = { ej["shapeDir"][0], ej["shapeDir"][1], ej["shapeDir"][2] };
            }
            emitter.SetShapeConeRange(innerAngle, outerAngle, h, minR, maxR, dir);
            break;
        }
        default: break;
        }
    }
}