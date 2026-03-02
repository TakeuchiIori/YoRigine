#include "AreaManager.h"


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

void AreaManager::Update(const Vector3& targetPosition)
{
	for (auto& [name, area] : areas_) {
		if (area && area->IsActive()) {
			area->Update(targetPosition);
		}
	}
}

void AreaManager::Draw(Line* line)
{
	if (!isDebugDrawEnabled_ || !line) {
		return;
	}

	for (auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->IsDebugDrawEnabled()) {
			area->Draw(line);
		}
	}
}

void AreaManager::Reset()
{
	areas_.clear();
}

void AreaManager::AddArea(const std::string& name, std::shared_ptr<BaseArea> area)
{
	if (!area) {
		return;
	}

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
	std::shared_ptr<BaseArea> nearestArea = nullptr;
	float minDistance = FLT_MAX;

	// 最も近いエリアを見つける
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive()) {
			Vector3 center = area->GetCenter();
			float distance = Length(position - center);

			if (distance < minDistance) {
				minDistance = distance;
				nearestArea = area;
			}
		}
	}

	// 最も近いエリアにクランプ
	if (nearestArea) {
		return nearestArea->ClampPosition(position);
	}

	// エリアが見つからない場合は元の位置を返す
	return position;
}
///************************* オブジェクト登録管理 *************************///

/// <summary>
/// オブジェクトをエリア制限対象として登録
/// </summary>
/// <param name="wt">登録するワールドトランスフォーム</param>
/// <param name="tag">識別用タグ(デバッグ用)</param>
void AreaManager::RegisterObject(WorldTransform* wt, const std::string& tag)
{
	if (!wt) {
		return;
	}

	// 既に登録されていないかチェック
	for (const auto& obj : restrictedObjects_) {
		if (obj.worldTransform == wt) {
			return; // 既に登録済み
		}
	}

	// 新規登録
	restrictedObjects_.emplace_back(wt, tag);
}

/// <summary>
/// オブジェクトの登録を解除
/// </summary>
/// <param name="wt">解除するワールドトランスフォーム</param>
void AreaManager::UnregisterObject(WorldTransform* wt)
{
	if (!wt) {
		return;
	}

	// 該当するオブジェクトを削除
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
	// Boundary用途のエリアが存在するかチェック
	bool hasBoundaryArea = false;
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->GetPurpose() == AreaPurpose::Boundary) {
			hasBoundaryArea = true;
			break;
		}
	}

	// Boundaryエリアがなければ何もしない
	if (!hasBoundaryArea) {
		return;
	}

	// 現在の計算済み行列からワールド座標を抽出（4行目）
	Vector3 currentPos = {
		wt->matWorld_.m[3][0],
		wt->matWorld_.m[3][1],
		wt->matWorld_.m[3][2]
	};
	// エリア内かチェック
	if (!IsInsideAreaByPurpose(currentPos, AreaPurpose::Boundary)) {
		// エリア外なら境界内にクランプ
		Vector3 clampedPos = ClampToNearestArea(currentPos);
		wt->translate_ = clampedPos;
		// 行列の 4行目 (m[3][0]〜[3][2]) が平行移動成分
		wt->matWorld_.m[3][0] = clampedPos.x;
		wt->matWorld_.m[3][1] = clampedPos.y;
		wt->matWorld_.m[3][2] = clampedPos.z;
	}


}

/// <summary>
/// 登録されているすべてのオブジェクトの位置をエリア内に補正
/// 毎フレーム呼ぶことでエリア外に出られないようにする
/// </summary>
void AreaManager::UpdateRestrictedObjects()
{
	// Boundary用途のエリアが存在するかチェック
	bool hasBoundaryArea = false;
	for (const auto& [name, area] : areas_) {
		if (area && area->IsActive() && area->GetPurpose() == AreaPurpose::Boundary) {
			hasBoundaryArea = true;
			break;
		}
	}

	// Boundaryエリアがなければ何もしない
	if (!hasBoundaryArea) {
		return;
	}

	// 登録されているオブジェクトをチェック&補正
	for (auto& obj : restrictedObjects_) {
		// 無効化されているオブジェクトはスキップ
		if (!obj.enabled || !obj.worldTransform) {
			continue;
		}

		Vector3 currentPos = obj.worldTransform->translate_;

		// エリア内かチェック
		if (!IsInsideAreaByPurpose(currentPos, AreaPurpose::Boundary)) {
			// エリア外なら境界内にクランプ
			Vector3 clampedPos = ClampToNearestArea(currentPos);
			obj.worldTransform->translate_ = clampedPos;
		}
	}
}

/// <summary>
/// 特定のオブジェクトの制限を有効/無効化
/// </summary>
/// <param name="wt">対象のワールドトランスフォーム</param>
/// <param name="enabled">有効化するか</param>
void AreaManager::SetObjectRestrictionEnabled(WorldTransform* wt, bool enabled)
{
	if (!wt) {
		return;
	}

	for (auto& obj : restrictedObjects_) {
		if (obj.worldTransform == wt) {
			obj.enabled = enabled;
			return;
		}
	}
}

/// <summary>
/// すべてのオブジェクトをクリア
/// </summary>
void AreaManager::ClearAllObjects()
{
	restrictedObjects_.clear();
}