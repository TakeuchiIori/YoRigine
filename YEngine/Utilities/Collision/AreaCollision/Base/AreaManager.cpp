#include "AreaManager.h"
#include "../CircleArea.h"
#include "../PolygonArea.h"
#include <cfloat>
#include <cmath>
#include <algorithm>

// ============================================================
// コンストラクタ（indexAj_ の登録は一度だけ行う）
// ============================================================
AreaManager::AreaManager()
{
	indexAj_.Add("names",    &savedNames_)
	         .Add("types",    &savedTypes_)
	         .Add("purposes", &savedPurposes_);
}

AreaManager* AreaManager::GetInstance()
{
	static AreaManager instance;
	return &instance;
}

void AreaManager::Initialize()
{
	areas_.clear();
	restrictedObjects_.clear();
	isDebugDrawEnabled_ = false;
}

void AreaManager::Update(const Vector3& targetPosition, void* targetKey, float deltaTime)
{
	for (auto& [name, area] : areas_) {
		if (area && area->IsActive()) {
			area->Update(targetPosition, targetKey, deltaTime);
		}
	}
}

void AreaManager::UpdateTargets(const std::vector<AreaTarget>& targets, float deltaTime)
{
	for (auto& [name, area] : areas_) {
		if (!area || !area->IsActive()) continue;
		for (const auto& t : targets) {
			area->Update(t.position, t.key, deltaTime);
		}
	}
}

void AreaManager::ForgetTarget(void* targetKey)
{
	for (auto& [name, area] : areas_) {
		if (area) area->ForgetTarget(targetKey);
	}
}

void AreaManager::Draw(Line* line,
	const std::initializer_list<std::string>& excludes)
{
	if (!isDebugDrawEnabled_ || !line) return;

	for (auto& [name, area] : areas_) {
		// 除外リストに含まれていたらスキップ
		bool excluded = false;
		for (const auto& ex : excludes) {
			if (name == ex) { excluded = true; break; }
		}
		if (excluded) continue;

		if (area && area->IsActive() && area->IsDebugDrawEnabled()) {
			area->Draw(line);
		}
	}
}

void AreaManager::DrawArea(const std::string& name, Line* line)
{
	auto area = GetArea(name);
	if (area) {
		area->Draw(line);
	}
}

void AreaManager::Reset()
{
	areas_.clear();
}

void AreaManager::AddArea(const std::string& name, std::shared_ptr<BaseArea> area)
{
	if (!area) return;
	areas_[name] = area;
}

void AreaManager::RemoveArea(const std::string& name)
{
	auto it = areas_.find(name);
	if (it != areas_.end()) {
		areas_.erase(it);
	}
}

std::shared_ptr<BaseArea> AreaManager::GetArea(const std::string& name)
{
	auto it = areas_.find(name);
	if (it != areas_.end()) {
		return it->second;
	}
	return nullptr;
}

void AreaManager::SetAreaActive(const std::string& name, bool active)
{
	auto area = GetArea(name);
	if (area) {
		area->SetActive(active);
	}
}

void AreaManager::SetAllAreasActive(bool active)
{
	for (auto& [name, area] : areas_) {
		if (area) {
			area->SetActive(active);
		}
	}
}

bool AreaManager::IsInsideAnyArea(const Vector3& position) const
{
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->IsInside(position)) {
			return true;
		}
	}
	return false;
}

bool AreaManager::IsInsideAreaByPurpose(const Vector3& position, AreaPurpose purpose) const
{
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() &&
			area->GetPurpose() == purpose &&
			area->IsInside(position)) {
			return true;
		}
	}
	return false;
}

Vector3 AreaManager::ClampToNearestArea(const Vector3& position) const
{
	// 1) いずれかのエリアの内側にいるなら、そのまま返す。
	//    旧実装は「中心距離が最も近いエリア」にクランプしていたため、
	//    別エリアの内側にいる位置が遠いエリア境界に飛ぶバグがあった。
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->IsInside(position)) {
			return position;
		}
	}

	// 2) 外側にいる場合は「境界が最も近いエリア」を選んでクランプする。
	//    GetDistanceFromBoundary は外側で負、絶対値が境界までの距離。
	std::shared_ptr<BaseArea> nearestArea = nullptr;
	float minAbs = FLT_MAX;

	for (const auto& [name, area] : areas_) {
		if (!area || !area->IsActive()) continue;
		float d = std::fabs(area->GetDistanceFromBoundary(position));
		if (d < minAbs) {
			minAbs = d;
			nearestArea = area;
		}
	}

	if (nearestArea) {
		return nearestArea->ClampPosition(position);
	}
	return position;
}

///************************* JSON 保存・読み込み *************************///

// ============================================================
// タイプ文字列からエリアを生成するファクトリ
// 新しい形状を追加する場合はここに1行追加する
// ============================================================
std::shared_ptr<BaseArea> AreaManager::CreateArea(const std::string& typeString)
{
	if (typeString == "Circle")  return std::make_shared<CircleArea>();
	if (typeString == "Polygon") return std::make_shared<PolygonArea>();
	return nullptr;
}

// ============================================================
// 全エリアを1ファイルに保存
// ============================================================
void AreaManager::SaveAllToFile(const std::string& filepath)
{
	// インデックスを現在の areas_ から再構築
	savedNames_.clear();
	savedTypes_.clear();
	savedPurposes_.clear();

	for (const auto& [name, area] : areas_) {
		savedNames_.push_back(name);
		savedTypes_.push_back(area->GetTypeString());
		savedPurposes_.push_back(
			area->GetPurpose() == AreaPurpose::Boundary ? "Boundary" : "Trigger");
	}

	// root AutoJson にすべてをグループとして登録して一括保存
	AutoJson root;
	root.AddGroup("_index", indexAj_);

	for (auto& [name, area] : areas_) {
		root.AddGroup(name, area->GetAutoJson());
	}

	root.SaveToFile(filepath);
}

// ============================================================
// ファイルから全エリアを復元（2パス方式）
//
// パス1: _index だけ読んで名前・タイプ・Purpose を取得
// パス2: エリアを生成してから各パラメータを読み込む
// ============================================================
void AreaManager::LoadAllFromFile(const std::string& filepath)
{
	// ── パス1：_index のみ読み込む ──────────────────
	{
		AutoJson pass1;
		pass1.AddGroup("_index", indexAj_);
		pass1.LoadFromFile(filepath);   // _index 以外のキーは無視される
	}

	// インデックスを元にエリアを生成
	areas_.clear();
	for (size_t i = 0; i < savedNames_.size(); ++i) {
		auto area = CreateArea(savedTypes_[i]);
		if (!area) continue;

		area->SetPurpose(savedPurposes_[i] == "Trigger"
			? AreaPurpose::Trigger : AreaPurpose::Boundary);

		areas_[savedNames_[i]] = area;
	}

	// ── パス2：各エリアのパラメータを読み込む ──────
	{
		AutoJson pass2;
		pass2.AddGroup("_index", indexAj_); // 存在しても上書きされるだけで無害

		for (auto& [name, area] : areas_) {
			pass2.AddGroup(name, area->GetAutoJson());
		}
		pass2.LoadFromFile(filepath);
	}
}

///************************* オブジェクト登録管理 *************************///

void AreaManager::RegisterObject(WorldTransform* wt, const std::string& tag)
{
	if (!wt) return;

	for (const auto& obj : restrictedObjects_) {
		if (obj.worldTransform == wt) return; // 既に登録済み
	}

	restrictedObjects_.emplace_back(wt, tag);
}

void AreaManager::UnregisterObject(WorldTransform* wt)
{
	if (!wt) return;

	restrictedObjects_.erase(
		std::remove_if(
			restrictedObjects_.begin(),
			restrictedObjects_.end(),
			[wt](const RestrictedObject& obj) {
				return obj.worldTransform == wt;
			}
		),
		restrictedObjects_.end()
	);
}

void AreaManager::UpdateSingleObject(WorldTransform* wt)
{
	bool hasBoundaryArea = false;
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->GetPurpose() == AreaPurpose::Boundary) {
			hasBoundaryArea = true;
			break;
		}
	}

	if (!hasBoundaryArea) return;

	Vector3 currentPos = {
		wt->matWorld_.m[3][0],
		wt->matWorld_.m[3][1],
		wt->matWorld_.m[3][2]
	};

	if (!IsInsideAreaByPurpose(currentPos, AreaPurpose::Boundary)) {
		Vector3 clampedPos = ClampToNearestArea(currentPos);
		wt->translate_ = clampedPos;
		wt->matWorld_.m[3][0] = clampedPos.x;
		wt->matWorld_.m[3][1] = clampedPos.y;
		wt->matWorld_.m[3][2] = clampedPos.z;
	}
}

void AreaManager::UpdateRestrictedObjects()
{
	bool hasBoundaryArea = false;
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->GetPurpose() == AreaPurpose::Boundary) {
			hasBoundaryArea = true;
			break;
		}
	}

	if (!hasBoundaryArea) return;

	for (auto& obj : restrictedObjects_) {
		if (!obj.enabled || !obj.worldTransform) continue;

		Vector3 currentPos = obj.worldTransform->translate_;

		if (!IsInsideAreaByPurpose(currentPos, AreaPurpose::Boundary)) {
			Vector3 clampedPos = ClampToNearestArea(currentPos);
			obj.worldTransform->translate_ = clampedPos;
		}
	}
}

void AreaManager::SetObjectRestrictionEnabled(WorldTransform* wt, bool enabled)
{
	if (!wt) return;

	for (auto& obj : restrictedObjects_) {
		if (obj.worldTransform == wt) {
			obj.enabled = enabled;
			return;
		}
	}
}

void AreaManager::ClearAllObjects()
{
	restrictedObjects_.clear();
}
